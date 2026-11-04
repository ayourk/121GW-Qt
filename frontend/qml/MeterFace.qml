// SPDX-License-Identifier: MIT
// The whole meter, front on: the blue boot, the face plate, the logo, the LCD, the eight buttons
// with their legends, the rotary dial and the four input jacks, on a 500×1000 canvas the window
// scales. Layers, bottom to top: the SVG face's "artwork" group (if any; it replaces the boot and
// face plate), the QML elements (each placed by name, its art from QML or the SVG's art-<name>),
// then the SVG's "overlay" group. Ctrl+E edits the layout: click an element, move it with the arrow
// keys (Shift: 10 px), R puts it back, Esc ends.
import QtQuick
import GW121 1.0
import "."

Item {
    id: root
    objectName: "meterFace"
    width: 500; height: 1000
    focus: layout.editMode

    Keys.onPressed: (event) => {
        if (!layout.editMode)
            return
        const step = (event.modifiers & Qt.ShiftModifier) ? 10 : 1
        switch (event.key) {
        case Qt.Key_Left: layout.nudge(layout.selected, -step, 0); break
        case Qt.Key_Right: layout.nudge(layout.selected, step, 0); break
        case Qt.Key_Up: layout.nudge(layout.selected, 0, -step); break
        case Qt.Key_Down: layout.nudge(layout.selected, 0, step); break
        case Qt.Key_R: layout.resetOffset(layout.selected); break
        case Qt.Key_Escape: layout.editMode = false; break
        default: return
        }
        event.accepted = true
    }

    // The boot: lit across the front, shaded at the sides; and the face plate.
    Rectangle {
        anchors.fill: parent
        visible: !layout.hasBase
        radius: 70
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.bootShade }
            GradientStop { position: 0.12; color: Theme.bootLit }
            GradientStop { position: 0.88; color: Theme.bootLit }
            GradientStop { position: 1.0; color: Theme.bootShade }
        }
    }
    Rectangle {
        visible: !layout.hasBase
        x: 28; y: 26; width: 444; height: 948
        radius: 46
        color: Theme.face
        border.color: Qt.darker(Theme.face, 1.35)
        border.width: 2
    }
    Image {   // the SVG face's base
        anchors.fill: parent
        visible: layout.hasBase
        source: layout.hasBase ? "image://face/artwork" : ""
        sourceSize: Qt.size(1500, 3000)
        smooth: true
    }

    component Legend: Text {
        color: Theme.white
        font { family: Theme.font; pixelSize: 12; bold: true }
    }

    // Logo: "121GW" "EEV"(blue)"Blog" "MULTIMETER".
    Placed {
        name: "logo"; printed: true
        baseX: 60; baseY: 40
        Row {
            spacing: 10
            Text { text: "121GW"; color: Theme.white; anchors.baseline: blog.baseline
                   font { family: Theme.font; pixelSize: 24; bold: true; italic: true } }
            Text { id: blog; text: "<font color='#1A8FD0'>EEV</font>Blog"; textFormat: Text.StyledText
                   color: Theme.white; font { family: Theme.font; pixelSize: 32; bold: true } }
            Text { text: "MULTIMETER"; color: Theme.white; anchors.baseline: blog.baseline
                   font { family: Theme.font; pixelSize: 13; bold: true } }
        }
    }

    // The LCD in its raised frame.
    Placed {
        name: "lcd"
        baseX: 40; baseY: 88
        Item {
            width: 420; height: 230
            Rectangle {
                anchors.fill: parent
                visible: !layout.svgHas("art-lcd")
                radius: 16
                color: Theme.lcdFrame
            }
            SvgArt { name: "lcd"; anchors.centerIn: parent }
            Lcd {
                anchors.fill: parent
                anchors.margins: 4
                visible: !layout.exporting
                controller: meter
                glass: Theme.lcdGlass
                ink: Theme.lcdInk
                bezel: Theme.lcdFrame
                backlight: meter.backlight
            }
        }
    }

    // Buttons: two rows of four, with the legends printed round them.
    readonly property var columns: [44, 152, 260, 368]
    Placed { name: "legend-15v"; printed: true; baseX: root.columns[0] + 14; baseY: 336; Legend { text: "15V ▶|" } }
    Placed { name: "legend-autohold"; printed: true; baseX: root.columns[1] + 10; baseY: 336; Legend { text: "AutoHOLD" } }
    Placed { name: "legend-up"; printed: true; baseX: root.columns[2] + 40; baseY: 334; Legend { text: "▲"; font.pixelSize: 14 } }
    Placed { name: "legend-down"; printed: true; baseX: root.columns[3] + 40; baseY: 334; Legend { text: "▼"; font.pixelSize: 14 } }
    Placed { name: "btn-range"; baseX: root.columns[0]; baseY: 356; MeterButton { keyId: "RANGE"; label: "RANGE"; artName: "btn-range" } }
    Placed { name: "btn-hold"; baseX: root.columns[1]; baseY: 356; MeterButton { keyId: "HOLD"; label: "HOLD"; artName: "btn-hold"; enabledLong: false } }
    Placed { name: "btn-rel"; baseX: root.columns[2]; baseY: 356; MeterButton { keyId: "REL"; label: "REL △"; artName: "btn-rel" } }
    Placed { name: "btn-peak"; baseX: root.columns[3]; baseY: 356; MeterButton { keyId: "PEAK"; label: "1ms PEAK"; artName: "btn-peak" } }
    Placed {
        name: "legend-1khz"; printed: true
        baseX: root.columns[2] + 28; baseY: 407
        Legend { text: "1kHz"; font.pixelSize: 11; topPadding: 2
                 Rectangle { width: 26; height: 1.5; color: Theme.white } }
    }
    Placed {
        name: "icon-bt"; printed: true
        baseX: root.columns[3] + 38; baseY: 405
        Text { text: "✱"; color: Theme.bluetooth; font { family: Theme.font; pixelSize: 16; bold: true } }
    }
    Placed { name: "btn-mode"; baseX: root.columns[0]; baseY: 428
             MeterButton { keyId: "MODE"; label: "MODE"; artName: "btn-mode"; bodyColor: Theme.modeButton } }
    Placed { name: "btn-minmax"; baseX: root.columns[1]; baseY: 428; MeterButton { keyId: "MINMAX"; label: "MIN/MAX"; artName: "btn-minmax" } }
    Placed { name: "btn-mem"; baseX: root.columns[2]; baseY: 428; MeterButton { keyId: "MEM"; label: "MEM"; artName: "btn-mem" } }
    Placed { name: "btn-setup"; baseX: root.columns[3]; baseY: 428
             MeterButton { keyId: "SETUP"; label: "SETUP"; artName: "btn-setup"; bodyColor: Theme.setupButton; textColor: Theme.setupText } }
    Placed {   // the backlight bulb under MODE
        name: "icon-backlight"; printed: true
        baseX: root.columns[0] + 38; baseY: 478
        Text { text: "☀"; color: meter.backlight ? "#FFE08A" : Theme.bulb; font { family: Theme.font; pixelSize: 17 } }
    }

    // The dial: its legends, dots and knob are placed elements of their own.
    Dial { id: dial; x: 50; y: 500; faceOrigin: Qt.point(x, y) }

    Placed {
        name: "legend-truerms"; printed: true
        baseX: 52; baseY: 800
        Text { text: "True RMS"; color: Theme.white; font { family: Theme.font; pixelSize: 22; bold: true; italic: true } }
    }

    // VΩ on its own above COM, with its legends.
    Placed {
        name: "legend-v"; printed: true
        baseX: 360; baseY: 734
        Column {
            Text { anchors.right: parent.right; text: "VΩ ·)))"; color: Theme.white
                   font { family: Theme.font; pixelSize: 14; bold: true } }
            Text { anchors.right: parent.right; text: "▶| ⊣⊢ Temp"; color: Theme.white
                   font { family: Theme.font; pixelSize: 11; bold: true } }
        }
    }
    Placed { name: "jack-v"; baseX: 368; baseY: 772; Jack { jackId: "v"; artName: "jack-v"; tagSide: "left" } }

    // The bottom row: A 500mA, mA µA, COM.
    Placed { name: "legend-lowburden"; printed: true; baseX: 58; baseY: 836; Legend { text: "LOW BURDEN" } }
    Placed { name: "legend-a"; printed: true; baseX: 62; baseY: 852; Legend { text: "A 500mA"; font.pixelSize: 15 } }
    Placed { name: "legend-ma"; printed: true; baseX: 222; baseY: 852; Legend { text: "mA µA"; font.pixelSize: 15 } }
    Placed { name: "legend-com"; printed: true; baseX: 378; baseY: 852; Legend { text: "COM"; font.pixelSize: 15 } }
    Placed { name: "jack-a"; baseX: 64; baseY: 876; Jack { jackId: "a"; artName: "jack-a"; tagSide: "below" } }
    Placed { name: "jack-ma"; baseX: 214; baseY: 876; Jack { jackId: "ma"; artName: "jack-ma"; tagSide: "below" } }
    Placed { name: "jack-com"; baseX: 368; baseY: 876
             Jack { jackId: "com"; artName: "jack-com"; insulator: Theme.jackCom; tagSide: "below" } }
    Placed { name: "warn-a"; printed: true; baseX: 145; baseY: 884
             Legend { text: "⚠\n≥10A\nMAX"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter } }
    Placed { name: "warn-ma"; printed: true; baseX: 292; baseY: 884
             Legend { text: "⚠\n400mA\nMAX"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter } }
    Placed { name: "warn-com"; printed: true; baseX: 440; baseY: 880
             Legend { width: 30; text: "⚠\nMAX\nCAT\nIII\n600V"; font.pixelSize: 8; horizontalAlignment: Text.AlignHCenter } }
    Placed {
        name: "legend-fused"; printed: true
        baseX: 70; baseY: 946
        Item {
            width: 220; height: 14
            Rectangle { y: 8; width: 220; height: 1.5; color: Theme.white }
            Legend { anchors.centerIn: parent; text: " FUSED "; font.pixelSize: 9
                     Rectangle { anchors.fill: parent; color: Theme.face; z: -1 } }
        }
    }

    Image {   // the SVG face's overlay: glare, bezel lips; clicks pass through
        anchors.fill: parent
        visible: layout.hasOverlay && !layout.exporting
        source: layout.hasOverlay ? "image://face/overlay" : ""
        sourceSize: Qt.size(1500, 3000)
        smooth: true
    }

    // Edit mode: what's selected and how to move it.
    Rectangle {
        visible: layout.editMode
        x: 28; y: 2; width: 444; height: 22; radius: 6
        color: "#CC1A0A1A"
        Text {
            anchors.centerIn: parent
            color: "white"
            font { family: Theme.font; pixelSize: 11; bold: true }
            text: {
                if (layout.selected === "")
                    return "Edit layout: click an element · Esc to finish"
                const o = layout.offsets[layout.selected]
                return layout.selected + "  " + (o ? o[0] + ", " + o[1] : "0, 0")
                       + "   ←↑↓→ 1px · Shift 10px · R reset · Esc"
            }
        }
    }
}
