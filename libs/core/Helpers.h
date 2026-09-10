#pragma once
#include <SharedTypes.h>
#include <cmath>

inline constexpr float minTemperatureGoodCelsius = 10.f;    // what temp is still good (minimum)
inline constexpr float maxTemperatureGoodCelsius = 35.f;    // what temp is still good (maximum)
inline constexpr float minTemperatureWarningCelsius = 5.f;  // what temp is still just a warning (minimum)
inline constexpr float maxTemperatureWarningCelsius = 36.f; // what temp is still just a warning (maximum)

inline SharedTypes::Status GetBatteryStatus(float batteryPercentage)
{
    if (batteryPercentage > SharedTypes::degradedEntryBatteryPercent)
        return SharedTypes::Status::good;
    else if (batteryPercentage > SharedTypes::safeEntryBatteryPercent)
        return SharedTypes::Status::warning;
    else
        return SharedTypes::Status::critical;
}

inline SharedTypes::Status GetSolarGenerationStatus(float generationWatts, bool isInSunlight)
{
    if (!isInSunlight)
        return SharedTypes::Status::none;

    if (generationWatts > 30.f)
        return SharedTypes::Status::good;
    else if (generationWatts > 10.f)
        return SharedTypes::Status::warning;
    else
        return SharedTypes::Status::critical;
}

inline SharedTypes::Status GetTemperatureStatus(float temperatureCelsius)
{
    if (temperatureCelsius <= maxTemperatureGoodCelsius && temperatureCelsius >= minTemperatureGoodCelsius)
        return SharedTypes::Status::good;
    else if (temperatureCelsius <= maxTemperatureWarningCelsius && temperatureCelsius >= minTemperatureWarningCelsius)
        return SharedTypes::Status::warning;
    else
        return SharedTypes::Status::critical;
}

inline float ExpectedPowerWatts(const SharedTypes::Telemetry &telemetry)
{
    return SharedTypes::basePowerWatts + SharedTypes::avionicsPowerWatts + (telemetry.payloadEnabled ? SharedTypes::payloadPowerWatts : 0.f) + (telemetry.commsTransmitting ? SharedTypes::commsPowerWatts : 0.f) + (telemetry.heaterEnabled ? SharedTypes::heaterPowerWatts : 0.f);
}

inline SharedTypes::Status GetPowerConsumptionStatus(const SharedTypes::Telemetry &telemetry)
{
    const float excessWatts = telemetry.powerConsumptionWatts - ExpectedPowerWatts(telemetry);

    if (telemetry.powerConsumptionWatts > SharedTypes::maxPowerWatts + SharedTypes::powerMarginWatts)
        return SharedTypes::Status::critical;
    else if (excessWatts > SharedTypes::powerMarginWatts)
        return SharedTypes::Status::warning;
    else
        return SharedTypes::Status::good;
}

inline float SecondsUntilNextContact(double METSeconds)
{
    const float timeIntoOrbit = static_cast<float>(fmod(METSeconds, SharedTypes::orbitPeriodSeconds));

    if (timeIntoOrbit >= SharedTypes::commsStart && timeIntoOrbit < SharedTypes::commsEnd)
        return 0.f;

    if (timeIntoOrbit < SharedTypes::commsStart)
        return SharedTypes::commsStart - timeIntoOrbit;

    return SharedTypes::orbitPeriodSeconds - timeIntoOrbit + SharedTypes::commsStart;
}

inline double NextContactStartMET(double METSeconds)
{
    const double orbitStartMET = METSeconds - fmod(METSeconds, SharedTypes::orbitPeriodSeconds);
    const double thisOrbitWindowStartMET = orbitStartMET + SharedTypes::commsStart;

    return METSeconds < thisOrbitWindowStartMET ? thisOrbitWindowStartMET : thisOrbitWindowStartMET + SharedTypes::orbitPeriodSeconds;
}