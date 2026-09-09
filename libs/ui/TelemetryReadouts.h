#pragma once

#include "SharedTypes.h"
#include "ReadoutsModel.h"
#include "Helpers.h"
#include "ReadoutFormatting.h"
#include "Helpers.h"

enum TelemetryReadoutRow
{
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
    telemetryReadoutRowCount
};

inline QVector<ReadoutRow> TelemetryReadouts()
{
    return {
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
        {"Payload Enabled", "", 0}
    };
};

inline QString ModeText(SharedTypes::Mode mode)
{
    return mode == SharedTypes::Mode::nominal  ? "Nominal"
           : mode == SharedTypes::Mode::degraded ? "Degraded"
                                                 : "Safe";
}

inline SharedTypes::Status GetModeStatus(SharedTypes::Mode mode)
{
    return mode == SharedTypes::Mode::nominal  ? SharedTypes::Status::good
           : mode == SharedTypes::Mode::degraded ? SharedTypes::Status::warning
                                                 : SharedTypes::Status::critical;
}

inline void UpdateTelemetryReadouts(ReadoutsModel &model, const SharedTypes::Telemetry &telemetry, int firstRow, bool stale)
{
    auto rowStatus = [stale] (SharedTypes::Status status)                           // helper lambda
    { return static_cast<int>(stale ? SharedTypes::Status::stale : status);};
    
    model.UpdateRow(firstRow + METRow, MissionElapsedTimeText(telemetry.missionElapsedTimeSeconds), rowStatus(SharedTypes::Status::none));
    model.UpdateRow(firstRow + batteryRow, QString("%1 %").arg(telemetry.batteryPercent, 0, 'f', 1), rowStatus(GetBatteryStatus(telemetry.batteryPercent)));
    model.UpdateRow(firstRow + solarGenerationRow, QString("%1 W").arg(telemetry.solarGenerationWatts, 0, 'f', 1), rowStatus(GetSolarGenerationStatus(telemetry.solarGenerationWatts, telemetry.isInSunlight)));
    model.UpdateRow(firstRow + powerConsumptionRow, QString("%1 W").arg(telemetry.powerConsumptionWatts, 0, 'f', 1), rowStatus(GetPowerConsumptionStatus(telemetry)));
    model.UpdateRow(firstRow + temperatureRow, QString("%1 C").arg(telemetry.temperatureCelsius, 0, 'f', 1), rowStatus(GetTemperatureStatus(telemetry.temperatureCelsius)));
    model.UpdateRow(firstRow + heaterRow, telemetry.heaterEnabled ? "On" : "Off", rowStatus(SharedTypes::Status::none));
    model.UpdateRow(firstRow + radiatorRow, telemetry.radiatorLouversOpen ? "Open" : "Shut", rowStatus(SharedTypes::Status::none));
    model.UpdateRow(firstRow + sunlightRow, telemetry.isInSunlight ? "Yes" : "No", rowStatus(telemetry.isInSunlight ? SharedTypes::Status::good : SharedTypes::Status::none));
    model.UpdateRow(firstRow + commsRow, telemetry.communicationsAvailable ? "Yes" : "No", rowStatus(telemetry.communicationsAvailable ? SharedTypes::Status::good : SharedTypes::Status::none));
    model.UpdateRow(firstRow + commsTransmittingRow, telemetry.commsTransmitting ? "Yes" : "No", rowStatus(telemetry.commsTransmitting ? SharedTypes::Status::good : SharedTypes::Status::none));
    model.UpdateRow(firstRow + temperatureSensorRow, telemetry.temperatureSensorHealthy ? "Yes" : "No", rowStatus(telemetry.temperatureSensorHealthy ? SharedTypes::Status::good : SharedTypes::Status::critical));
    model.UpdateRow(firstRow + powerSensorRow, telemetry.powerSensorHealthy ? "Yes" : "No", rowStatus(telemetry.powerSensorHealthy ? SharedTypes::Status::good : SharedTypes::Status::critical));
    model.UpdateRow(firstRow + attitudeSensorRow, telemetry.attitudeSensorHealthy ? "Yes" : "No", rowStatus(telemetry.attitudeSensorHealthy ? SharedTypes::Status::good : SharedTypes::Status::critical));
    model.UpdateRow(firstRow + payloadRow, telemetry.payloadEnabled ? "Yes" : "No", rowStatus(telemetry.payloadEnabled ? SharedTypes::Status::good : SharedTypes::Status::none));
}