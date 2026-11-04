// SPDX-License-Identifier: MIT
// One element's artwork from the SVG face ("art-<name>"), at the size the SVG gives it.
import QtQuick

Image {
    property string name
    readonly property rect r: layout.svgRect("art-" + name)
    visible: layout.svgHas("art-" + name)
    width: r.width; height: r.height
    source: visible ? "image://face/art-" + name : ""
    sourceSize: Qt.size(Math.ceil(r.width * 3), Math.ceil(r.height * 3))
    smooth: true
}
