#pragma once
#include "SharedTypes.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <optional>

inline const QStringList requiredTelemetryKeys = {
    "missionElapsedTimeSeconds", "batteryPercent", "solarGenerationWatts",
    "powerConsumptionWatts", "temperatureCelsius", "heaterEnabled",
    "radiatorLouversOpen", "isInSunlight", "temperatureSensorHealthy",
    "powerSensorHealthy", "attitudeSensorHealthy", "communicationsAvailable",
    "commsTransmitting", "payloadEnabled", "secondsUntilSunrise", "timeScale"};

inline const QStringList requiredCommandKeys = {
    "mode", "payloadEnabled", "commsTransmitting", "heaterEnabled"};

inline bool HasAllKeys(const QJsonObject &json, const QStringList &requiredKeys)
{
    for (const QString &key : requiredKeys)
        if (!json.contains(key))
            return false;

    return true;
}

inline QJsonObject TelemetryToJsonObject(const SharedTypes::Telemetry &telemetry)
{
    QJsonObject json;
    json["missionElapsedTimeSeconds"] = telemetry.missionElapsedTimeSeconds;
    json["batteryPercent"] = telemetry.batteryPercent;
    json["solarGenerationWatts"] = telemetry.solarGenerationWatts;
    json["powerConsumptionWatts"] = telemetry.powerConsumptionWatts;
    json["temperatureCelsius"] = telemetry.temperatureCelsius;
    json["heaterEnabled"] = telemetry.heaterEnabled;
    json["radiatorLouversOpen"] = telemetry.radiatorLouversOpen;
    json["isInSunlight"] = telemetry.isInSunlight;
    json["temperatureSensorHealthy"] = telemetry.temperatureSensorHealthy;
    json["powerSensorHealthy"] = telemetry.powerSensorHealthy;
    json["attitudeSensorHealthy"] = telemetry.attitudeSensorHealthy;
    json["communicationsAvailable"] = telemetry.communicationsAvailable;
    json["commsTransmitting"] = telemetry.commsTransmitting;
    json["payloadEnabled"] = telemetry.payloadEnabled;
    json["secondsUntilSunrise"] = telemetry.secondsUntilSunrise;
    json["timeScale"] = telemetry.timeScale;

    return json;
}

inline QByteArray TelemetryToJson(const SharedTypes::Telemetry &telemetry)
{
    return QJsonDocument(TelemetryToJsonObject(telemetry)).toJson(QJsonDocument::Compact);
}

inline std::optional<SharedTypes::Telemetry> TelemetryFromJsonObject(const QJsonObject &json)
{
    if (!HasAllKeys(json, requiredTelemetryKeys))
        return std::nullopt;

    SharedTypes::Telemetry telemetry;

    telemetry.missionElapsedTimeSeconds = json["missionElapsedTimeSeconds"].toDouble();
    telemetry.batteryPercent = json["batteryPercent"].toDouble(); // Json only has doubles
    telemetry.solarGenerationWatts = json["solarGenerationWatts"].toDouble();
    telemetry.powerConsumptionWatts = json["powerConsumptionWatts"].toDouble();
    telemetry.temperatureCelsius = json["temperatureCelsius"].toDouble();
    telemetry.heaterEnabled = json["heaterEnabled"].toBool();
    telemetry.radiatorLouversOpen = json["radiatorLouversOpen"].toBool();
    telemetry.isInSunlight = json["isInSunlight"].toBool();
    telemetry.temperatureSensorHealthy = json["temperatureSensorHealthy"].toBool();
    telemetry.powerSensorHealthy = json["powerSensorHealthy"].toBool();
    telemetry.attitudeSensorHealthy = json["attitudeSensorHealthy"].toBool();
    telemetry.communicationsAvailable = json["communicationsAvailable"].toBool();
    telemetry.commsTransmitting = json["commsTransmitting"].toBool();
    telemetry.payloadEnabled = json["payloadEnabled"].toBool();
    telemetry.secondsUntilSunrise = json["secondsUntilSunrise"].toDouble();
    telemetry.timeScale = json["timeScale"].toDouble();

    return telemetry;
}

inline std::optional<SharedTypes::Telemetry> TelemetryFromJson(const QByteArray &payload)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    return TelemetryFromJsonObject(document.object());
}

inline QByteArray CommandsToJson(const SharedTypes::Commands &commands)
{
    QJsonObject json;
    json["mode"] = static_cast<int>(commands.mode);
    json["payloadEnabled"] = commands.payloadEnabled;
    json["commsTransmitting"] = commands.commsTransmitting;
    json["heaterEnabled"] = commands.heaterEnabled;
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

inline std::optional<SharedTypes::Commands> CommandsFromJson(const QByteArray &payload)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    QJsonObject json = document.object();

    if (!HasAllKeys(json, requiredCommandKeys))
        return std::nullopt;

    const int modeNumber = json["mode"].toInt(-1);
    if (modeNumber < 0 || modeNumber > static_cast<int>(SharedTypes::Mode::safe))
        return std::nullopt;

    SharedTypes::Commands commands;
    commands.mode = static_cast<SharedTypes::Mode>(modeNumber);
    commands.payloadEnabled = json["payloadEnabled"].toBool();
    commands.commsTransmitting = json["commsTransmitting"].toBool();
    commands.heaterEnabled = json["heaterEnabled"].toBool();
    return commands;
}