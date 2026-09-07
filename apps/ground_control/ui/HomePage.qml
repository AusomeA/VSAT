import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: homePage
    background: Rectangle {
        color: "black"
    }

    readonly property int shortestSide: Math.min(width, height)
    readonly property int titleFontSize: Math.round(shortestSide * .06)
    readonly property int baseFontSize: Math.round(shortestSide * 0.035)
    readonly property int buttonFontSize: Math.round(shortestSide * 0.06)
    readonly property int pageMargin: Math.round(shortestSide * .03)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: homePage.pageMargin
        spacing: homePage.pageMargin

        Label {
            text: "Ground Control"
            color: "green"
            font.pixelSize: homePage.titleFontSize
            Layout.alignment: Qt.AlignHCenter
        }

        Flow {
            Layout.fillWidth: true
            spacing: homePage.pageMargin

            Label {
                text: "FC Link: " + (groundControl.flightComputerLinked ? "Linked" : "Not Linked")
                color: groundControl.flightComputerLinked ? "green" : "red"
                font.pixelSize: homePage.baseFontSize
            }
            Label {
                text: "Sim Link: " + (groundControl.simulatorLinked ? "Linked" : "Not Linked")
                color: groundControl.simulatorLinked ? "green" : "red"
                font.pixelSize: homePage.baseFontSize
            }
            Label {
                text: "Mode: " + groundControl.modeText
                color: root.statusColor(groundControl.modeStatus)
                font.pixelSize: homePage.baseFontSize
            }
        }

        Button {
            text: "Display Stats"
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.pixelSize: homePage.buttonFontSize
            onClicked: homePage.StackView.view.push("StatsPage.qml")
        }

        Button {
            text: "Fault Injection"
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.pixelSize: homePage.buttonFontSize
            onClicked: homePage.StackView.view.push("FaultPage.qml")
        }

        Button {
            text: "Commands"
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.pixelSize: homePage.buttonFontSize
            onClicked: homePage.StackView.view.push("CommandPage.qml")
        }
    }
}
