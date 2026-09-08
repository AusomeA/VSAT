import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import FlightComputer

ApplicationWindow {
    id: root

    FlightComputerShell {
        id: flightComputer
    }

    function statusColor(status) {
        switch (status) {
        case SharedTypes.Status.good:
            return "green";
        case SharedTypes.Status.warning:
            return "yellow";
        case SharedTypes.Status.critical:
            return "red";
        case SharedTypes.Status.stale:
            return "steelblue";
        default:
            return "white"; // none / unknown
        }
    }

    function toggleFullScreen() {
        root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
    }

    visible: true
    visibility: Window.FullScreen
    width: Screen.width / 2
    height: Screen.height - 20

    x: 0
    y: 0

    title: "Flight Computer"

    color: "black"

    readonly property int readoutRowCount: Math.max(1, Math.ceil(readoutRepeater.count / 2))
    readonly property int rowHeight: Math.floor(readoutGrid.height / readoutRowCount)
    readonly property int baseFontSize: Math.round(rowHeight * 0.45)
    readonly property int cellPadding: Math.round(baseFontSize * 0.5)
    readonly property real labelFraction: 0.7

    MouseArea {
        width: 80
        height: 80
        anchors.top: parent.top
        anchors.right: parent.right
        onPressAndHold: Qt.quit()    // fires after holding ~0.8 s
    }

    MouseArea {
        width: 80
        height: 80
        anchors.top: parent.top
        anchors.left: parent.left
        onPressAndHold: root.toggleFullScreen()
    }

    // Esc Key exits full screen mode and returns to windowed mode, or vice versa
    Shortcut {
        sequence: "Esc"
        onActivated: root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
    }

    Grid {
        id: readoutGrid
        anchors.fill: parent
        anchors.margins: 10
        columns: 2
        rowSpacing: 0
        columnSpacing: 10

        Repeater {
            id: readoutRepeater
            model: flightComputer.readoutsModel

            delegate: RowLayout {
                id: readoutRow
                width: (readoutGrid.width - readoutGrid.columnSpacing) / readoutGrid.columns
                spacing: 0
                required property string label
                required property string value
                required property int status

                Rectangle {
                    Layout.preferredWidth: readoutRow.width * root.labelFraction
                    Layout.preferredHeight: rowHeight
                    color: "black"
                    border.color: "white"
                    border.width: 1

                    Label {
                        anchors.fill: parent
                        anchors.margins: cellPadding
                        verticalAlignment: Label.AlignVCenter
                        horizontalAlignment: Label.AlignHCenter
                        elide: Text.ElideRight
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 8

                        text: readoutRow.label

                        font.pixelSize: baseFontSize
                        color: "white"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: rowHeight
                    color: "black"
                    border.color: "white"
                    border.width: 1

                    Label {

                        anchors.fill: parent
                        anchors.margins: cellPadding
                        verticalAlignment: Label.AlignVCenter
                        horizontalAlignment: Label.AlignHCenter
                        elide: Text.ElideRight
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 8

                        text: readoutRow.value

                        font.pixelSize: baseFontSize
                        color: statusColor(readoutRow.status)
                    }
                }
            }
        }
    }
}
