#include "FlightComputer.h"
#include <iostream>

using namespace std;

FlightComputer::FlightComputer()
    : mode_(SharedTypes::Mode::nominal),
      payloadEnabled_(false),
      commsTransmitting_(false),
      heaterEnabled_(false),
      lastGroundContactSeconds_(-1.0)
{
}

SharedTypes::Commands FlightComputer::Update(const SharedTypes::Telemetry &telemetry)
{
    telemetry_ = telemetry;

    if (lastGroundContactSeconds_ < 0.0)
        lastGroundContactSeconds_ = telemetry_.missionElapsedTimeSeconds;

    if (telemetry_.communicationsAvailable && telemetry_.commsTransmitting)
        lastGroundContactSeconds_ = telemetry_.missionElapsedTimeSeconds;

    ModeCheck();
    PayloadCheck();
    CommsCheck();
    HeaterCheck();

    SharedTypes::Commands commands;
    commands.mode = mode_;
    commands.payloadEnabled = payloadEnabled_;
    commands.commsTransmitting = commsTransmitting_;
    commands.heaterEnabled = heaterEnabled_;

    return commands;
}

bool FlightComputer::RequestExitSafeMode()
{
    if(mode_ != SharedTypes::Mode::safe)
        return false;

    exitSafeModeRequested_ = true;
    ModeCheck();
    return mode_ != SharedTypes::Mode::safe;
}

bool FlightComputer::ForceSafeMode()
{
    mode_ = SharedTypes::Mode::safe;
    cout << "Spacecraft in safe mode because:" << endl
         << "Commanded by Ground Control" << endl;

    return true;
}

bool FlightComputer::Reboot()
{
    cout << "Flight computer rebooting" << endl;
    *this = FlightComputer();
    return true;
}

bool FlightComputer::SetFaultInhibited(const QString &faultName, bool inhibited)
{
    const bool knownFault = faultName == SharedTypes::temperatureSensorFaultMessage
                         || faultName == SharedTypes::powerSensorFaultMessage
                         || faultName == SharedTypes::attitudeSensorFaultMessage;

    if(!knownFault)
        return false;

    if(inhibited)
        inhibitedFaults_.insert(faultName);
    else
        inhibitedFaults_.remove(faultName);

    cout << "Fault " << faultName.toStdString() << (inhibited ? " inhibited" : " re-enabled") << endl;
    return true;
}

void FlightComputer::ModeCheck()
{
    if (mode_ == SharedTypes::Mode::safe)
    {
        if(!exitSafeModeRequested_)
            return;

        exitSafeModeRequested_ = false;
        mode_ = SharedTypes::Mode::nominal;
        cout << "Ground requested exit from safe mode" << endl;
    }

    if (mode_ == SharedTypes::Mode::degraded)
    {
        if (telemetry_.batteryPercent > SharedTypes::nominalRecoveryBatteryPercent && 
            !GetSensorFailed(telemetry_.temperatureSensorHealthy, SharedTypes::temperatureSensorFaultMessage) && 
            !GetSensorFailed(telemetry_.powerSensorHealthy, SharedTypes::powerSensorFaultMessage) && 
            !GetSensorFailed(telemetry_.attitudeSensorHealthy, SharedTypes::attitudeSensorFaultMessage))
        {
            mode_ = SharedTypes::Mode::nominal;
            cout << "Spacecraft returned to nominal" << endl;
        }
    }

    QString safeModeReasons;

    if (telemetry_.batteryPercent < SharedTypes::safeEntryBatteryPercent)
        safeModeReasons += QString("Battery below %1%\n").arg(SharedTypes::safeEntryBatteryPercent);
    if (GetSensorFailed(telemetry_.temperatureSensorHealthy, SharedTypes::temperatureSensorFaultMessage))
        safeModeReasons += "Temperature sensor failed\n";
    if (GetSensorFailed(telemetry_.powerSensorHealthy, SharedTypes::powerSensorFaultMessage))
        safeModeReasons += "Power sensor failed\n";
    if (GetSensorFailed(telemetry_.attitudeSensorHealthy, SharedTypes::attitudeSensorFaultMessage))
        safeModeReasons += "Attitude sensor failed\n";
    if (telemetry_.missionElapsedTimeSeconds - lastGroundContactSeconds_ > maxSecondsWithoutGroundContact)
        safeModeReasons += "No ground contact\n";

    if (!safeModeReasons.isEmpty())
    {
        mode_ = SharedTypes::Mode::safe;
        cout << "Spacecraft in safe mode because:" << endl
             << safeModeReasons.toStdString();
    }
    else if (telemetry_.batteryPercent < SharedTypes::degradedEntryBatteryPercent && !telemetry_.isInSunlight && telemetry_.secondsUntilSunrise > minSecondsUntilSunrise)
    {
        if (mode_ == SharedTypes::Mode::degraded)
            return;

        mode_ = SharedTypes::Mode::degraded;
        cout << "Spacecraft in degraded mode because:" << endl
             << "Battery below " << SharedTypes::degradedEntryBatteryPercent << "% and more than 1/3 of the eclipse remaining" << endl;
    }
}

float FlightComputer::TimeIntoOrbit() const
{
    return fmod(telemetry_.missionElapsedTimeSeconds, SharedTypes::orbitPeriodSeconds);
}

void FlightComputer::PayloadCheck()
{
    if (mode_ != SharedTypes::Mode::nominal)
    {
        if (payloadEnabled_)
            payloadEnabled_ = false;

        return;
    }

    if (!payloadEnabled_)
    {
        float timeInOrbit = TimeIntoOrbit();
        if (timeInOrbit >= SharedTypes::payloadStartTime && timeInOrbit <= SharedTypes::payloadEndTime)
        {
            payloadEnabled_ = true;
            cout << "Payload Enabled" << endl;
        }
    }
    else
    {
        float timeInOrbit = TimeIntoOrbit();
        if (timeInOrbit > SharedTypes::payloadEndTime || timeInOrbit < SharedTypes::payloadStartTime)
        {
            payloadEnabled_ = false;
            cout << "Payload Disabled" << endl;
        }
    }
}

void FlightComputer::CommsCheck()
{
    float timeInOrbit = TimeIntoOrbit();

    if (mode_ == SharedTypes::Mode::safe)
    {
        if (!commsTransmitting_)
        {
            float timeIntoBeaconCycle = fmod(telemetry_.missionElapsedTimeSeconds, SharedTypes::beaconPeriodSeconds);
            if (timeIntoBeaconCycle <= SharedTypes::beaconTransmitSeconds)
            {
                commsTransmitting_ = true;
                cout << "Comms Transmitting in Safe Mode (Beacon Mode)" << endl;
            }
        }
        else
        {
            float timeIntoBeaconCycle = fmod(telemetry_.missionElapsedTimeSeconds, SharedTypes::beaconPeriodSeconds);
            if (timeIntoBeaconCycle > SharedTypes::beaconTransmitSeconds)
            {
                commsTransmitting_ = false;
                cout << "Comms Stopped Transmitting in Safe Mode (Beacon Mode)" << endl;
            }
        }
        return;
    }

    if (mode_ == SharedTypes::Mode::degraded)
    {
        float commsQuarterWindow = (SharedTypes::commsEnd - SharedTypes::commsStart) / 4;

        if (!commsTransmitting_)
        {
            if (timeInOrbit >= SharedTypes::commsStart + commsQuarterWindow && timeInOrbit <= SharedTypes::commsEnd - commsQuarterWindow)
            {
                commsTransmitting_ = true;
                cout << "Comms Transmitting in Degraded Mode" << endl;
            }
        }
        else
        {
            if (timeInOrbit < SharedTypes::commsStart + commsQuarterWindow || timeInOrbit > SharedTypes::commsEnd - commsQuarterWindow)
            {
                commsTransmitting_ = false;
                cout << "Comms Stopped Transmitting in Degraded Mode" << endl;
            }
        }
        return;
    }

    if (!commsTransmitting_)
    {
        if (timeInOrbit >= SharedTypes::commsStart && timeInOrbit <= SharedTypes::commsEnd && telemetry_.batteryPercent > minCommsBatteryPercent)
        {
            commsTransmitting_ = true;
            cout << "Comms Transmitting..." << endl;
        }
    }
    else
    {
        if (timeInOrbit > SharedTypes::commsEnd || timeInOrbit < SharedTypes::commsStart)
        {
            commsTransmitting_ = false;
            cout << "Comms Stopped Transmitting" << endl;
        }
    }
}

void FlightComputer::HeaterCheck()
{
    if (!heaterEnabled_ && !telemetry_.isInSunlight && telemetry_.temperatureCelsius <= SharedTypes::heaterOnCelsius)
    {
        heaterEnabled_ = true;
        cout << "Heater commanded on" << endl;
    }
    else if (heaterEnabled_ && (telemetry_.isInSunlight || telemetry_.temperatureCelsius > SharedTypes::heaterOffCelsius))
    {
        heaterEnabled_ = false;
        cout << "Heater commanded off" << endl;
    }
}