#include "SpacecraftSimulator.h"
#include "ReadoutFormatting.h"
#include "Helpers.h"
#include "TelemetryJson.h"
#include "EnvelopeJson.h"
#include <iostream>
#include <QTime>
#include <cmath>
#include <algorithm>
#include <QRandomGenerator>
#include <QHostAddress>
#include <QCoreApplication>

using namespace std;

SpacecraftSimulator::SpacecraftSimulator(QObject *parent)
    : QObject(parent),
      commandReceiver_(SharedTypes::simCommandPort),
      faultReceiver_(SharedTypes::godPort),
      flightComputerAddress_(QHostAddress::LocalHost),
      mode_(SharedTypes::Mode::nominal),
      isRunning_(false),
      missionElapsedTimeSeconds_(0.0),
      updateIntervalSeconds_(1.0),
      timeScale_(1.f),
      batteryCapacityWattHours_(45.f),
      batteryEnergyWattHours_(batteryCapacityWattHours_),
      solarGenerationWatts_(40.f),
      powerConsumptionWatts_(20.f),
      temperatureCelsius_(22.f),
      isInSunlight_(false),
      communicationsAvailable_(true),
      temperatureSensorHealthy_(true),
      powerSensorHealthy_(true),
      attitudeSensorHealthy_(true),
      payloadEnabled_(false),
      commsTransmitting_(false),
      heaterEnabled_(false),
      radiatorLouversOpen_(false),
      chaosEnabled_(false),
      inCommandFallback_(false), // 3 s grace period before we fallback
      brownedOut_(false),
      blackedOut_(false)
{
    connect(&updateTimer_, &QTimer::timeout, this, &SpacecraftSimulator::AdvanceOneTick);
    connect(&telemetrySendTimer_, &QTimer::timeout, this, &SpacecraftSimulator::SendTelemetry);
    connect(&commandReceiver_, &UdpReceiver::DatagramReceived, this, &SpacecraftSimulator::HandleCommands);
    connect(&faultReceiver_, &UdpReceiver::DatagramReceived, this, &SpacecraftSimulator::HandleFaultInjection);
    connect(&discovery_, &Discovery::peerAppeared, this, [this](const QString &appName, const QHostAddress &address)
            {
        if(appName == SharedTypes::flightComputerName)
            flightComputerAddress_ = address; });
    telemetrySendTimer_.start(telemetrySendIntervalMilliseconds);

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() > 1)
    {
        flightComputerAddress_ = QHostAddress(arguments[1]);
        if (flightComputerAddress_.isNull())
            qFatal("Invalid address argument: %s", qPrintable(arguments[1]));
    }

    timeSinceLastCommand_.start();
}

void SpacecraftSimulator::Start()
{
    isRunning_ = true;

    updateTimer_.start(static_cast<int>(updateIntervalSeconds_ * 1000));
    cout << "Simulation started." << endl;

    ///// Testing Variables //////
    // missionElapsedTimeSeconds_ = 0;
    // batteryPercentage_ = 20.f;
    // solarGenerationWatts_ = 10.01f;
    // powerConsumptionWatts_ = 34.9f;
    // temperatureCelsius_ = 29.9f;
    // isInSunlight_ = false;
    // communicationsAvailable_ = false;
    // temperatureSensorHealthy_ = false;
    // powerSensorHealthy_ = false;
    // attitudeSensorHealthy_ = false;
    // payloadEnabled_ = false;

    PopulateReadouts();
}

void SpacecraftSimulator::Stop()
{
    isRunning_ = false;
    updateTimer_.stop();
    cout << "Simulation stopped." << endl;
    UpdateReadouts();
}

void SpacecraftSimulator::Update(double deltaTimeSeconds)
{
    if (!isRunning_)
    {
        cout << "Simulation is not running. Cannot update." << endl;
        return;
    }

    missionElapsedTimeSeconds_ += deltaTimeSeconds;

    // sun calculation
    isInSunlight_ = TimeIntoOrbit() > SharedTypes::eclipseSeconds;

    if (!blackedOut_)
    {
        // fault injection
        if (chaosEnabled_ && QRandomGenerator::global()->generateDouble() < deltaTimeSeconds / meanSecondsBetweenFaults)
            FailRandomSensor(QRandomGenerator::global()->bounded(3));

        // power checks
        CommandLossCheck();
        CommsCheck();
        BrownoutCheck();
    }

    BlackoutCheck();

    RadiatorCheck();

    // watts calculations
    if (isInSunlight_)
        solarGenerationWatts_ = 40.f;
    else
        solarGenerationWatts_ = 0.f;

    UpdatePowerConsumption();

    // battery calculations
    float netPowerWatts = solarGenerationWatts_ - powerConsumptionWatts_;

    batteryEnergyWattHours_ += netPowerWatts * (deltaTimeSeconds / 3600);
    batteryEnergyWattHours_ = qBound(0.f, batteryEnergyWattHours_, batteryCapacityWattHours_);

    // temperature calculation
    float targetTemperature = (isInSunlight_ ? sunlitEquilibriumCelsius : eclipseEquilibriumCelsius) + powerConsumptionWatts_ * celsiusPerWatt; // where the temperature settles
    if (radiatorLouversOpen_)
        targetTemperature -= louverCoolingCelsius;
    float thermalResponse = 1.f - std::exp(-deltaTimeSeconds / thermalTimeConstantSeconds); // needed to calculate exactly (even with increased time scale)
    temperatureCelsius_ += (targetTemperature - temperatureCelsius_) * thermalResponse;
}

void SpacecraftSimulator::IncreaseTimeScale()
{
    if (timeScale_ != maxTimeScale)
    {
        timeScale_ = min(timeScale_ * 2.0, static_cast<double>(maxTimeScale));
        cout << "Time Scale = " << timeScale_ << endl;
        emit timeScaleChanged();
        UpdateReadouts();
    }
}

void SpacecraftSimulator::DecreaseTimeScale()
{
    if (timeScale_ != minTimeScale)
    {
        timeScale_ = max(timeScale_ / 2.0, static_cast<double>(minTimeScale));
        cout << "Time Scale = " << timeScale_ << endl;
        emit timeScaleChanged();
        UpdateReadouts();
    }
}

void SpacecraftSimulator::AdvanceOneTick()
{
    float simulatedSeconds = updateIntervalSeconds_ * timeScale_;

    while (simulatedSeconds > 0.0)
    {
        float physicsStepSeconds = min(simulatedSeconds, 1.f);
        Update(physicsStepSeconds);
        simulatedSeconds -= physicsStepSeconds;
    }

    UpdateReadouts();
}

SharedTypes::Telemetry SpacecraftSimulator::BuildTelemetry() const
{
    SharedTypes::Telemetry telemetry;
    telemetry.missionElapsedTimeSeconds = missionElapsedTimeSeconds_;
    telemetry.batteryPercent = BatteryCalculation();
    telemetry.solarGenerationWatts = solarGenerationWatts_;
    telemetry.powerConsumptionWatts = powerConsumptionWatts_;
    telemetry.temperatureCelsius = temperatureCelsius_;
    telemetry.heaterEnabled = heaterEnabled_;
    telemetry.radiatorLouversOpen = radiatorLouversOpen_;
    telemetry.isInSunlight = isInSunlight_;
    telemetry.temperatureSensorHealthy = temperatureSensorHealthy_;
    telemetry.powerSensorHealthy = powerSensorHealthy_;
    telemetry.attitudeSensorHealthy = attitudeSensorHealthy_;
    telemetry.communicationsAvailable = communicationsAvailable_;
    telemetry.commsTransmitting = commsTransmitting_;
    telemetry.payloadEnabled = payloadEnabled_;
    telemetry.secondsUntilSunrise = isInSunlight_ ? 0.f : SharedTypes::eclipseSeconds - TimeIntoOrbit();
    telemetry.timeScale = timeScale_;
    return telemetry;
}

void SpacecraftSimulator::SetHeater(bool enabled)
{
    if (enabled && brownedOut_)
        return;

    if (enabled == heaterEnabled_)
        return;

    heaterEnabled_ = enabled;

    if (!blackedOut_)
        cout << (enabled ? "Heater turned on" : "Heater turned off") << endl;
}

void SpacecraftSimulator::BrownoutCheck()
{
    if (!brownedOut_ && BatteryCalculation() <= brownoutEntryPercent)
    {
        brownedOut_ = true;
        cout << "Brownout" << endl;
    }
    else if (brownedOut_ && BatteryCalculation() > brownoutRecoveryPercent)
    {
        brownedOut_ = false;
        cout << "Recovered from Brownout" << endl;
    }

    if (brownedOut_)
    {
        payloadEnabled_ = false;
        commsTransmitting_ = false;
        SetHeater(false);
    }
}

void SpacecraftSimulator::BlackoutCheck()
{
    if (!blackedOut_ && batteryEnergyWattHours_ <= 0.5f)
    {
        blackedOut_ = true;
        payloadEnabled_ = false;
        commsTransmitting_ = false;
        SetHeater(false);
        cout << "Blackout" << endl;
    }
    else if (blackedOut_ && BatteryCalculation() > blackoutRecoveryPercent)
    {
        blackedOut_ = false;
        inCommandFallback_ = false;
        timeSinceLastCommand_.restart();
        cout << "Recovered from Blackout" << endl;
    }
}

void SpacecraftSimulator::CommsCheck()
{
    float timeInOrbit = TimeIntoOrbit();
    communicationsAvailable_ = timeInOrbit >= SharedTypes::commsStart && timeInOrbit <= SharedTypes::commsEnd;
}

void SpacecraftSimulator::HeaterCheck()
{
    if (!heaterEnabled_ && !isInSunlight_ && temperatureCelsius_ <= survivalHeaterOnCelsius)
        SetHeater(true);
    else if (heaterEnabled_ && (isInSunlight_ || temperatureCelsius_ > survivalHeaterOffCelsius))
        SetHeater(false);
}

void SpacecraftSimulator::RadiatorCheck()
{
    if (!radiatorLouversOpen_ && isInSunlight_ && temperatureCelsius_ >= radiatorLouversOpenCelsius)
    {
        radiatorLouversOpen_ = true;
        if (!blackedOut_)
            cout << "Radiator louvers opened" << endl;
    }
    else if (radiatorLouversOpen_ && (!isInSunlight_ || temperatureCelsius_ < radiatorLouversClosedCelsius))
    {
        radiatorLouversOpen_ = false;
        if (!blackedOut_)
            cout << "Radiator louvers closed" << endl;
    }
}

void SpacecraftSimulator::CommandLossCheck()
{
    bool commandsLost = timeSinceLastCommand_.elapsed() > SharedTypes::linkLostMilliseconds;

    if (commandsLost && !inCommandFallback_)
    {
        inCommandFallback_ = true;
        cout << "Command link lost to flight computer - entering fallback" << endl;
    }
    else if (!commandsLost && inCommandFallback_)
    {
        inCommandFallback_ = false;
        cout << "Command link restored" << endl;
    }

    if (!inCommandFallback_)
        return;

    payloadEnabled_ = false;
    HeaterCheck();

    float timeIntoBeaconCycle = fmod(missionElapsedTimeSeconds_, SharedTypes::beaconPeriodSeconds);
    commsTransmitting_ = timeIntoBeaconCycle <= SharedTypes::beaconTransmitSeconds;
}

void SpacecraftSimulator::PopulateReadouts()
{
    QVector<ReadoutRow> rows{
        {"Time Scale", "", 0},
        {"Mode", "", 0},
        {"Running", "", 0},
        {"MET", "", 0},
        {"Battery", "", 0},
        {"Solar Generation", "", 0},
        {"Power Consumption", "", 0},
        {"Temperature", "", 0},
        {"Heater", "", 0},
        {"Radiator Louvers", "", 0},
        {"In Sunlight", "", 0},
        {"Comms Available", "", 0},
        {"Comms Transmitting", "", 0},
        {"Temp Sensor OK", "", 0},
        {"Power Sensor OK", "", 0},
        {"Attitude Sensor OK", "", 0},
        {"Payload Enabled", "", 0},
        {"Chaos Enabled", "", 0}};

    readoutsModel_.SetRows(rows);
    UpdateReadouts();
}

void SpacecraftSimulator::UpdateReadouts()
{
    SharedTypes::Status runningStatus = isRunning_ ? SharedTypes::Status::good : SharedTypes::Status::critical;

    readoutsModel_.UpdateRow(timeScaleRow, QString("%1 x").arg(timeScale_, 0, 'f', 1), static_cast<int>(SharedTypes::Status::none));
    readoutsModel_.UpdateRow(modeRow,
                             inCommandFallback_                     ? "Fallback"
                             : mode_ == SharedTypes::Mode::nominal  ? "Nominal"
                             : mode_ == SharedTypes::Mode::degraded ? "Degraded"
                                                                    : "Safe",
                             static_cast<int>(inCommandFallback_ ? SharedTypes::Status::warning : static_cast<SharedTypes::Status>(mode_)));
    readoutsModel_.UpdateRow(runningRow, isRunning_ ? "Yes" : "No", static_cast<int>(runningStatus));
    readoutsModel_.UpdateRow(METRow, MissionElapsedTimeText(missionElapsedTimeSeconds_), static_cast<int>(SharedTypes::Status::none));
    readoutsModel_.UpdateRow(batteryRow, QString("%1 %").arg(BatteryCalculation(), 0, 'f', 1), static_cast<int>(GetBatteryStatus(BatteryCalculation())));
    readoutsModel_.UpdateRow(solarGenerationRow, QString("%1 W").arg(solarGenerationWatts_, 0, 'f', 1), static_cast<int>(GetSolarGenerationStatus(solarGenerationWatts_, isInSunlight_)));
    readoutsModel_.UpdateRow(powerConsumptionRow, QString("%1 W").arg(powerConsumptionWatts_, 0, 'f', 1), static_cast<int>(GetPowerConsumptionStatus(BuildTelemetry())));
    readoutsModel_.UpdateRow(temperatureRow, QString("%1 C").arg(temperatureCelsius_, 0, 'f', 1), static_cast<int>(GetTemperatureStatus(temperatureCelsius_)));
    readoutsModel_.UpdateRow(heaterRow, heaterEnabled_ ? "On" : "Off", static_cast<int>(SharedTypes::Status::none));
    readoutsModel_.UpdateRow(radiatorRow, radiatorLouversOpen_ ? "Open" : "Shut", static_cast<int>(SharedTypes::Status::none));
    readoutsModel_.UpdateRow(sunlightRow, isInSunlight_ ? "Yes" : "No", static_cast<int>(isInSunlight_ ? SharedTypes::Status::good : SharedTypes::Status::none));
    readoutsModel_.UpdateRow(commsRow, communicationsAvailable_ ? "Yes" : "No", static_cast<int>(communicationsAvailable_ ? SharedTypes::Status::good : SharedTypes::Status::none));
    readoutsModel_.UpdateRow(commsTransmittingRow, commsTransmitting_ ? "Yes" : "No", static_cast<int>(commsTransmitting_ ? SharedTypes::Status::good : SharedTypes::Status::none));
    readoutsModel_.UpdateRow(temperatureSensorRow, temperatureSensorHealthy_ ? "Yes" : "No", static_cast<int>(temperatureSensorHealthy_ ? SharedTypes::Status::good : SharedTypes::Status::critical));
    readoutsModel_.UpdateRow(powerSensorRow, powerSensorHealthy_ ? "Yes" : "No", static_cast<int>(powerSensorHealthy_ ? SharedTypes::Status::good : SharedTypes::Status::critical));
    readoutsModel_.UpdateRow(attitudeSensorRow, attitudeSensorHealthy_ ? "Yes" : "No", static_cast<int>(attitudeSensorHealthy_ ? SharedTypes::Status::good : SharedTypes::Status::critical));
    readoutsModel_.UpdateRow(payloadRow, payloadEnabled_ ? "Yes" : "No", static_cast<int>(payloadEnabled_ ? SharedTypes::Status::good : SharedTypes::Status::none));
    readoutsModel_.UpdateRow(chaosRow, chaosEnabled_ ? "Yes" : "No", static_cast<int>(SharedTypes::Status::none));
}

void SpacecraftSimulator::SendTelemetry()
{
    if (blackedOut_)
        return;

    telemetrySocket_.writeDatagram(TelemetryToJson(BuildTelemetry()), flightComputerAddress_, SharedTypes::simTelemetryPort); // local host changes when we move to pi's
}

void SpacecraftSimulator::HandleCommands(const QByteArray &payload)
{
    if (blackedOut_)
        return;

    std::optional<SharedTypes::Commands> commands = CommandsFromJson(payload);
    if (!commands)
    {
        qWarning() << "Dropped malformed command packet";
        return;
    }
    mode_ = commands->mode;
    if (!brownedOut_)
    {
        payloadEnabled_ = commands->payloadEnabled;
        commsTransmitting_ = commands->commsTransmitting;
    }
    SetHeater(commands->heaterEnabled);
    timeSinceLastCommand_.restart();
    UpdatePowerConsumption();
    UpdateReadouts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Testing Functions ////////////////////////////////////////////////////

void SpacecraftSimulator::BatteryTestUp()
{
    batteryEnergyWattHours_ += 10.f;
    cout << "Battery added by 10 for testing" << endl;
    UpdateReadouts();
}

void SpacecraftSimulator::BatteryTestDown()
{
    batteryEnergyWattHours_ -= 10.f;
    cout << "Battery dropped by 10 for testing" << endl;
    UpdateReadouts();
}

void SpacecraftSimulator::ToggleSensorFault(int sensorIndex)
{
    switch (sensorIndex)
    {
    case 0:
        temperatureSensorHealthy_ = !temperatureSensorHealthy_;
        cout << "temperature sensor healthy is " << (temperatureSensorHealthy_ ? "true" : "false") << endl;
        break;
    case 1:
        powerSensorHealthy_ = !powerSensorHealthy_;
        cout << "power sensor healthy is " << (powerSensorHealthy_ ? "true" : "false") << endl;
        break;
    case 2:
        attitudeSensorHealthy_ = !attitudeSensorHealthy_;
        cout << "attitude sensor healthy is " << (attitudeSensorHealthy_ ? "true" : "false") << endl;
        break;
    default:
        cout << "incorrect input" << endl;
        break;
    }
}

void SpacecraftSimulator::FailRandomSensor(int randomNumber)
{
    switch (randomNumber)
    {
    case 0:
        temperatureSensorHealthy_ = false;
        cout << "temperature sensor failed" << endl;
        break;
    case 1:
        powerSensorHealthy_ = false;
        cout << "power sensor failed" << endl;
        break;
    case 2:
        attitudeSensorHealthy_ = false;
        cout << "attitude sensor failed" << endl;
        break;
    default:
        cout << "random failure function failed (wrong index?)" << endl;
        break;
    }
}

void SpacecraftSimulator::ToggleChaosMode()
{
    chaosEnabled_ = !chaosEnabled_;
    cout << "Chaos mode " << (chaosEnabled_ ? "on" : "off") << endl;
}

void SpacecraftSimulator::HandleFaultInjection(const QByteArray &payload, const QHostAddress &senderAddress, quint16 senderPort)
{
    std::optional<Envelope> envelope = EnvelopeFromJson(payload);
    if (!envelope || envelope->type != SharedTypes::faultInjectionMessageType)
    {
        qWarning() << "Dropped malformed fault injection packet";
        return;
    }

    const QString senderKey = senderAddress.toString() + ":" + QString::number(senderPort);
    LastAck &lastAck = lastAcks_[senderKey];

    if (lastAck.sequence == envelope->sequence)
        cout << "Duplicate fault injection request " << envelope->sequence << " re-acknowledged without applying" << endl;
    else
    {
        const QString faultName = envelope->body["fault"].toString();
        const bool active = envelope->body["active"].toBool();
        lastAck.sequence = envelope->sequence;
        lastAck.accepted = ApplyFaultInjection(faultName, active);
    }

    Envelope ackEnvelope;
    ackEnvelope.type = SharedTypes::ackMessageType;
    ackEnvelope.sequence = envelope->sequence;
    ackEnvelope.body["accepted"] = lastAck.accepted;
    telemetrySocket_.writeDatagram(EnvelopeToJson(ackEnvelope), senderAddress, senderPort);
}

bool SpacecraftSimulator::ApplyFaultInjection(const QString &faultName, bool active)
{
    cout << "Received fault injection request: " << faultName.toStdString() << " active=" << (active ? "true" : "false") << endl;

    const bool healthy = !active;

    if (faultName == SharedTypes::temperatureSensorFaultMessage)
        temperatureSensorHealthy_ = healthy;
    else if (faultName == SharedTypes::powerSensorFaultMessage)
        powerSensorHealthy_ = healthy;
    else if (faultName == SharedTypes::attitudeSensorFaultMessage)
        attitudeSensorHealthy_ = healthy;
    else if (faultName == SharedTypes::chaosFaultMessage)
    {
        chaosEnabled_ = active;
        cout << "Chaos mode " << (active ? "on" : "off") << endl;
        return true;
    }
    else if (faultName == SharedTypes::batteryUpMessage)
    {
        BatteryTestUp();
        return true;
    }
    else if (faultName == SharedTypes::batteryDownMessage)
    {
        BatteryTestDown();
        return true;
    }
    else if (faultName == SharedTypes::timeScaleUpMessage)
    {
        IncreaseTimeScale();
        return true;
    }
    else if (faultName == SharedTypes::timeScaleDownMessage)
    {
        DecreaseTimeScale();
        return true;
    }
    else
    {
        cout << "Rejected unknown fault injection request: " << faultName.toStdString() << endl;
        return false;
    }

    cout << faultName.toStdString() << " set to " << (healthy ? "healthy" : "faulty") << endl;
    return true;
}

void SpacecraftSimulator::UpdatePowerConsumption()
{
    if (blackedOut_)
    {
        powerConsumptionWatts_ = 0.f;
        return;
    }

    powerConsumptionWatts_ = SharedTypes::basePowerWatts + SharedTypes::avionicsPowerWatts;
    if (payloadEnabled_)
        powerConsumptionWatts_ += SharedTypes::payloadPowerWatts;
    if (commsTransmitting_)
        powerConsumptionWatts_ += SharedTypes::commsPowerWatts;
    if (heaterEnabled_)
        powerConsumptionWatts_ += SharedTypes::heaterPowerWatts;
}