#include "GroundControl.h"
#include "EnvelopeJson.h"
#include "TelemetryReadouts.h"
#include <iostream>

using namespace std;

GroundControl::GroundControl(QObject *parent)
    : QObject(parent),
      telemetryReceiver_(SharedTypes::groundTelemetryPort),
      godStatusReceiver_(SharedTypes::godStatusPort)
{
    PopulateRows();
    connect(&telemetryReceiver_, &UdpReceiver::DatagramReceived, this, &GroundControl::HandleGroundTelemetry);
    connect(&godStatusReceiver_, &UdpReceiver::DatagramReceived, this, &GroundControl::HandleGodStatus);

    connect(&linkCheckTimer_, &QTimer::timeout, this, &GroundControl::UpdateLinkRow);
    linkCheckTimer_.start(linkCheckIntervalMilliseconds);

    faultsModel_.SetRows({{"Temperature Sensor Fault", "Off", static_cast<int>(SharedTypes::Status::none)},
                          {"Power Sensor Fault", "Off", static_cast<int>(SharedTypes::Status::none)},
                          {"Attitude Sensor Fault", "Off", static_cast<int>(SharedTypes::Status::none)},
                          {"Chaos Mode", "Off", static_cast<int>(SharedTypes::Status::none)}});
    connect(&godSender_, &AckUdpSender::Acknowledged, this, &GroundControl::HandleFaultAck);
    connect(&godSender_, &AckUdpSender::GaveUp, this, &GroundControl::HandleFaultGaveUp);

    adjustsModel_.SetRows({{"Battery", "Ready", static_cast<int>(SharedTypes::Status::none)},
                           {"Time Scale", "Ready", static_cast<int>(SharedTypes::Status::none)}});
    connect(&godSender_, &AckUdpSender::Acknowledged, this, &GroundControl::HandleAdjustAck);
    connect(&godSender_, &AckUdpSender::GaveUp, this, &GroundControl::HandleAdjustGaveUp);

    commandsModel_.SetRows({{"Reboot Flight Computer", "Ready", static_cast<int>(SharedTypes::Status::none)},
                            {"Ping", "Ready", static_cast<int>(SharedTypes::Status::none)}});
    connect(&groundSender_, &AckUdpSender::Acknowledged, this, &GroundControl::HandleCommandAck);
    connect(&groundSender_, &AckUdpSender::GaveUp, this, &GroundControl::HandleCommandGaveUp);

    commandsSwitchesModel_.SetRows({{"Safe Mode", "Off", static_cast<int>(SharedTypes::Status::none)},
                                    {"Payload Inhibit", "Off", static_cast<int>(SharedTypes::Status::none)}});
    connect(&groundSender_, &AckUdpSender::Acknowledged, this, &GroundControl::HandleCommandSwitchAck);
    connect(&groundSender_, &AckUdpSender::GaveUp, this, &GroundControl::HandleCommandSwitchGaveUp);

    inhibitsModel_.SetRows({{"Temperature Sensor Inhibit", "Off", static_cast<int>(SharedTypes::Status::none)},
                            {"Power Sensor Inhibit", "Off", static_cast<int>(SharedTypes::Status::none)},
                            {"Attitude Sensor Inhibit", "Off", static_cast<int>(SharedTypes::Status::none)}});
    connect(&inhibitSender_, &AckUdpSender::Acknowledged, this, &GroundControl::HandleInhibitAck);
    connect(&inhibitSender_, &AckUdpSender::GaveUp, this, &GroundControl::HandleInhibitGaveUp);

    connect(&discovery_, &Discovery::peerAppeared, this, [this](const QString &appName, const QHostAddress &)
            {
                if(appName == SharedTypes::simulatorName)
                {
                    simulatorDiscovered_ = true;
                    cout << "Simulator discovered" << endl;
                    if ( lastMETSeconds_ < 0.0)
                        SyncMissionClock(0.0, clockTimeScale_);
                    for(int row = 0; row < faultRowCount; ++row)
                        faultsModel_.UpdateRow(row, "Off", static_cast<int> (SharedTypes::Status::none)); 
                    
                    emit summaryChanged();
                }

                if(appName == SharedTypes::flightComputerName)
                        ResetInhibitRows(); });
    connect(&discovery_, &Discovery::peerDisappeared, this, [this](const QString &appName, const QHostAddress &)
            {
                if(appName == SharedTypes::simulatorName)
                {
                    simulatorDiscovered_ = false;
                    cout << "Simulator lost" << endl;
                    emit summaryChanged();
                } });
}

void GroundControl::PopulateRows()
{
    readoutsModel_.SetRows(QVector<ReadoutRow>{
                               {"Flight Computer Link", "", 0},
                               {"FC->Sim Link", "", 0},
                               {"Mode", "", 0},
                           } +
                           TelemetryReadouts());
}

void GroundControl::HandleGroundTelemetry(const QByteArray &payload)
{
    std::optional<Envelope> envelope = EnvelopeFromJson(payload);
    if (!envelope || envelope->type != SharedTypes::groundTelemetryMessageType)
    {
        qWarning() << "Dropped malformed ground telemetry packet";
        return;
    }

    std::optional<SharedTypes::Telemetry> telemetry = TelemetryFromJsonObject(envelope->body["telemetry"].toObject());
    const int modeNumber = envelope->body["mode"].toInteger(-1);

    if (!telemetry || modeNumber < 0 || modeNumber > static_cast<int>(SharedTypes::Mode::safe))
    {
        qWarning() << "Dropped malformed ground telemetry packet";
        return;
    }

    telemetry_ = *telemetry;
    mode_ = static_cast<SharedTypes::Mode>(modeNumber);
    simLinkOk_ = envelope->body["simLinkOk"].toBool();
    SyncMissionClock(telemetry_.missionElapsedTimeSeconds, telemetry_.timeScale);
    lastContactMETSeconds_ = telemetry_.missionElapsedTimeSeconds;

    timeSinceLastPacket_.restart();
    UpdateRows();
    emit summaryChanged();
}

void GroundControl::HandleGodStatus(const QByteArray &payload)
{
    std::optional<Envelope> envelope = EnvelopeFromJson(payload);
    if (!envelope || envelope->type != SharedTypes::godStatusMessageType || !HasAllKeys(envelope->body, SharedTypes::requiredGodStatusKeys))
    {
        qWarning() << "Dropped malformed god status packet";
        return;
    }

    const float timeScale = static_cast<float>(envelope->body["timeScale"].toDouble());
    const float batteryPercent = static_cast<float>(envelope->body["batteryPercent"].toDouble());

    SetClockTimeScale(timeScale);
    adjustsModel_.UpdateRow(batteryAdjustRow, QString("%1 %").arg(batteryPercent, 0, 'f', 1), static_cast<int>(SharedTypes::Status::none));
    adjustsModel_.UpdateRow(timeScaleAdjustRow, QString("%1x").arg(timeScale), static_cast<int>(SharedTypes::Status::none));
    UpdateMissionClockRows();
}

void GroundControl::UpdateLinkRow()
{
    const bool neverHeard = !timeSinceLastPacket_.isValid();
    const qint64 silentMilliseconds = neverHeard ? 0 : timeSinceLastPacket_.elapsed();
    const bool linkLost = neverHeard || silentMilliseconds > SharedTypes::linkLostMilliseconds;
    const bool linkReturned = !flightComputerLinked_ && !linkLost;
    const bool linkDropped = flightComputerLinked_ && linkLost;
    flightComputerLinked_ = !linkLost;

    if (linkDropped)
        cout << "Flight computer link lost" << endl;
    if (linkReturned)
        cout << "Flight computer link restored" << endl;

    if (rebootInProgress_ && linkReturned)
    {
        rebootInProgress_ = false;
        cout << "Flight computer rebooted" << endl;
        commandsModel_.UpdateRow(rebootRow, "Rebooted", static_cast<int>(SharedTypes::Status::good));
        ResetInhibitRows();
    }

    readoutsModel_.UpdateRow(flightComputerLinkRow,
                             neverHeard ? "No Link" : linkLost ? QString("No Link (%1 s)").arg(silentMilliseconds / 1000.0, 0, 'f', 1)
                                                               : "Good Link",
                             static_cast<int>(linkLost ? SharedTypes::Status::critical : SharedTypes::Status::good));

    if (linkLost)
        UpdateRows(true);

    UpdateMissionClockRows();
    emit summaryChanged();
}

void GroundControl::UpdateRows(bool stale)
{
    auto rowStatus = [stale](SharedTypes::Status status)
    { return static_cast<int>(stale ? SharedTypes::Status::stale : status); };

    readoutsModel_.UpdateRow(simulatorLinkRow, simLinkOk_ ? "Good Link" : "No Link", rowStatus(simLinkOk_ ? SharedTypes::Status::good : SharedTypes::Status::critical));
    readoutsModel_.UpdateRow(modeRow, ModeText(mode_), rowStatus(GetModeStatus(mode_)));
    UpdateTelemetryReadouts(readoutsModel_, telemetry_, gcHeaderRowCount, stale);
    UpdateSafeModeSwitchRow(stale);
}

void GroundControl::SyncMissionClock(double METSeconds, float timeScale)
{
    lastMETSeconds_ = METSeconds;
    clockTimeScale_ = timeScale;
    timeSinceMETSync_.restart();
}

void GroundControl::SetClockTimeScale(float timeScale)
{
    if (lastMETSeconds_ >= 0.0)
    {
        lastMETSeconds_ = GetEstimatedMETSeconds();
        timeSinceMETSync_.restart();
    }

    clockTimeScale_ = timeScale;
}

double GroundControl::GetEstimatedMETSeconds() const
{
    return lastMETSeconds_ + timeSinceMETSync_.elapsed() / 1000.0 * clockTimeScale_;
}

void GroundControl::UpdateMissionClockRows()
{
    if (lastMETSeconds_ < 0.0 || flightComputerLinked_)
        return;

    const double estimatedMETSeconds = GetEstimatedMETSeconds();
    const double secondsOverdue = SecondsContactOverdue(estimatedMETSeconds, lastContactMETSeconds_);

    UpdateMissionClockReadouts(readoutsModel_, estimatedMETSeconds, gcHeaderRowCount, false);

    if(secondsOverdue > 0.0)
        readoutsModel_.UpdateRow(static_cast<int>(gcHeaderRowCount) + nextContactRow, CountdownText(static_cast<float>(-secondsOverdue)), static_cast<int>(SharedTypes::Status::critical));
}

QString GroundControl::FaultName(int faultRow)
{
    switch (faultRow)
    {
    case temperatureSensorFaultRow:
        return SharedTypes::temperatureSensorFaultMessage;
    case powerSensorFaultRow:
        return SharedTypes::powerSensorFaultMessage;
    case attitudeSensorFaultRow:
        return SharedTypes::attitudeSensorFaultMessage;
    case chaosRow:
        return SharedTypes::chaosFaultMessage;
    default:
        return QString();
    }
}

void GroundControl::SetFault(int faultRow, bool active)
{
    const QString faultName = FaultName(faultRow);
    if (faultName.isEmpty())
    {
        qWarning() << "Unknown fault row" << faultRow;
        return;
    }

    const QList<QHostAddress> simulators = discovery_.LivePeerAddresses(SharedTypes::simulatorName);

    if (simulators.isEmpty())
    {
        faultsModel_.UpdateRow(faultRow, "No Simulator", static_cast<int>(SharedTypes::Status::critical));
        cout << "No simulator to send fault " << faultName.toStdString() << " to" << endl;
        return;
    }

    QJsonObject body;
    body["fault"] = faultName;
    body["active"] = active;

    const qint64 sequence = godSender_.SendAck(SharedTypes::faultInjectionMessageType, body, simulators.first(), SharedTypes::godPort);
    pendingFaults_[sequence] = {faultRow, active};
    faultsModel_.UpdateRow(faultRow, active ? "Turning On..." : "Turning Off...", static_cast<int>(SharedTypes::Status::warning));
    cout << "Sent fault " << faultName.toStdString() << (active ? " on" : " off") << endl;
}

void GroundControl::SendCommand(int commandRow)
{
    const QString commandName = CommandName(commandRow);
    if (commandName.isEmpty())
    {
        qWarning() << "Unknown command row" << commandRow;
        return;
    }

    const QList<QHostAddress> flightComputers = discovery_.LivePeerAddresses(SharedTypes::flightComputerName);
    if (flightComputers.isEmpty())
    {
        commandsModel_.UpdateRow(commandRow, "No Flight Computer", static_cast<int>(SharedTypes::Status::critical));
        cout << "No flight computer to send command " << commandName.toStdString() << " to" << endl;
        return;
    }

    QJsonObject body;
    body["command"] = commandName;

    const qint64 sequence = groundSender_.SendAck(SharedTypes::groundCommandMessageType, body, flightComputers.first(), SharedTypes::groundCommandPort);
    pendingCommands_[sequence] = commandRow;
    commandsModel_.UpdateRow(commandRow, "Sending...", static_cast<int>(SharedTypes::Status::warning));
    cout << "Sent command " << commandName.toStdString() << endl;
}

void GroundControl::SetCommandSwitch(int switchRow, bool on)
{
    const QString commandName = CommandSwitchName(switchRow, on);
    if (commandName.isEmpty())
    {
        qWarning() << "Unknown command switch row" << switchRow;
        return;
    }

    const QList<QHostAddress> flightComputers = discovery_.LivePeerAddresses(SharedTypes::flightComputerName);
    if (flightComputers.isEmpty())
    {
        commandsSwitchesModel_.UpdateRow(switchRow, "No Flight Computer", static_cast<int>(SharedTypes::Status::critical));
        cout << "No flight computer to send " << commandName.toStdString() << " to" << endl;
        return;
    }

    QJsonObject body;
    body["command"] = commandName;
    if (commandName == SharedTypes::inhibitPayloadCommand)
        body["inhibited"] = on;

    switchOutcomeSuffix_.remove(switchRow);
    const qint64 sequence = groundSender_.SendAck(SharedTypes::groundCommandMessageType, body, flightComputers.first(), SharedTypes::groundCommandPort);
    pendingCommandSwitches_[sequence] = {switchRow, on};
    commandsSwitchesModel_.UpdateRow(switchRow, on ? "Turning On..." : "Turning Off...", static_cast<int>(SharedTypes::Status::warning));
    cout << "Sent " << commandName.toStdString() << endl;
}

void GroundControl::SetInhibits(int faultRow, bool inhibited)
{
    const QString faultName = FaultName(faultRow);
    if (faultName.isEmpty())
    {
        qWarning() << "Unknown inhibit row" << faultRow;
        return;
    }

    const QList<QHostAddress> flightComputers = discovery_.LivePeerAddresses(SharedTypes::flightComputerName);
    if (flightComputers.isEmpty())
    {
        inhibitsModel_.UpdateRow(faultRow, "No Flight Computer", static_cast<int>(SharedTypes::Status::critical));
        cout << "No flight computer to send inhibit " << faultName.toStdString() << " to" << endl;
        return;
    }

    QJsonObject body;
    body["command"] = SharedTypes::inhibitFaultCommand;
    body["fault"] = faultName;
    body["inhibited"] = inhibited;

    const qint64 sequence = inhibitSender_.SendAck(SharedTypes::groundCommandMessageType, body, flightComputers.first(), SharedTypes::groundCommandPort);
    pendingInhibits_[sequence] = {faultRow, inhibited};
    inhibitsModel_.UpdateRow(faultRow, inhibited ? "Turning On..." : "Turning Off...", static_cast<int>(SharedTypes::Status::warning));
    cout << "Sent inhibit " << faultName.toStdString() << (inhibited ? " on" : " off") << endl;
}

void GroundControl::HandleFaultAck(qint64 sequence, bool accepted)
{
    if (!pendingFaults_.contains(sequence))
        return;

    const PendingFault fault = pendingFaults_.take(sequence);
    cout << "Fault " << FaultName(fault.row).toStdString() << (fault.active ? " on" : " off") << (accepted ? " accepted" : " rejected") << endl;

    if (accepted)
        faultsModel_.UpdateRow(fault.row, fault.active ? "On" : "Off", static_cast<int>(fault.active ? SharedTypes::Status::critical : SharedTypes::Status::none));
    else
        faultsModel_.UpdateRow(fault.row, "Rejected", static_cast<int>(SharedTypes::Status::critical));
}

void GroundControl::HandleFaultGaveUp(qint64 sequence)
{
    if (!pendingFaults_.contains(sequence))
        return;

    const PendingFault fault = pendingFaults_.take(sequence);
    cout << "Fault " << FaultName(fault.row).toStdString() << ": no response" << endl;
    faultsModel_.UpdateRow(fault.row, "No Response", static_cast<int>(SharedTypes::Status::critical));
}

void GroundControl::SendAdjust(int adjustRow, bool increase)
{
    const QString faultName = AdjustName(adjustRow, increase);
    if (faultName.isEmpty())
    {
        qWarning() << "Unknown adjust row" << adjustRow;
        return;
    }

    const QList<QHostAddress> simulators = discovery_.LivePeerAddresses(SharedTypes::simulatorName);

    if (simulators.isEmpty())
    {
        adjustsModel_.UpdateRow(adjustRow, "No Simulator", static_cast<int>(SharedTypes::Status::critical));
        cout << "No simulator to send " << faultName.toStdString() << " to" << endl;
        return;
    }

    QJsonObject body;
    body["fault"] = faultName;
    body["active"] = true;

    const qint64 sequence = godSender_.SendAck(SharedTypes::faultInjectionMessageType, body, simulators.first(), SharedTypes::godPort);
    pendingAdjusts_[sequence] = {adjustRow, faultName};
    adjustsModel_.UpdateRow(adjustRow, "Sending...", static_cast<int>(SharedTypes::Status::warning));
    cout << "Sent " << faultName.toStdString() << endl;
}

QString GroundControl::CommandName(int commandRow)
{
    switch (commandRow)
    {
    case rebootRow:
        return SharedTypes::rebootCommand;
    case pingRow:
        return SharedTypes::pingCommand;
    default:
        return QString();
    }
}

void GroundControl::HandleCommandAck(qint64 sequence, bool accepted)
{
    if (!pendingCommands_.contains(sequence))
        return;

    const int commandRow = pendingCommands_.take(sequence);
    cout << "Command " << CommandName(commandRow).toStdString() << (accepted ? " accepted" : " rejected") << endl;

    if (accepted && commandRow == rebootRow)
    {
        rebootInProgress_ = true;
        cout << "Flight computer rebooting..." << endl;
        commandsModel_.UpdateRow(commandRow, "Rebooting...", static_cast<int>(SharedTypes::Status::warning));
        return;
    }

    if (accepted && commandRow == pingRow)
    {
        commandsModel_.UpdateRow(commandRow, "Link OK", static_cast<int>(SharedTypes::Status::good));
        return;
    }

    commandsModel_.UpdateRow(commandRow, accepted ? "Accepted" : "Rejected", static_cast<int>(accepted ? SharedTypes::Status::good : SharedTypes::Status::critical));
}

void GroundControl::HandleCommandGaveUp(qint64 sequence)
{
    if (!pendingCommands_.contains(sequence))
        return;

    const int commandRow = pendingCommands_.take(sequence);
    cout << "Command " << CommandName(commandRow).toStdString() << ": no response" << endl;
    commandsModel_.UpdateRow(commandRow, "No Response", static_cast<int>(SharedTypes::Status::critical));
}

void GroundControl::HandleInhibitAck(qint64 sequence, bool accepted)
{
    if (!pendingInhibits_.contains(sequence))
        return;

    const PendingFault inhibit = pendingInhibits_.take(sequence);
    cout << "Inhibit " << FaultName(inhibit.row).toStdString() << (inhibit.active ? " on" : " off") << (accepted ? " accepted" : " rejected") << endl;

    if (accepted)
        inhibitsModel_.UpdateRow(inhibit.row, inhibit.active ? "On" : "Off", static_cast<int>(inhibit.active ? SharedTypes::Status::warning : SharedTypes::Status::none));
    else
        inhibitsModel_.UpdateRow(inhibit.row, "Rejected", static_cast<int>(SharedTypes::Status::critical));
}

void GroundControl::HandleInhibitGaveUp(qint64 sequence)
{
    if (!pendingInhibits_.contains(sequence))
        return;

    const PendingFault inhibit = pendingInhibits_.take(sequence);
    cout << "Inhibit " << FaultName(inhibit.row).toStdString() << ": no response" << endl;
    inhibitsModel_.UpdateRow(inhibit.row, "No Response", static_cast<int>(SharedTypes::Status::critical));
}

void GroundControl::ResetInhibitRows()
{
    for (int row = 0; row < inhibitsModel_.rowCount(); ++row)
        inhibitsModel_.UpdateRow(row, "Off", static_cast<int>(SharedTypes::Status::none));

    commandsSwitchesModel_.UpdateRow(payloadInhibitRow, "Off", static_cast<int>(SharedTypes::Status::none));
    switchOutcomeSuffix_.clear();
}

QString GroundControl::AdjustName(int adjustRow, bool increase)
{
    switch (adjustRow)
    {
    case batteryAdjustRow:
        return increase ? SharedTypes::batteryUpMessage : SharedTypes::batteryDownMessage;
    case timeScaleAdjustRow:
        return increase ? SharedTypes::timeScaleUpMessage : SharedTypes::timeScaleDownMessage;
    default:
        return QString();
    }
}

void GroundControl::HandleAdjustAck(qint64 sequence, bool accepted)
{
    if (!pendingAdjusts_.contains(sequence))
        return;

    const PendingAdjust adjust = pendingAdjusts_.take(sequence);
    cout << adjust.faultName.toStdString() << (accepted ? " accepted" : " rejected") << endl;
    adjustsModel_.UpdateRow(adjust.row, accepted ? "Accepted" : "Rejected", static_cast<int>(accepted ? SharedTypes::Status::good : SharedTypes::Status::critical));

    if (!accepted)
        adjustsModel_.UpdateRow(adjust.row, "Rejected", static_cast<int>(SharedTypes::Status::critical));
}

void GroundControl::HandleAdjustGaveUp(qint64 sequence)
{
    if (!pendingAdjusts_.contains(sequence))
        return;

    const PendingAdjust adjust = pendingAdjusts_.take(sequence);
    cout << adjust.faultName.toStdString() << ": no response" << endl;
    adjustsModel_.UpdateRow(adjust.row, "No Response", static_cast<int>(SharedTypes::Status::critical));
}

QString GroundControl::CommandSwitchName(int switchRow, bool on)
{
    switch (switchRow)
    {
    case safeModeRow:
        return on ? SharedTypes::forceSafeModeCommand : SharedTypes::exitSafeModeCommand;
    case payloadInhibitRow:
        return SharedTypes::inhibitPayloadCommand;
    default:
        return QString();
    }
}

void GroundControl::HandleCommandSwitchAck(qint64 sequence, bool accepted)
{
    if (!pendingCommandSwitches_.contains(sequence))
        return;

    const PendingFault commandSwitch = pendingCommandSwitches_.take(sequence);
    cout << CommandSwitchName(commandSwitch.row, commandSwitch.active).toStdString() << (accepted ? " accepted" : " rejected") << endl;

    if (accepted)
        commandsSwitchesModel_.UpdateRow(commandSwitch.row, commandSwitch.active ? "On" : "Off", static_cast<int>(commandSwitch.active ? SharedTypes::Status::warning : SharedTypes::Status::none));
    else
    {
        switchOutcomeSuffix_[commandSwitch.row] = " (Rejected)";
        commandsSwitchesModel_.UpdateRow(commandSwitch.row, "Rejected", static_cast<int>(SharedTypes::Status::critical));
    }
}

void GroundControl::HandleCommandSwitchGaveUp(qint64 sequence)
{
    if (!pendingCommandSwitches_.contains(sequence))
        return;

    const PendingFault commandSwitch = pendingCommandSwitches_.take(sequence);
    cout << CommandSwitchName(commandSwitch.row, commandSwitch.active).toStdString() << ": no response" << endl;
    switchOutcomeSuffix_[commandSwitch.row] = " (No Response)";
    commandsSwitchesModel_.UpdateRow(commandSwitch.row, "No Response", static_cast<int>(SharedTypes::Status::critical));
}

bool GroundControl::SwitchPending(int switchRow) const
{
    for (const PendingFault &pending : pendingCommandSwitches_)
        if (pending.row == switchRow)
            return true;

    return false;
}

void GroundControl::UpdateSafeModeSwitchRow(bool stale)
{
    if (SwitchPending(safeModeRow))
        return;

    const bool inSafeMode = mode_ == SharedTypes::Mode::safe;
    const QString suffix = switchOutcomeSuffix_.value(safeModeRow);
    SharedTypes::Status status = stale               ? SharedTypes::Status::stale
                                 : !suffix.isEmpty() ? SharedTypes::Status::critical
                                 : inSafeMode        ? SharedTypes::Status::warning
                                                     : SharedTypes::Status::none;
    commandsSwitchesModel_.UpdateRow(safeModeRow, (inSafeMode ? "On" : "Off") + suffix, static_cast<int>(status));
}