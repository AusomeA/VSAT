#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QUdpSocket>

namespace SharedTypes
{
    Q_NAMESPACE
    QML_ELEMENT

    inline constexpr quint16 simTelemetryPort = 45000;    // sim->FC
    inline constexpr quint16 simCommandPort = 45001;      // FC->sim
    inline constexpr quint16 groundTelemetryPort = 45002; // fc->gc
    inline constexpr quint16 groundCommandPort = 45003;   // gc->fc
    inline constexpr quint16 godPort = 45004;             // gc->sim, god mode port for fault injection
    inline constexpr quint16 discoveryPort = 45010;       // find peers on the network

    static constexpr int discoveryIntervalMilliseconds = 1000;
    inline const QString simulatorName = "spacecraft_simulator";
    inline const QString flightComputerName = "flight_computer";
    inline const QString groundControlName = "ground_control";
    inline const QString defaultVehicleName = "vsat-1";

    // Orbit times
    inline constexpr float orbitPeriodSeconds = 5400.f;
    inline constexpr float sunlitSeconds = 3000.f;
    inline constexpr float eclipseSeconds = orbitPeriodSeconds - sunlitSeconds;

    // payload windows
    inline constexpr float payloadStartTime = 900.f;
    inline constexpr float payloadEndTime = 1500.f;

    // Comms variables
    inline constexpr float commsStart = 1200.f;
    inline constexpr float commsEnd = 1800.f;
    inline constexpr float beaconPeriodSeconds = 150.f;
    inline constexpr float beaconTransmitSeconds = 15.f;

    inline constexpr int linkLostMilliseconds = 3000;

    // Heater variables
    inline constexpr float heaterOnCelsius = 15.f;
    inline constexpr float heaterOffCelsius = 25.f;

    // Battery thresholds
    inline constexpr float degradedEntryBatteryPercent = 50.f;
    inline constexpr float safeEntryBatteryPercent = 30.f;
    inline constexpr float nominalRecoveryBatteryPercent = 60.f;

    // message vocabulary
    inline const QString groundTelemetryMessageType = "groundTelemetry";
    inline const QString faultInjectionMessageType = "faultInjection";
    inline const QString groundCommandMessageType = "groundCommand";
    inline const QString ackMessageType = "ack";

    // fault messages
    inline const QString temperatureSensorFaultMessage = "temperatureSensorFault";
    inline const QString powerSensorFaultMessage = "powerSensorFault";
    inline const QString attitudeSensorFaultMessage = "attitudeSensorFault";
    inline const QString chaosFaultMessage = "chaosMode";
    inline const QString batteryUpMessage = "batteryUp";
    inline const QString batteryDownMessage = "batteryDown";
    inline const QString timeScaleUpMessage = "timeScaleUp";
    inline const QString timeScaleDownMessage = "timeScaleDown";

    // command messages
    inline const QString exitSafeModeCommand = "exitSafeMode";
    inline const QString rebootCommand = "reboot";
    inline const QString inhibitFaultCommand = "inhibitFault";
    inline const QString forceSafeModeCommand = "forceSafeMode";
    inline const QString pingCommand = "ping";

    // Ack Variables
    inline constexpr int ackTimeoutMilliseconds = 500;
    inline constexpr int maxSendAttempts = 3;

    enum class Mode
    {
        nominal,
        degraded,
        safe
    };
    Q_ENUM_NS(Mode)

    enum class Status
    {
        good,
        warning,
        critical,
        none,
        stale
    };
    Q_ENUM_NS(Status)

    struct Telemetry
    {
        double missionElapsedTimeSeconds = 0.0;
        float batteryPercent = 0.f;
        float solarGenerationWatts = 0.f;
        float powerConsumptionWatts = 0.f;
        float temperatureCelsius = 0.f;
        bool heaterEnabled = false;
        bool radiatorLouversOpen = false;
        bool isInSunlight = false;
        bool temperatureSensorHealthy = true;
        bool powerSensorHealthy = true;
        bool attitudeSensorHealthy = true;
        bool communicationsAvailable = false;
        bool commsTransmitting = false;
        bool payloadEnabled = false;
        float secondsUntilSunrise = 0.f;
    };

    struct Commands
    {
        Mode mode = Mode::nominal;
        bool payloadEnabled = false;
        bool commsTransmitting = false;
        bool heaterEnabled = false;
    };
}