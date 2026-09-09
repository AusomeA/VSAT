#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QElapsedTimer>
#include "ReadoutsModel.h"
#include "Discovery.h"
#include "UdpReceiver.h"
#include "SharedTypes.h"
#include "TelemetryReadouts.h"
#include "AckUdpSender.h"

enum GCReadoutRowIndex
{
    flightComputerLinkRow,
    simulatorLinkRow,
    modeRow,
    gcHeaderRowCount
};

enum FaultRowIndex
{
    temperatureSensorFaultRow,
    powerSensorFaultRow,
    attitudeSensorFaultRow,
    chaosRow,
    faultRowCount
};

enum CommandRowIndex
{
    rebootRow,
    pingRow,
    commandRowCount
};

enum CommandSwitchRowIndex
{
    safeModeRow,
    payloadInhibitRow,
    commandSwitchRowCount
};

enum AdjustRowIndex
{
    batteryAdjustRow,
    timeScaleAdjustRow,
    adjustRowCount
};

class GroundControl : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GroundControl)
    Q_PROPERTY(QAbstractItemModel *readoutsModel READ ReadoutsModelPtr CONSTANT)
    Q_PROPERTY(bool flightComputerLinked READ FlightComputerLinked NOTIFY summaryChanged)
    Q_PROPERTY(bool simulatorLinked READ SimulatorLinked NOTIFY summaryChanged)
    Q_PROPERTY(QString modeText READ CurrentModeText NOTIFY summaryChanged)
    Q_PROPERTY(int modeStatus READ CurrentModeStatus NOTIFY summaryChanged)
    Q_PROPERTY(QAbstractItemModel *faultsModel READ FaultsModelPtr CONSTANT)
    Q_PROPERTY(QAbstractItemModel *commandsModel READ CommandsModelPtr CONSTANT)
    Q_PROPERTY(QAbstractItemModel *commandsSwitchModel READ CommandsSwitchModelPtr CONSTANT)
    Q_PROPERTY(QAbstractItemModel *inhibitsModel READ InhibitsModelPtr CONSTANT)
    Q_PROPERTY(QAbstractItemModel *adjustsModel READ AdjustsModelPtr CONSTANT)

public:
    GroundControl(QObject *parent = nullptr);

    QAbstractItemModel *ReadoutsModelPtr() { return &readoutsModel_; }

    bool FlightComputerLinked() const { return flightComputerLinked_; }
    bool SimulatorLinked() const { return simulatorDiscovered_; }
    QString CurrentModeText() const { return ModeText(mode_); }
    int CurrentModeStatus() const { return static_cast<int>(flightComputerLinked_ ? GetModeStatus(mode_) : SharedTypes::Status::stale); }

    QAbstractItemModel *FaultsModelPtr() { return &faultsModel_; }
    Q_INVOKABLE void SetFault(int faultRow, bool active);

    QAbstractItemModel *CommandsModelPtr() { return &commandsModel_; }
    Q_INVOKABLE void SendCommand(int commandRow);

    QAbstractItemModel *CommandsSwitchModelPtr() { return &commandsSwitchesModel_; }
    Q_INVOKABLE void SetCommandSwitch(int switchRow, bool on);

    QAbstractItemModel *InhibitsModelPtr() { return &inhibitsModel_; }
    Q_INVOKABLE void SetInhibits(int faultRow, bool inhibited);

    QAbstractItemModel *AdjustsModelPtr() { return &adjustsModel_; }
    Q_INVOKABLE void SendAdjust(int adjustRow, bool increase);

signals:
    void summaryChanged();

private:
    ReadoutsModel readoutsModel_;
    UdpReceiver telemetryReceiver_;

    Discovery discovery_{SharedTypes::groundControlName, SharedTypes::defaultVehicleName};

    QTimer linkCheckTimer_;
    QElapsedTimer timeSinceLastPacket_;

    SharedTypes::Telemetry telemetry_;
    SharedTypes::Mode mode_ = SharedTypes::Mode::nominal;
    bool simLinkOk_ = false;
    bool flightComputerLinked_ = false;
    bool simulatorDiscovered_ = false;

    double lastMETSeconds_ = -1.0;
    float clockTimeScale_ = 1.f;
    QElapsedTimer timeSinceMETSync_;

    static constexpr int linkCheckIntervalMilliseconds = 200;

    ReadoutsModel faultsModel_;
    AckUdpSender godSender_;

    struct PendingFault
    {
        int row = 0;
        bool active = false;
    };

    QMap<qint64, PendingFault> pendingFaults_;

    ReadoutsModel adjustsModel_;
    struct PendingAdjust
    {
        int row = 0;
        QString faultName;
    };
    QMap<qint64, PendingAdjust> pendingAdjusts_;

    ReadoutsModel commandsModel_;
    AckUdpSender groundSender_;
    QMap<qint64, int> pendingCommands_;

    ReadoutsModel commandsSwitchesModel_;
    QMap<qint64, PendingFault> pendingCommandSwitches_;
    QMap<int, QString> switchOutcomeSuffix_;

    ReadoutsModel inhibitsModel_;
    AckUdpSender inhibitSender_;
    QMap<qint64, PendingFault> pendingInhibits_;

    bool rebootInProgress_ = false;

    void HandleGroundTelemetry(const QByteArray &payload);
    void UpdateLinkRow();
    void PopulateRows();
    void UpdateRows(bool stale = false);

    void SyncMissionClock(double METSeconds, float timeScale);
    double GetEstimatedMETSeconds() const;
    void UpdateMissionClockRows();

    static QString FaultName(int faultRow);
    void HandleFaultAck(qint64 sequence, bool accepted);
    void HandleFaultGaveUp(qint64 sequence);

    static QString CommandName(int commandRow);
    void HandleCommandAck(qint64 sequence, bool accepted);
    void HandleCommandGaveUp(qint64 sequence);

    void HandleInhibitAck(qint64 sequence, bool accepted);
    void HandleInhibitGaveUp(qint64 sequence);
    void ResetInhibitRows();

    static QString AdjustName(int adjustRow, bool increase);
    void HandleAdjustAck(qint64 sequence, bool accepted);
    void HandleAdjustGaveUp(qint64 sequence);

    static QString CommandSwitchName(int switchRow, bool on);
    void HandleCommandSwitchAck(qint64 sequence, bool accepted);
    void HandleCommandSwitchGaveUp(qint64 sequence);
    bool SwitchPending(int switchRow) const;
    void UpdateSafeModeSwitchRow(bool stale);
};