// SPDX-License-Identifier: MIT
// One of the meter's eight buttons. A tap presses it; holding it 0.7 s long-presses it. It flashes
// when the key is pressed from here or by an assistant (blue), or worked out from the meter's
// display as pressed on the meter itself (amber).
import QtQuick
import "."

Item {
    id: root
    property string keyId          // RANGE, HOLD, REL, PEAK, MODE, MINMAX, MEM, SETUP
    property string label
    property color bodyColor: Theme.button
    property color textColor: Theme.buttonText
    property bool enabledLong: true
    property string artName                 // its SVG art id is art-<artName>
    readonly property bool svgArt: layout.svgHas("art-" + artName)
    width: 92; height: 46
    SvgArt { name: root.artName; anchors.centerIn: parent }

    Rectangle {
        id: body
        anchors.fill: parent
        visible: !root.svgArt
        radius: height / 2
        color: area.pressed ? Qt.darker(root.bodyColor, 1.25) : root.bodyColor
        border.width: 2
        border.color: Qt.darker(root.bodyColor, 1.6)
        Rectangle {   // a soft top highlight, like the rubber keys
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
            height: parent.height / 2 - 4
            radius: height / 2
            color: Qt.rgba(1, 1, 1, area.pressed ? 0.03 : 0.08)
        }
        Text {
            anchors.centerIn: parent
            text: root.label
            color: root.textColor
            font { family: Theme.font; pixelSize: root.label.length > 6 ? 14 : 16; bold: true }
        }
    }
    Rectangle {
        id: flash
        anchors.fill: parent
        anchors.margins: -5
        radius: height / 2
        color: "transparent"
        border.width: 3
        opacity: 0
        visible: !layout.exporting
    }
    Rectangle {   // pressed feedback over SVG art
        anchors.fill: parent
        radius: height / 2
        visible: root.svgArt && area.pressed
        color: Qt.rgba(0, 0, 0, 0.25)
    }
    SequentialAnimation {
        id: flashAnim
        property color tint
        PropertyAction { target: flash; property: "border.color"; value: flashAnim.tint }
        NumberAnimation { target: flash; property: "opacity"; from: 1; to: 0; duration: 700; easing.type: Easing.OutQuad }
    }
    Connections {
        target: meter
        function onKeyFlash(key, longPress, fromApp) {
            if (key !== root.keyId)
                return
            flashAnim.tint = fromApp ? Theme.bluetooth : Theme.suggest
            flashAnim.restart()
        }
    }
    Timer {
        id: holdTimer
        interval: 700
        onTriggered: { area.longFired = true; meter.press(root.keyId, true) }
    }
    MouseArea {
        id: area
        property bool longFired: false
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onPressed: { longFired = false; if (root.enabledLong) holdTimer.start() }
        onReleased: {
            holdTimer.stop()
            if (!longFired && containsMouse)
                meter.press(root.keyId, false)
        }
        onCanceled: holdTimer.stop()
        hoverEnabled: true
    }
}
