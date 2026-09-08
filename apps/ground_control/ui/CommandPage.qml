import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundControl

Page {
    id: commandPage

    background: Rectangle {
        color: "black"
    }

    readonly property int shortestSide: Math.min(width, height)
    readonly property int titleFontSize: Math.round(shortestSide * .06)
    readonly property int pageMargin: Math.round(shortestSide * .03)
    readonly property int buttonHeight: Math.round(shortestSide * 0.1)
    readonly property int buttonFontSize: Math.round(shortestSide * 0.05)
    readonly property int commandCount: Math.max(1, commandRepeater.count)
    readonly property int rowHeight: Math.round(Math.min((height - titleFontSize - buttonHeight - pageMargin * (commandCount + 3)) / commandCount, shortestSide * .2))
    readonly property int rowFontSize: Math.round(Math.min(rowHeight * 0.4, shortestSide * .1))

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: commandPage.pageMargin
        spacing: commandPage.pageMargin

        Label {
            text: "Commands"
            color: "green"
            font.pixelSize: commandPage.titleFontSize
            Layout.alignment: Qt.AlignHCenter
        }

        Repeater {
            id: commandRepeater
            model: groundControl.commandsModel

            delegate: RowLayout {
                id: commandRow
                Layout.fillWidth: true
                Layout.preferredHeight: commandPage.rowHeight
                spacing: commandPage.pageMargin * 2

                required property int index
                required property string label
                required property string value
                required property int status

                Button {
                    id: commandButton
                    text: commandRow.label
                    font.pixelSize: commandPage.rowFontSize
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onClicked: groundControl.SendCommand(commandRow.index)

                    contentItem: Text {
                        text: commandButton.text
                        font: commandButton.font
                        color: "black"
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 8
                        elide: Text.ElideRight
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Label {
                    text: commandRow.value
                    color: root.statusColor(commandRow.status)
                    font.pixelSize: commandPage.rowFontSize
                    fontSizeMode: Text.Fit
                    minimumPixelSize: 8
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    Layout.preferredWidth: commandPage.width * 0.35
                    Layout.fillHeight: true
                }
            }
        }

        Item { Layout.fillHeight: true}

        Button {
            text: "Back"
            Layout.preferredWidth: commandPage.buttonHeight * 3
            Layout.preferredHeight: commandPage.buttonHeight * 2
            font.pixelSize: commandPage.buttonFontSize
            onClicked: commandPage.StackView.view.pop()
        }
    }
}
