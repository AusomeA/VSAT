#include <gtest/gtest.h>
#include "FlightComputer.h"
#include "TelemetryJson.h"

SharedTypes::Telemetry NominalTelemetry()
{
    SharedTypes::Telemetry telemetry;
    telemetry.missionElapsedTimeSeconds = 3000.0; // one full orbit
    telemetry.batteryPercent = 80.f;
    telemetry.solarGenerationWatts = 40.f;
    telemetry.powerConsumptionWatts = 13.f;
    telemetry.temperatureCelsius = 30.f;
    telemetry.isInSunlight = true;
    telemetry.temperatureSensorHealthy = true;
    telemetry.powerSensorHealthy = true;
    telemetry.attitudeSensorHealthy = true;
    telemetry.secondsUntilSunrise = 0.f;
    return telemetry;
}

TEST(Sanity, HarnessWorks)
{
    EXPECT_EQ(1 + 1, 2);
}

TEST(ModeCheck, DegradedToSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = SharedTypes::degradedEntryBatteryPercent - 1;
    telemetry.isInSunlight = false;
    telemetry.missionElapsedTimeSeconds = 300.0;
    telemetry.secondsUntilSunrise = 2100.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::degraded);

    telemetry.batteryPercent = 29.f;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, BatterySafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = 29.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, TemperatureSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.temperatureSensorHealthy = false;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, PowerSensorSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.powerSensorHealthy = false;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, AttitudeSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.attitudeSensorHealthy = false;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, LowBatInSunlightNominalCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = 45.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);
}

TEST(ModeCheck, LowBatAlmostSunriseNominalCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = 45.f;
    telemetry.isInSunlight = false;
    telemetry.secondsUntilSunrise = 100.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);
}

TEST(ModeCheck, DegradedToDegradedBackToNominalCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    // puts it degraded mode
    telemetry.batteryPercent = SharedTypes::degradedEntryBatteryPercent - 1;
    telemetry.isInSunlight = false;
    telemetry.missionElapsedTimeSeconds = 300.0;
    telemetry.secondsUntilSunrise = 2100.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::degraded);

    // makes sure it stays in degraded mode
    telemetry.batteryPercent = 55.f;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::degraded);

    // makes sure it returns to nominal
    telemetry.batteryPercent = 61.f;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);
}

TEST(ModeCheck, SafeContinuedToSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = 29.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);

    telemetry.batteryPercent = 100.f;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, GroundContactSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);

    telemetry.missionElapsedTimeSeconds = 13900.0;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, ExitSafeModeWhenClearCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    EXPECT_FALSE(flightComputer.RequestExitSafeMode());

    telemetry.batteryPercent = SharedTypes::safeEntryBatteryPercent - 1.f;
    flightComputer.Update(telemetry);
    telemetry.batteryPercent = 100.f;
    flightComputer.Update(telemetry);

    EXPECT_TRUE(flightComputer.RequestExitSafeMode());
    EXPECT_EQ(flightComputer.GetMode(), SharedTypes::Mode::nominal);
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);
}

TEST(ModeCheck, ExitSafeModeRefusalCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.temperatureSensorHealthy = false;
    flightComputer.Update(telemetry);

    EXPECT_FALSE(flightComputer.RequestExitSafeMode());
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);

    telemetry.temperatureSensorHealthy = true;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, RebootWithFaultActiveCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.temperatureSensorHealthy = false;
    flightComputer.Update(telemetry);
    EXPECT_EQ(flightComputer.GetMode(), SharedTypes::Mode::safe);

    EXPECT_TRUE(flightComputer.Reboot());
    EXPECT_EQ(flightComputer.GetMode(), SharedTypes::Mode::nominal);

    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, InhibitedSensorCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    EXPECT_FALSE(flightComputer.SetFaultInhibited("Not a real fault", true));
    EXPECT_TRUE(flightComputer.SetFaultInhibited(SharedTypes::temperatureSensorFaultMessage, true));

    telemetry.temperatureSensorHealthy = false;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);

    EXPECT_TRUE(flightComputer.SetFaultInhibited(SharedTypes::temperatureSensorFaultMessage, false));
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, ExitSafeModeWithInhibitorCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.temperatureSensorHealthy = false;
    flightComputer.Update(telemetry);
    EXPECT_FALSE(flightComputer.RequestExitSafeMode());

    flightComputer.SetFaultInhibited(SharedTypes::temperatureSensorFaultMessage, true);
    EXPECT_TRUE(flightComputer.RequestExitSafeMode());
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::nominal);
}

TEST(ModeCheck, RebootClearsInhibitCheck)                   
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    flightComputer.SetFaultInhibited(SharedTypes::temperatureSensorFaultMessage, true);
    telemetry.temperatureSensorHealthy = false;
    flightComputer.Update(telemetry);
    EXPECT_EQ(flightComputer.GetMode(), SharedTypes::Mode::nominal);

    flightComputer.Reboot();
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
}

TEST(ModeCheck, ForceSafeModeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();
    flightComputer.Update(telemetry);
    EXPECT_EQ(flightComputer.GetMode(), SharedTypes::Mode::nominal);

    EXPECT_TRUE(flightComputer.ForceSafeMode());
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);

    EXPECT_TRUE(flightComputer.RequestExitSafeMode());
    EXPECT_EQ(flightComputer.GetMode(), SharedTypes::Mode::nominal);
}

TEST(PayloadCheck, PayloadOnOffCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.missionElapsedTimeSeconds = SharedTypes::payloadStartTime - 1;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.payloadEnabled, false);

    telemetry.missionElapsedTimeSeconds = SharedTypes::payloadStartTime + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.payloadEnabled, true);

    telemetry.missionElapsedTimeSeconds = SharedTypes::payloadEndTime - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.payloadEnabled, true);

    telemetry.missionElapsedTimeSeconds = SharedTypes::payloadEndTime + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.payloadEnabled, false);
}

TEST(PayloadCheck, PayloadOffDuringDegradedSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = SharedTypes::degradedEntryBatteryPercent - 1;
    telemetry.isInSunlight = false;
    telemetry.missionElapsedTimeSeconds = SharedTypes::payloadStartTime + 1;
    telemetry.secondsUntilSunrise = 2100.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.payloadEnabled, false);

    telemetry.batteryPercent = SharedTypes::safeEntryBatteryPercent - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.payloadEnabled, false);
}

TEST(PayloadCheck, PayloadOffDuringSafeCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.missionElapsedTimeSeconds = SharedTypes::payloadStartTime + 1;
    telemetry.isInSunlight = false;
    telemetry.secondsUntilSunrise = 1499.f;
    telemetry.attitudeSensorHealthy = false;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);

    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);
    EXPECT_EQ(commands.payloadEnabled, false);
}

TEST(PayloadCheck, PayloadInhibitCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();
    telemetry.missionElapsedTimeSeconds = 1000.0;

    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_TRUE(commands.payloadEnabled);

    EXPECT_TRUE(flightComputer.SetPayloadInhibited(true));
    commands = flightComputer.Update(telemetry);
    EXPECT_FALSE(commands.payloadEnabled);

    EXPECT_TRUE(flightComputer.SetPayloadInhibited(false));
    commands = flightComputer.Update(telemetry);
    EXPECT_TRUE(commands.payloadEnabled);

    EXPECT_TRUE(flightComputer.SetPayloadInhibited(true));
    flightComputer.Reboot();
    commands = flightComputer.Update(telemetry);
    EXPECT_TRUE(commands.payloadEnabled);
}

TEST(CommsCheck, CommsTransmittingCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsStart - 1;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, false);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsStart + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, true);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsEnd - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, true);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsEnd + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, false);
}

TEST(CommsCheck, CommsTransmittingDegradedCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();
    float commsQuarterWindow = (SharedTypes::commsEnd - SharedTypes::commsStart) / 4;

    // puts it degraded mode
    telemetry.batteryPercent = SharedTypes::degradedEntryBatteryPercent - 1;
    telemetry.isInSunlight = false;
    telemetry.missionElapsedTimeSeconds = 300.0;
    telemetry.secondsUntilSunrise = 2100.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::degraded);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsStart + commsQuarterWindow - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, false);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsStart + commsQuarterWindow + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, true);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsEnd - commsQuarterWindow - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, true);

    telemetry.missionElapsedTimeSeconds = SharedTypes::commsEnd - commsQuarterWindow + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, false);
}

TEST(CommsCheck, CommsTransmittingBeaconCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.batteryPercent = 10;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.mode, SharedTypes::Mode::safe);

    telemetry.missionElapsedTimeSeconds += SharedTypes::beaconPeriodSeconds - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, false);

    telemetry.missionElapsedTimeSeconds += 2;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, true);

    telemetry.missionElapsedTimeSeconds += SharedTypes::beaconTransmitSeconds - 2;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, true);

    telemetry.missionElapsedTimeSeconds += 2;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.commsTransmitting, false);
}

TEST(HeaterCheck, HeaterOnOffCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    float inBetweenCelsius = (SharedTypes::heaterOnCelsius + SharedTypes::heaterOffCelsius) / 2;

    telemetry.isInSunlight = false;

    telemetry.temperatureCelsius = inBetweenCelsius;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, false);

    telemetry.temperatureCelsius = SharedTypes::heaterOnCelsius + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, false);

    telemetry.temperatureCelsius = SharedTypes::heaterOnCelsius - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, true);

    telemetry.temperatureCelsius = inBetweenCelsius;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, true);

    telemetry.temperatureCelsius = SharedTypes::heaterOffCelsius - 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, true);

    telemetry.temperatureCelsius = SharedTypes::heaterOffCelsius + 1;
    commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, false);
}

TEST(HeaterCheck, HeaterSunlightCheck)
{
    FlightComputer flightComputer;
    SharedTypes::Telemetry telemetry = NominalTelemetry();

    telemetry.isInSunlight = true;

    telemetry.temperatureCelsius = 10.f;
    SharedTypes::Commands commands = flightComputer.Update(telemetry);
    EXPECT_EQ(commands.heaterEnabled, false);
}

TEST(TelemetryJson, TelemetryRoundTripCheck)
{
    SharedTypes::Telemetry telemetry;
    telemetry.missionElapsedTimeSeconds = 4321.5;
    telemetry.batteryPercent = 54.3f;
    telemetry.solarGenerationWatts = 99.f;
    telemetry.powerConsumptionWatts = 101.f;
    telemetry.temperatureCelsius = -12.25f;
    telemetry.heaterEnabled = true;
    telemetry.radiatorLouversOpen = true;
    telemetry.isInSunlight = true;
    telemetry.temperatureSensorHealthy = false;
    telemetry.powerSensorHealthy = false;
    telemetry.attitudeSensorHealthy = false;
    telemetry.communicationsAvailable = true;
    telemetry.commsTransmitting = true;
    telemetry.payloadEnabled = true;
    telemetry.secondsUntilSunrise = 9500.1f;

    std::optional<SharedTypes::Telemetry> result = TelemetryFromJson(TelemetryToJson(telemetry));

    ASSERT_TRUE(result.has_value()); // if unpacking failed, stop here

    EXPECT_DOUBLE_EQ(result->missionElapsedTimeSeconds, 4321.5);
    EXPECT_FLOAT_EQ(result->batteryPercent, 54.3f);
    EXPECT_FLOAT_EQ(result->solarGenerationWatts, 99.f);
    EXPECT_FLOAT_EQ(result->powerConsumptionWatts, 101.f);
    EXPECT_FLOAT_EQ(result->temperatureCelsius, -12.25f);
    EXPECT_EQ(result->heaterEnabled, true);
    EXPECT_EQ(result->radiatorLouversOpen, true);
    EXPECT_EQ(result->isInSunlight, true);
    EXPECT_EQ(result->temperatureSensorHealthy, false);
    EXPECT_EQ(result->powerSensorHealthy, false);
    EXPECT_EQ(result->attitudeSensorHealthy, false);
    EXPECT_EQ(result->communicationsAvailable, true);
    EXPECT_EQ(result->commsTransmitting, true);
    EXPECT_EQ(result->payloadEnabled, true);
    EXPECT_FLOAT_EQ(result->secondsUntilSunrise, 9500.1f);
}

TEST(TelemetryJson, CommandsRoundTripCheck)
{
    SharedTypes::Commands commands;
    commands.mode = SharedTypes::Mode::degraded;
    commands.payloadEnabled = true;
    commands.commsTransmitting = true;
    commands.heaterEnabled = true;

    std::optional<SharedTypes::Commands> results = CommandsFromJson(CommandsToJson(commands));

    ASSERT_TRUE(results.has_value()); // if unpacking failed, stop here

    EXPECT_EQ(results->mode, SharedTypes::Mode::degraded);
    EXPECT_EQ(results->payloadEnabled, true);
    EXPECT_EQ(results->commsTransmitting, true);
    EXPECT_EQ(results->heaterEnabled, true);
}

TEST(TelemetryJson, RejectsGarbageCheck)
{
    std::optional<SharedTypes::Telemetry> result = TelemetryFromJson("This is not a json");

    EXPECT_FALSE(result.has_value());
}

TEST(TelemetryJson, RejectsMissingKey)
{
    SharedTypes::Telemetry telemetry;
    telemetry.missionElapsedTimeSeconds = 4321.5;
    telemetry.batteryPercent = 54.3f;
    telemetry.solarGenerationWatts = 99.f;
    telemetry.powerConsumptionWatts = 101.f;
    telemetry.temperatureCelsius = -12.25f;
    telemetry.heaterEnabled = true;
    telemetry.radiatorLouversOpen = true;
    telemetry.isInSunlight = true;
    telemetry.temperatureSensorHealthy = false;
    telemetry.powerSensorHealthy = false;
    telemetry.attitudeSensorHealthy = false;
    telemetry.communicationsAvailable = true;
    telemetry.commsTransmitting = true;
    telemetry.payloadEnabled = true;
    telemetry.secondsUntilSunrise = 9500.1f;

    QJsonObject json = QJsonDocument::fromJson(TelemetryToJson(telemetry)).object();
    json.remove("batteryPercent");
    QByteArray brokenPayload = QJsonDocument(json).toJson();

    std::optional<SharedTypes::Telemetry> result = TelemetryFromJson(brokenPayload);

    EXPECT_FALSE(result.has_value());
}

TEST(TelemetryJson, RejectsBadModeCheck)
{
    SharedTypes::Commands commands;
    commands.mode = SharedTypes::Mode::degraded;
    commands.payloadEnabled = true;
    commands.commsTransmitting = true;
    commands.heaterEnabled = true;

    QJsonObject json = QJsonDocument::fromJson(CommandsToJson(commands)).object();
    json["mode"] = 25;
    QByteArray brokenPayload = QJsonDocument(json).toJson();

    std::optional<SharedTypes::Commands> result = CommandsFromJson(brokenPayload);

    EXPECT_FALSE(result.has_value());

    json["mode"] = -1;
    brokenPayload = QJsonDocument(json).toJson();
    result = CommandsFromJson(brokenPayload);

    EXPECT_FALSE(result.has_value());
}
