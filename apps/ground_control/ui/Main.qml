import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import GroundControl

ApplicationWindow {
    id: root

    Material.theme: Material.Dark
    Material.roundedScale: Material.SmallScale

    GroundControl {
        id: groundControl
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

    title: "Ground Control"

    color: "black"

    StackView {
        id: pageStack
        anchors.fill: parent
        initialItem: HomePage {}
    }

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
}
