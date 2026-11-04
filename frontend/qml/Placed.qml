// SPDX-License-Identifier: MIT
// A front panel element at its place: its QML default (baseX, baseY), or centered on the SVG face's
// element of the same name, plus any nudge from the layout edit mode. Printed artwork (legends,
// the logo) is swapped for the SVG's "art-<name>" when it has one; elements with behavior handle
// their own art. In edit mode a click selects it and the arrow keys move it.
import QtQuick
import "."

Item {
    id: p
    property string name
    property real baseX: 0
    property real baseY: 0
    property bool centered: false            // baseX, baseY are the center, not the top left
    property point origin: Qt.point(0, 0)    // the parent's top left, in face units
    property bool printed: false             // plain artwork: replaced by art-<name> when the SVG has it
    readonly property bool svgArt: layout.svgHas("art-" + name)
    readonly property bool fromSvg: layout.svgHas(name)
    readonly property rect svgRect: fromSvg ? layout.svgRect(name) : Qt.rect(0, 0, 0, 0)
    readonly property var off: layout.offsets[name]
    default property alias content: holder.data

    width: holder.childrenRect.width
    height: holder.childrenRect.height
    x: (fromSvg ? svgRect.x + svgRect.width / 2 - width / 2 - origin.x
                : (centered ? baseX - width / 2 : baseX)) + (off ? off[0] : 0)
    y: (fromSvg ? svgRect.y + svgRect.height / 2 - height / 2 - origin.y
                : (centered ? baseY - height / 2 : baseY)) + (off ? off[1] : 0)

    Item {
        id: holder
        // While exporting a template only the boot and face plate are captured as its base.
        visible: !(p.printed && p.svgArt) && !layout.exporting
        width: childrenRect.width; height: childrenRect.height
    }
    SvgArt {
        name: p.name
        visible: p.printed && p.svgArt
        anchors.centerIn: parent
    }

    function reportPlace() { layout.report(name, origin.x + x, origin.y + y, width, height) }
    onXChanged: reportPlace()
    onYChanged: reportPlace()
    onWidthChanged: reportPlace()
    Component.onCompleted: reportPlace()

    Rectangle {   // edit mode: every element outlined, the selected one bold
        z: 100
        anchors.fill: parent
        anchors.margins: -2
        color: "transparent"
        visible: layout.editMode
        border.width: layout.selected === p.name ? 2 : 1
        border.color: layout.selected === p.name ? "#FF40FF" : Qt.rgba(1, 0.25, 1, 0.45)
    }
    MouseArea {
        z: 101
        anchors.fill: parent
        anchors.margins: -2
        enabled: layout.editMode
        cursorShape: enabled ? Qt.CrossCursor : Qt.ArrowCursor
        onClicked: layout.selected = p.name
    }
}
