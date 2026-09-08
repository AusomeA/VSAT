import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import GroundControl

Page {
    id: statsPage

    background: Rectangle {
        color: "black"
    }

    readonly property bool portrait: height > width
    readonly property int shortestSide: Math.min(width, height)
    readonly property int columnCount: portrait ? 1 : 2
    readonly property int readoutRowCount: Math.max(1, Math.ceil(readoutRepeater.count / columnCount))
    readonly property int rowHeight: Math.floor(readoutGrid.height / readoutRowCount)
    readonly property int baseFontSize: Math.round(rowHeight * 0.45)
    readonly property int cellPadding: Math.round(baseFontSize * 0.5)
    readonly property int buttonHeight: Math.round(shortestSide * 0.1)
    readonly property int buttonFontSize: Math.round(shortestSide * 0.05)
    readonly property real labelFraction: 0.7

    Grid {
        id: readoutGrid
        anchors.fill: parent
        anchors.margins: 10
        anchors.bottomMargin: statsPage.buttonHeight + 40
        columns: statsPage.columnCount
        rowSpacing: 0
        columnSpacing: 10

        Repeater {
            id: readoutRepeater
            model: groundControl.readoutsModel

            delegate: RowLayout {
                id: readoutRow
                width: (readoutGrid.width - readoutGrid.columnSpacing * (readoutGrid.columns - 1)) / readoutGrid.columns
                spacing: 0
                required property string label
                required property string value
                required property int status

                Rectangle {
                    Layout.preferredWidth: readoutRow.width * statsPage.labelFraction
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
                        color: root.statusColor(readoutRow.status)
                    }
                }
            }
        }
    }

    Button {
        text: "Back"
        width: statsPage.buttonHeight * 3
        height: statsPage.buttonHeight * 2
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 20
        font.pixelSize: statsPage.buttonFontSize
        onClicked: statsPage.StackView.view.pop()
    }
}
