// SPDX-License-Identifier: MIT
// The 121GW's colors, sampled from product photos (see palette.md beside the repo).
pragma Singleton
import QtQuick

QtObject {
    readonly property color bootLit: "#1E9AD6"
    readonly property color bootShade: "#0F78B4"
    readonly property color face: "#3A4249"
    readonly property color lcdFrame: "#49504F"
    readonly property color lcdGlass: "#6B7262"
    readonly property color lcdInk: "#2E302E"
    readonly property color white: "#E6ECEE"
    readonly property color logoBlue: "#1A8FD0"
    readonly property color button: "#4A5560"
    readonly property color buttonText: "#F0F4F6"
    readonly property color modeButton: "#D63D36"
    readonly property color setupButton: "#E2BF55"
    readonly property color setupText: "#221B1B"
    readonly property color legendRed: "#C0403A"
    readonly property color bulb: "#D0A040"
    readonly property color bluetooth: "#2A8CC8"
    readonly property color dialLegend: "#EEF2F4"
    readonly property color knob: "#383D40"
    readonly property color knobInsert: "#C8CDD2"
    readonly property color gripRecess: "#33393C"
    readonly property color gripRidge: "#53585A"
    readonly property color jackRed: "#C0383A"
    readonly property color jackCom: "#464D4D"
    readonly property color jackHole: "#202120"
    readonly property color jackCollar: "#272828"

    // Guidance overlays (not on the meter).
    readonly property color suggest: "#FFC24D"
    readonly property color ready: "#52D273"
    readonly property color leadRed: "#FF4B3E"
    readonly property color leadBlack: "#F2F5F7"
    readonly property color window: "#16191C"
    readonly property string font: "DejaVu Sans"
}
