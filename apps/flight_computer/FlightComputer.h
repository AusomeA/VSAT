#pragma once
#include "SharedTypes.h"
#include <QSet>

class FlightComputer
{

public:
    FlightComputer();

    SharedTypes::Commands Update(const SharedTypes::Telemetry &telemetry);
    bool RequestExitSafeMode();
    bool ForceSafeMode();
    bool Reboot();
    bool SetFaultInhibited(const QString &faultName, bool inhibited);
    bool SetPayloadInhibited(bool inhibited);

    SharedTypes::Mode GetMode() const { return mode_; }
    const SharedTypes::Telemetry &GetTelemetry() const { return telemetry_; }
    double GetLastGroundContactSeconds() const { return lastGroundContactSeconds_; }

private:
    SharedTypes::Mode mode_;
    bool payloadEnabled_;
    bool commsTransmitting_;
    bool heaterEnabled_; // FC's own last-commanded state - deliberately NOT read from telemetry
    bool payloadInhibited_ = false;

    SharedTypes::Telemetry telemetry_;

    double lastGroundContactSeconds_;

    bool exitSafeModeRequested_ = false;
    QSet<QString> inhibitedFaults_;

    static constexpr float maxSecondsWithoutGroundContact = 10800; // max amount of time without contact before we go into safe mode

    static constexpr float minSecondsUntilSunrise = SharedTypes::eclipseSeconds / 3.f; // no need to go to safe mode on low battery if sun is about to rise

    static constexpr float minCommsBatteryPercent = 10.f;

    void ModeCheck();
    void PayloadCheck();
    void CommsCheck();
    void HeaterCheck();

    bool GetSensorFailed(bool healthy, const QString &faultName) const { return !healthy && !inhibitedFaults_.contains(faultName); }

    float TimeIntoOrbit() const;
};