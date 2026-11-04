// SPDX-License-Identifier: MIT
// 121gw-qt's window: the meter, scaled to fit, and the side panel.
// Customize: run `121gw-qt --export-qml` and edit ~/.config/121gw-qt/qml/ (the app loads it from there);
// Ctrl+E nudges elements; ~/.config/121gw-qt/face.svg can supply artwork (see --export-svg).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

ApplicationWindow {
    id: window
    visible: true
    width: 1080; height: 1000
    minimumWidth: 760; minimumHeight: 640
    title: meter.meterName !== "" ? "121gw-qt — " + meter.meterName : "121gw-qt"
    color: Theme.window

    Shortcut {
        sequence: "Ctrl+E"
        onActivated: { layout.editMode = !layout.editMode; face.forceActiveFocus() }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            MeterFace {
                id: face
                anchors.centerIn: parent
                scale: Math.min(parent.width / width, parent.height / height)
            }
        }
        // A plain Item between the two layouts: the column re-lays itself out when the prompt
        // below appears (nested directly in the RowLayout, it didn't on Qt 6.4).
        Item {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                TabBar {
                    id: tabs
                    Layout.fillWidth: true
                    TabButton { text: "Meter" }
                    TabButton {
                        text: meter.cardBusy && meter.cardProgress >= 0 && meter.cardOperation !== "list"
                              ? "SD card (" + Math.round(meter.cardProgress * 100) + "%)" : "SD card"
                    }
                }
                // The meter's SETUP prompt, whichever view is showing.
                Frame {
                    Layout.fillWidth: true
                    visible: meter.cardPhase === "confirm_on_meter" && tabs.currentIndex !== 1
                    padding: 8
                    background: Rectangle { color: "#4A3A10"; radius: 6; border.color: Theme.suggest; border.width: 2 }
                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: Theme.suggest
                        font.bold: true
                        text: "Confirm on the meter: press SETUP to accept " + meter.cardFile + " (30 s)"
                        MouseArea { anchors.fill: parent; onClicked: tabs.currentIndex = 1 }
                    }
                }
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabs.currentIndex
                    SidePanel {}
                    CardPanel { shown: tabs.currentIndex === 1 }
                }
            }
        }
    }
}
