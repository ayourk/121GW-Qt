// SPDX-License-Identifier: MIT
// Beside the meter: connection, the reading in words, dial guidance, logging and the MCP server.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    property string manualNote: ""
    Connections {
        target: meter
        function onDialIsManual() {
            root.manualNote = "The meter's dial turns by hand: turn the real knob."
            noteTimer.restart()
        }
    }
    Timer { id: noteTimer; interval: 4000; onTriggered: root.manualNote = "" }

    component Heading: Label {
        font.pixelSize: 13
        font.bold: true
        color: Theme.bootLit
        Layout.topMargin: 8
    }
    component Card: Frame {
        Layout.fillWidth: true
        padding: 10
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 6

        // Errors and notes.
        Card {
            visible: meter.errorText !== "" || root.manualNote !== ""
            background: Rectangle { color: "#5A2A22"; radius: 6 }
            RowLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: meter.errorText !== "" ? meter.errorText : root.manualNote
                }
                ToolButton { text: "✕"; visible: meter.errorText !== ""; onClicked: meter.dismissError() }
            }
        }

        Heading { text: "METER" }
        Card {
            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: meter.connected ? meter.meterName + (meter.signalText !== "" ? "  ·  " + meter.signalText : "")
                                          : meter.connectionState
                    font.bold: true
                }
                RowLayout {
                    Button {
                        text: meter.scanning ? "Stop scan" : "Scan"
                        enabled: !meter.connected || meter.demo
                        onClicked: meter.scanning ? meter.stopScan() : meter.startScan()
                    }
                    Button { text: "Demo"; enabled: !meter.demo; onClicked: meter.startDemo() }
                    Button {
                        text: "Disconnect"
                        enabled: meter.connected || meter.scanning
                        onClicked: meter.disconnectMeter()
                    }
                }
                Repeater {
                    model: meter.discovered
                    delegate: ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name + "  (" + modelData.rssi + " dBm)  " + modelData.id
                        onClicked: meter.connectMeter(modelData.id)
                    }
                }
                Label {
                    visible: meter.demo
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: "Demo meter: click the dial legends to turn its knob, and the A or mA jack to plug its red lead."
                }
            }
        }

        Heading { text: "READING" }
        Card {
            ColumnLayout {
                anchors.fill: parent
                Label { text: meter.hasReading ? meter.modeName : "—"; opacity: 0.8 }
                Label {
                    text: meter.mainLine !== "" ? meter.mainLine : (meter.hasReading ? "measuring…" : "")
                    font.pixelSize: 30
                    font.bold: true
                }
                Label { visible: meter.subLine !== ""; text: meter.subLine }
                Label {
                    visible: meter.batteryText !== ""
                    text: "Battery " + meter.batteryText
                    opacity: 0.8
                }
                Label {
                    visible: meter.dialPosition >= 0
                    text: "Dial: " + (meter.dialPosition >= 0 ? meter.dialPositions[meter.dialPosition].legend : "")
                          + (meter.dialFunction !== "" ? " · " + meter.dialFunction : "")
                    opacity: 0.8
                }
            }
        }

        Heading { text: "DIAL GUIDANCE" }
        Card {
            ColumnLayout {
                anchors.fill: parent
                Label {
                    visible: meter.suggestedPosition < 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: "No suggestion. An assistant connected over MCP can light a dial position and the jacks to use."
                }
                Label {
                    visible: meter.suggestedPosition >= 0 && meter.suggestionReason !== ""
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.bold: true
                    text: meter.suggestionReason
                }
                Label {
                    visible: meter.suggestedPosition >= 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: meter.suggestionMet ? Theme.ready : Theme.suggest
                    font.pixelSize: 15
                    text: {
                        if (meter.suggestedPosition < 0)
                            return ""
                        const target = meter.dialPositions[meter.suggestedPosition].legend
                                       + (meter.suggestedFunction !== "" ? " · " + meter.suggestedFunction : "")
                        if (meter.suggestionMet)
                            return "✓ Ready: " + target
                        if (meter.dialPosition !== meter.suggestedPosition)
                            return "Turn the dial to " + target
                        if (meter.moveLeadTo !== "")
                            return "Move the red lead to " + meter.moveLeadTo
                        if (meter.modePressesNeeded > 0)
                            return "Press MODE " + (meter.modePressesNeeded === 1 ? "once" : meter.modePressesNeeded + " times")
                        return "Turn the dial to " + target
                    }
                }
                Repeater {
                    model: Object.keys(meter.jacks)
                    delegate: Label {
                        required property string modelData
                        readonly property var use: meter.jacks[modelData]
                        readonly property var names: ({ "a": "A 500mA", "ma": "mA µA", "com": "COM", "v": "VΩ" })
                        text: "• " + use.role + " lead → " + names[modelData] + (use.note !== "" ? "  (" + use.note + ")" : "")
                        opacity: 0.85
                    }
                }
                Button {
                    visible: meter.suggestedPosition >= 0
                    text: "Clear suggestion"
                    onClicked: meter.clearSuggestion()
                }
            }
        }

        Heading { text: "LOG" }
        Card {
            ColumnLayout {
                anchors.fill: parent
                CheckBox {
                    text: "Logging (" + meter.logCount + " readings)"
                    checked: meter.logging
                    onToggled: meter.logging = checked
                }
                RowLayout {
                    TextField { id: sessionName; Layout.fillWidth: true; placeholderText: "Session name (optional)" }
                    Button {
                        text: "Save"
                        enabled: meter.logCount > 0
                        onClicked: { const n = meter.saveSession(sessionName.text); if (n !== "") savedLabel.text = "Saved: " + n; sessionName.text = "" }
                    }
                }
                RowLayout {
                    Label { id: savedLabel; Layout.fillWidth: true; opacity: 0.7; elide: Text.ElideRight }
                    Button { text: "Clear"; flat: true; enabled: meter.logCount > 0; onClicked: meter.clearLog() }
                }
            }
        }

        Heading { text: "AI ASSISTANT (MCP)" }
        Card {
            ColumnLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    text: meter.mcpRunning ? meter.mcpEndpoint : "MCP server off"
                }
                Label {
                    visible: meter.mcpActivity !== ""
                    text: "Last call: " + meter.mcpActivity
                    opacity: 0.7
                }
                RowLayout {
                    Button {
                        text: "Copy 'claude mcp add'"
                        enabled: meter.mcpRunning
                        onClicked: meter.copyToClipboard(meter.mcpCommand)
                    }
                    Button {
                        text: "Set meter clock"
                        enabled: meter.connected
                        onClicked: meter.setClock()
                    }
                }
            }
        }

        Label {
            Layout.topMargin: 8
            opacity: 0.5
            text: "121gw-qt " + meter.version + " · ported from Readout (MIT)"
        }
    }
}
