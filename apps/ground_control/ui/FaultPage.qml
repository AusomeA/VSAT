import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundControl

Page {
    id: faultPage

    background: Rectangle {
        color: "black"
    }

    readonly property int shortestSide: Math.min(width, height)
    readonly property int titleFontSize: Math.round(shortestSide * .06)
    readonly property int pageMargin: Math.round(shortestSide * .03)
    readonly property int buttonHeight: Math.round(shortestSide * .1)
    readonly property int buttonFontSize: Math.round(shortestSide * .05)
    readonly property int faultCount: Math.max(1, faultRepeater.count)
    readonly property int adjustCount: adjustRepeater.count
    readonly property int totalRowCount: faultCount + adjustCount
    readonly property int headerFontSize: Math.round(shortestSide * .04)
    readonly property int rowHeight: Math.round(Math.min((height - headerFontSize - titleFontSize - buttonHeight - pageMargin * (totalRowCount + 4)) / totalRowCount, shortestSide * .2))
    readonly property int rowFontSize: Math.round(Math.min(rowHeight * 0.4, switchHeight * 1.2))
    readonly property bool portrait: height > width
    readonly property real labelFraction: portrait ? .35 : .5
    readonly property int labelWidth: Math.round(width * labelFraction)
    readonly property real switchFraction: .12
    readonly property int switchHeight: Math.round(Math.min(rowHeight * .5, width * switchFraction / 2))
    readonly property int switchColumnCount: 2
    readonly property int valueWidth: Math.round((width - pageMargin * 2 - labelWidth - (switchHeight * 2 + pageMargin * 2) * switchColumnCount) / switchColumnCount)
    readonly property int headerWidth: valueWidth + pageMargin + switchHeight * 2

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: faultPage.pageMargin
        spacing: faultPage.pageMargin

        Label {
            text: "Fault Injection"
            color: "green"
            font.pixelSize: faultPage.titleFontSize
            Layout.alignment: Qt.AlignHCenter
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rows: faultPage.faultCount + 1
            flow: GridLayout.TopToBottom
            columnSpacing: faultPage.pageMargin
            rowSpacing: faultPage.pageMargin

            // ───── LEFT COLUMN: header, then one row per sensor (label + value + red switch) ─────

            Label {
                text: "Inject"
                color: "gray"
                font.pixelSize: faultPage.headerFontSize
                fontSizeMode: Text.Fit
                minimumPixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: faultPage.headerWidth
                Layout.alignment: Qt.AlignRight
            }

            Repeater {
                id: faultRepeater
                model: groundControl.faultsModel

                delegate: RowLayout {
                    id: faultRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: faultPage.rowHeight
                    spacing: faultPage.pageMargin

                    required property int index
                    required property string label
                    required property string value
                    required property int status

                    Label {
                        text: faultRow.label
                        color: "white"
                        font.pixelSize: faultPage.rowFontSize
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 8
                        elide: Text.ElideRight
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.preferredWidth: faultPage.labelWidth
                        Layout.fillHeight: true
                    }

                    Label {
                        text: faultRow.value
                        color: root.statusColor(faultRow.status)
                        font.pixelSize: faultPage.rowFontSize
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 8
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        Layout.preferredWidth: faultPage.valueWidth
                        Layout.fillHeight: true
                    }

                    Switch {
                        id: faultSwitch
                        Layout.preferredWidth: faultPage.switchHeight * 2
                        Layout.alignment: Qt.AlignVCenter
                        checked: faultRow.value === "On" || faultRow.value === "Turning On..."
                        onToggled: {
                            groundControl.SetFault(faultRow.index, checked);
                            checked = Qt.binding(function () {
                                return faultRow.value === "On" || faultRow.value === "Turning On...";
                            });
                        }

                        indicator: Rectangle {
                            implicitWidth: faultPage.switchHeight * 1.5
                            implicitHeight: faultPage.switchHeight * 0.75
                            anchors.verticalCenter: parent.verticalCenter
                            radius: height / 2
                            color: faultSwitch.checked ? "red" : "gray"

                            Rectangle {
                                width: parent.height
                                height: parent.height
                                radius: height / 2
                                color: "white"
                                x: faultSwitch.checked ? parent.width - width : 0
                            }
                        }
                    }
                }
            }

            // ───── RIGHT COLUMN: header, then one row per sensor (value + orange switch, no label) ─────

            Label {
                text: "Inhibit"
                color: "gray"
                font.pixelSize: faultPage.headerFontSize
                fontSizeMode: Text.Fit
                minimumPixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Layout.preferredWidth: faultPage.headerWidth
                Layout.alignment: Qt.AlignLeft
            }

            Repeater {
                id: inhibitRepeater
                model: groundControl.inhibitsModel

                delegate: RowLayout {
                    id: inhibitRow
                    Layout.preferredHeight: faultPage.rowHeight
                    spacing: faultPage.pageMargin

                    required property int index
                    required property string value
                    required property int status

                    Label {
                        text: inhibitRow.value
                        color: root.statusColor(inhibitRow.status)
                        font.pixelSize: faultPage.rowFontSize
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 8
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        Layout.preferredWidth: faultPage.valueWidth
                        Layout.fillHeight: true
                    }

                    Switch {
                        id: inhibitSwitch
                        Layout.preferredWidth: faultPage.switchHeight * 2
                        Layout.alignment: Qt.AlignVCenter
                        checked: inhibitRow.value === "On" || inhibitRow.value === "Turning On..."
                        onToggled: {
                            groundControl.SetInhibits(inhibitRow.index, checked);
                            checked = Qt.binding(function () {
                                return inhibitRow.value === "On" || inhibitRow.value === "Turning On...";
                            });
                        }

                        indicator: Rectangle {
                            implicitWidth: faultPage.switchHeight * 1.5
                            implicitHeight: faultPage.switchHeight * 0.75
                            anchors.verticalCenter: parent.verticalCenter
                            radius: height / 2
                            color: inhibitSwitch.checked ? "orange" : "gray"

                            Rectangle {
                                width: parent.height
                                height: parent.height
                                radius: height / 2
                                color: "white"
                                x: inhibitSwitch.checked ? parent.width - width : 0
                            }
                        }
                    }
                }
            }
        }

        Repeater {
            id: adjustRepeater
            model: groundControl.adjustsModel

            delegate: RowLayout {
                id: adjustRow
                Layout.fillWidth: true
                Layout.preferredHeight: faultPage.rowHeight
                spacing: faultPage.pageMargin

                required property int index
                required property string label
                required property string value
                required property int status

                Label {
                    text: adjustRow.label
                    color: "white"
                    font.pixelSize: faultPage.rowFontSize
                    fontSizeMode: Text.Fit
                    minimumPixelSize: 8
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: faultPage.labelWidth
                    Layout.fillHeight: true
                }

                Label {
                    text: adjustRow.value
                    color: root.statusColor(adjustRow.status)
                    font.pixelSize: faultPage.rowFontSize
                    fontSizeMode: Text.Fit
                    minimumPixelSize: 8
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: faultPage.valueWidth
                    Layout.fillHeight: true
                }

                Button {
                    text: "-"
                    font.pixelSize: faultPage.rowFontSize
                    Layout.preferredWidth: faultPage.switchHeight * 2
                    Layout.preferredHeight: faultPage.rowHeight * 0.8
                    onClicked: groundControl.SendAdjust(adjustRow.index, false)
                }

                Button {
                    text: "+"
                    font.pixelSize: faultPage.rowFontSize
                    Layout.preferredWidth: faultPage.switchHeight * 2
                    Layout.preferredHeight: faultPage.rowHeight * 0.8
                    onClicked: groundControl.SendAdjust(adjustRow.index, true)
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        Button {
            text: "Back"
            Layout.preferredWidth: faultPage.buttonHeight * 3
            Layout.preferredHeight: faultPage.buttonHeight * 2
            font.pixelSize: faultPage.buttonFontSize
            onClicked: faultPage.StackView.view.pop()
        }
    }
}
