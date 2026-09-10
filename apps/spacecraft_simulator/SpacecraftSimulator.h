#pragma once
#include "SharedTypes.h"
#include "ReadoutsModel.h"
#include "UdpReceiver.h"
#include "Discovery.h"
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QString>
#include <QUdpSocket>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QHash>

enum ReadoutRowIndex
{
    timeScaleRow,
    modeRow,
    runningRow,
    METRow,
    batteryRow,
    solarGenerationRow,
    powerConsumptionRow,
    temperatureRow,
    heaterRow,
    radiatorRow,
    sunlightRow,
    commsRow,
    commsTransmittingRow,
    temperatureSensorRow,
    powerSensorRow,
    attitudeSensorRow,
    payloadRow,
    chaosRow,
    readoutRowCount
};

class SpacecraftSimulator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Spacecraft)
    Q_PROPERTY(double timeScale READ TimeScale NOTIFY timeScaleChanged)
    Q_PROPERTY(QAbstractItemModel *readoutsModel READ ReadoutsModelPtr CONSTANT)

public:

    SpacecraftSimulator(QObject *parent = nullptr);

    Q_INVOKABLE void Start();
    Q_INVOKABLE void Stop();
    void Update(double deltaTimeSeconds);

    Q_INVOKABLE void IncreaseTimeScale();
    Q_INVOKABLE void DecreaseTimeScale();
    double TimeScale() const { return timeScale_; }

    Q_INVOKABLE void ToggleSensorFault(int sensorIndex);
    Q_INVOKABLE void FailRandomSensor(int randomNumber);

    Q_INVOKABLE void ToggleChaosMode();

    SharedTypes::Mode GetMode() const { return mode_; }

    QAbstractItemModel *ReadoutsModelPtr() { return &readoutsModel_; }

    /////// Testing Functions ////////////
    Q_INVOKABLE void BatteryTestUp();
    Q_INVOKABLE void BatteryTestDown();
    /////////////////////////////////////

signals:
    void timeScaleChanged();

private:
    // Variables
    SharedTypes::Mode mode_;

    bool isRunning_;

    double missionElapsedTimeSeconds_;
    float updateIntervalSeconds_;
    float timeScale_;

    float batteryCapacityWattHours_; // Total capacity of battery
    float batteryEnergyWattHours_;   // How much juice is in the battery

    float solarGenerationWatts_;
    float powerConsumptionWatts_;
    float temperatureCelsius_;

    bool isInSunlight_;

    bool communicationsAvailable_;
    bool temperatureSensorHealthy_;
    bool powerSensorHealthy_;
    bool attitudeSensorHealthy_;

    bool payloadEnabled_;
    bool commsTransmitting_;
    bool heaterEnabled_;
    bool radiatorLouversOpen_;

    bool chaosEnabled_;
    static constexpr float meanSecondsBetweenFaults = 10800.f; // the mean seconds before a fault happens in chaos mode (10800 = 2 x 90 min orbits)

    QUdpSocket telemetrySocket_;
    UdpReceiver commandReceiver_;
    UdpReceiver faultReceiver_;
    QHostAddress flightComputerAddress_;

    struct LastAck
    {
        qint64 sequence = -1;
        bool accepted = false;
    };
    QHash<QString, LastAck> lastAcks_;

    QTimer telemetrySendTimer_;
    QTimer godStatusTimer_;
    qint64 godStatusSequence_ = 0;
    
    ReadoutsModel readoutsModel_;

    Discovery discovery_{SharedTypes::simulatorName, SharedTypes::defaultVehicleName};
    
    // Temperature Variables
    static constexpr float sunlitEquilibriumCelsius = 32.5f;    // what temp it settles in sunlight
    static constexpr float eclipseEquilibriumCelsius = -5.f;    // what temp it settles in darkness
    static constexpr float thermalTimeConstantSeconds = 600.f;  // how fast it responds
    static constexpr float celsiusPerWatt = 0.8f;               // internal heat from electronics (including heater)
    static constexpr float louverCoolingCelsius = 10.f;         // how much the radiator cools

    // Networking Variables
    static constexpr int telemetrySendIntervalMilliseconds = 200; // 5Hz, subject to change
    static constexpr int godStatusIntervalMilliseconds = 1000;    // for god mode battery updater
    
    // Time Scale Variables
    static constexpr float minTimeScale = .5f;
    static constexpr float maxTimeScale = 64.f;

    // Radiator Variables
    static constexpr float radiatorLouversOpenCelsius = 25.f;
    static constexpr float radiatorLouversClosedCelsius = 20.f;

    // Survivor Heater Variables
    static constexpr float survivalHeaterOnCelsius = 5.f;
    static constexpr float survivalHeaterOffCelsius = 15.f;

    // Brownout variables
    static constexpr float brownoutEntryPercent = 5.f;
    static constexpr float brownoutRecoveryPercent = 10.f;
    static constexpr float blackoutRecoveryPercent = 5.f;
    
    // Functions
    QTimer updateTimer_;
    QElapsedTimer timeSinceLastCommand_;
    bool inCommandFallback_;                // true when we have lost contact with the flight computer
    bool brownedOut_;                       // battery very low
    bool blackedOut_;                       // battery depleted

    float BatteryCalculation() const { return batteryEnergyWattHours_ / batteryCapacityWattHours_ * 100.f; }

    float TimeIntoOrbit() const { return fmod(missionElapsedTimeSeconds_, SharedTypes::orbitPeriodSeconds); }

    void AdvanceOneTick();

    SharedTypes::Telemetry BuildTelemetry() const;

    void SetHeater(bool enabled);

    void BrownoutCheck();
    void BlackoutCheck();
    void CommsCheck();
    void HeaterCheck();
    void RadiatorCheck();
    void CommandLossCheck();

    void PopulateReadouts();
    void UpdateReadouts();

    void SendTelemetry();
    void SendGodStatus();

    void HandleCommands(const QByteArray &payload);
    void HandleFaultInjection(const QByteArray &payload, const QHostAddress &senderAddress, quint16 senderPort);
    bool ApplyFaultInjection(const QString &faultName, bool active);

    void UpdatePowerConsumption();
};