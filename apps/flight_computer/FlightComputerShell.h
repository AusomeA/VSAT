#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QElapsedTimer>
#include <QUdpSocket>
#include <QHostAddress>
#include "FlightComputer.h"
#include "ReadoutsModel.h"
#include "UdpReceiver.h"
#include "Discovery.h"
#include "TelemetryReadouts.h"

enum FCReadoutRowIndex
{
    linkRow,
    modeRow,
    lastGroundContactRow,
    fcHeaderRowCount
};

enum FCFooterRowIndex
{
    lastCommandRow,
    fcFooterRowCount
};

class FlightComputerShell : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FlightComputerShell)
    Q_PROPERTY(QAbstractItemModel *readoutsModel READ ReadoutsModelPtr CONSTANT)
    Q_PROPERTY(QAbstractItemModel *footerModel READ FooterModelPtr CONSTANT)

public:
    FlightComputerShell(QObject *parent = nullptr);

    QAbstractItemModel *ReadoutsModelPtr() { return &readoutsModel_; }
    QAbstractItemModel *FooterModelPtr() { return &footerModel_; }

private:
    FlightComputer flightComputer_;
    ReadoutsModel readoutsModel_;
    ReadoutsModel footerModel_;

    UdpReceiver receiver_;
    UdpReceiver groundCommandReceiver_;
    QUdpSocket commandSocket_;
    QUdpSocket groundTelemetrySocket_;
    QHostAddress simulatorAddress_;
    qint64 groundSequence_ = 0;

    Discovery discovery_{SharedTypes::flightComputerName, SharedTypes::defaultVehicleName};

    QTimer linkCheckTimer_;
    QElapsedTimer timeSinceLastPacket_;

    QTimer bootTimer_;
    bool booting_ = false;
    bool groundContact_ = false;

    static constexpr int linkCheckIntervalMilliseconds = 200;
    static constexpr int bootDurationMilliseconds = 5000; // how long it takes the fc to boot

    void HandleTelemetry(const QByteArray &payload);
    void HandleGroundCommand(const QByteArray &payload, const QHostAddress &senderAddress, quint16 senderPort);
    void UpdateLinkRow();
    void SendGroundTelemetry(bool simLinkOk); // Have to update seperate in case packet does not arrive

    void PopulateRows();
    void UpdateRows(bool stale = false);

    void StartBoot();
    void FinishBoot();
};