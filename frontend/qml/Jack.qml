// SPDX-License-Identifier: MIT
// An input jack. It lights up when a probe lead belongs in it (red or black), with what for. On the
// demo meter, clicking a current jack plugs the red lead into it.
import QtQuick
import "."

Item {
    id: root
    property string jackId          // a, ma, com, v
    property color insulator: Theme.jackRed
    property string tagSide: "above"   // where the lead's tag goes: above, below or left
    readonly property var use: meter.jacks[jackId]
    readonly property bool wanted: use !== undefined
    readonly property bool redLead: wanted && use.role.indexOf("red") === 0
    readonly property bool plugged: meter.demo && meter.demoLead === jackId
    property string artName
    readonly property bool svgArt: layout.svgHas("art-" + artName)
    width: 72; height: 72
    SvgArt { name: root.artName; anchors.centerIn: parent }

    Rectangle {   // where the lead goes: a pulsing halo in the lead's color
        anchors.centerIn: parent
        width: parent.width + 22; height: width; radius: width / 2
        color: "transparent"
        border.width: 4
        border.color: root.redLead ? Theme.leadRed : Theme.leadBlack
        visible: root.wanted && !layout.exporting
        SequentialAnimation on opacity {
            running: root.wanted && meter.jacksSource === "suggestion"
            loops: Animation.Infinite
            NumberAnimation { from: 1; to: 0.25; duration: 650 }
            NumberAnimation { from: 0.25; to: 1; duration: 650 }
        }
    }
    Rectangle {   // the black collar
        visible: !root.svgArt || root.plugged
        color: root.svgArt ? "transparent" : Theme.jackCollar
        anchors.centerIn: parent
        width: parent.width; height: width; radius: width / 2
        Rectangle {   // the insulator ring
            anchors.centerIn: parent
            width: parent.width * 0.74; height: width; radius: width / 2
            color: root.svgArt ? "transparent" : root.insulator
            Rectangle {   // the socket
                anchors.centerIn: parent
                width: parent.width * 0.5; height: width; radius: width / 2
                color: root.svgArt ? "transparent" : Theme.jackHole
                border.color: root.svgArt ? "transparent" : Qt.lighter(Theme.jackHole, 1.8)
                border.width: 1
                Rectangle {   // a plugged demo lead
                    anchors.centerIn: parent
                    width: parent.width * 0.8; height: width; radius: width / 2
                    color: "#B8202A"
                    border.color: "#E85A5A"
                    border.width: 2
                    visible: root.plugged
                }
            }
        }
    }
    Rectangle {   // what the lead is for
        visible: root.wanted && !layout.exporting
        x: root.tagSide === "left" ? -width - 16 : (root.width - width) / 2
        y: root.tagSide === "left" ? (root.height - height) / 2
           : root.tagSide === "below" ? root.height + 3 : -height - 14
        width: tag.implicitWidth + 12; height: tag.implicitHeight + 6; radius: 4
        color: root.redLead ? Theme.leadRed : Theme.leadBlack
        z: 2
        Text {
            id: tag
            anchors.centerIn: parent
            text: root.wanted ? root.use.role.toUpperCase() : ""
            color: root.redLead ? "white" : "#111"
            font { family: Theme.font; pixelSize: 11; bold: true }
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: meter.demo && (root.jackId === "a" || root.jackId === "ma")
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: meter.plugLead(root.plugged ? "" : root.jackId)
    }
}
