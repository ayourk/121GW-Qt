// SPDX-License-Identifier: MIT
// The rotary dial: legends and position dots round the top, the gripped knob with its pointer.
// The knob turns to where the meter is (worked out from its mode). A suggested position's dot pulses
// amber until the meter gets there, then turns green, with a ghost pointer showing which way to turn.
// Every legend, dot and the knob are placed elements (dial-legend-<id>, dial-dot-<id>, dial-knob).
import QtQuick
import "."

Item {
    id: root
    width: 400; height: 300
    property point faceOrigin: Qt.point(0, 0)   // this dial's top left, in face units
    readonly property real cx: knobPlace.x + knobPlace.width / 2
    readonly property real cy: knobPlace.y + knobPlace.height / 2
    readonly property real baseCx: width / 2
    readonly property real baseCy: 180
    readonly property real legendRadius: 172
    readonly property real dotRadius: 140
    readonly property real gripRadius: 118
    property int shownPosition: 2   // where the knob is drawn: the last known position

    Connections {
        target: meter
        function onDialChanged() { if (meter.dialPosition >= 0) root.shownPosition = meter.dialPosition }
    }
    Component.onCompleted: if (meter.dialPosition >= 0) shownPosition = meter.dialPosition

    function angleOf(i) { return meter.dialPositions[i].angle }
    function px(i, r) { return baseCx + r * Math.cos(angleOf(i) * Math.PI / 180) }
    function py(i, r) { return baseCy - r * Math.sin(angleOf(i) * Math.PI / 180) }

    // Legends and dots.
    Repeater {
        model: meter.dialPositions.length
        delegate: Item {
            id: pos
            required property int index
            readonly property var info: meter.dialPositions[index]
            readonly property bool suggested: meter.suggestedPosition === index
            readonly property bool current: meter.dialPosition === index

            Placed {
                name: "dial-dot-" + pos.info.id
                origin: root.faceOrigin
                centered: true
                baseX: root.px(pos.index, root.dotRadius)
                baseY: root.py(pos.index, root.dotRadius)
                Item {
                    width: 14; height: 14
                    readonly property bool svgDot: layout.svgHas("art-dial-dot-" + pos.info.id)
                    SvgArt { name: "dial-dot-" + pos.info.id; anchors.centerIn: parent }
                    Rectangle {
                        anchors.centerIn: parent
                        visible: pos.suggested ? !layout.exporting : !parent.svgDot
                        width: pos.suggested ? 14 : pos.current ? 11 : 8; height: width; radius: width / 2
                        // The suggested position's dot: amber until the meter is there, then green.
                        color: pos.suggested ? (meter.suggestionMet ? Theme.ready : Theme.suggest) : Theme.dialLegend
                        SequentialAnimation on opacity {
                            running: pos.suggested && !meter.suggestionMet
                            loops: Animation.Infinite
                            onStopped: parent.opacity = 1
                            NumberAnimation { from: 1; to: 0.35; duration: 600 }
                            NumberAnimation { from: 0.35; to: 1; duration: 600 }
                        }
                    }
                }
            }
            Placed {
                name: "dial-legend-" + pos.info.id
                origin: root.faceOrigin
                centered: true
                printed: true
                baseX: root.px(pos.index, root.legendRadius)
                baseY: root.py(pos.index, root.legendRadius)
                Item {
                    width: legendText.width; height: legendText.height
                    Column {
                        id: legendText
                        spacing: 0
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: pos.info.redLegend !== ""
                            text: pos.info.redLegend
                            color: Theme.legendRed
                            font { family: Theme.font; pixelSize: 11; bold: true }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: pos.info.legend
                            color: Theme.dialLegend
                            font { family: Theme.font; pixelSize: pos.info.legend.length > 4 ? 15 : 19; bold: true }
                        }
                    }
                    MouseArea {   // turns the demo meter's knob (stays clickable over SVG art)
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: meter.turnDial(pos.index)
                    }
                }
            }
        }
    }

    // The knob: a gripped ring, the body, and the pointer bar with its light insert; or the SVG's
    // art-dial-knob, drawn pointing at 12 o'clock and turned the same way.
    Placed {
        id: knobPlace
        name: "dial-knob"
        origin: root.faceOrigin
        centered: true
        baseX: root.baseCx
        baseY: root.baseCy
        Item {
            id: knob
            width: 2 * root.gripRadius; height: width
            rotation: 90 - root.angleOf(root.shownPosition)
            visible: !layout.exporting
            opacity: meter.dialPosition >= 0 || meter.demo ? 1 : 0.75
            Behavior on rotation { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
            readonly property bool svgArt: layout.svgHas("art-dial-knob")

            SvgArt { name: "dial-knob"; anchors.centerIn: parent }
            Item {
                anchors.fill: parent
                visible: !knob.svgArt
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: Theme.gripRecess
                }
                Repeater {
                    model: 24
                    delegate: Item {
                        required property int index
                        anchors.fill: parent
                        rotation: index * 15
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 2
                            width: 13; height: 24; radius: 5
                            color: Theme.gripRidge
                        }
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 34; height: width; radius: width / 2
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.lighter(Theme.knob, 1.25) }
                        GradientStop { position: 1; color: Qt.darker(Theme.knob, 1.2) }
                    }
                }
                Rectangle {   // the pointer bar, across the knob
                    anchors.centerIn: parent
                    width: 46; height: parent.height + 6; radius: 16
                    color: Qt.lighter(Theme.knob, 1.12)
                    border.color: Qt.darker(Theme.knob, 1.5)
                    border.width: 2
                    Rectangle {   // the light insert at the pointing end
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 12
                        width: 14; height: 64; radius: 7
                        color: Theme.knobInsert
                        opacity: 0.92
                    }
                    Rectangle {   // the grip slot in the middle
                        anchors.centerIn: parent
                        width: 18; height: 40; radius: 9
                        color: Qt.darker(Theme.knob, 1.3)
                    }
                }
            }
        }
    }

    // The ghost pointer, over the knob: where an assistant asked for the knob to go.
    Item {
        z: 2
        visible: meter.suggestedPosition >= 0 && meter.suggestedPosition !== meter.dialPosition && !layout.exporting
        x: root.cx - width / 2; y: root.cy - height / 2
        width: 2 * root.gripRadius; height: width
        rotation: meter.suggestedPosition >= 0 ? 90 - root.angleOf(meter.suggestedPosition) : 0
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: -14
            width: 22; height: root.gripRadius + 14; radius: 11
            color: Qt.rgba(1, 0.76, 0.3, 0.28)
            border.color: Theme.suggest
            border.width: 2
        }
    }
}
