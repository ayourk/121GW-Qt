// SPDX-License-Identifier: MIT
// The meter's SD card over Bluetooth (custom firmware): its files, downloads, uploads, and a
// firmware update entry point. Transfers run at about 190 bytes per second.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    component Heading: Label {
        font.pixelSize: 13
        font.bold: true
        color: Theme.bootLit
        Layout.topMargin: 8
    }
    component Box: Frame {
        Layout.fillWidth: true
        padding: 10
    }
    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        opacity: 0.75
    }

    readonly property bool canUse: meter.connected && !meter.cardBusy

    // List the card once a meter connects, when the view is first shown.
    property bool shown: false
    function listIfNew() {
        if (shown && meter.connected && meter.cardListedAt === "" && !meter.cardBusy)
            meter.cardList()
    }
    onShownChanged: listIfNew()
    Connections {
        target: meter
        function onStateChanged() { root.listIfNew() }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 6

        Heading { text: "SD CARD" }
        Box {
            ColumnLayout {
                anchors.fill: parent
                Note {
                    visible: !meter.connected
                    text: "Connect a meter to reach its SD card. File transfer needs the custom firmware with "
                          + "Bluetooth file transfer; the stock firmware doesn't answer."
                }
                RowLayout {
                    Button { text: "Refresh"; enabled: root.canUse; onClicked: meter.cardList() }
                    Button {
                        text: "Upload…"
                        enabled: root.canUse
                        onClicked: {
                            const path = meter.chooseUploadFile()
                            if (path !== "")
                                uploadDialog.openFor(path, meter.suggestCardName(path))
                        }
                    }
                    Button {
                        text: "Update firmware…"
                        enabled: root.canUse
                        onClicked: {
                            const path = meter.chooseFirmwareFile()
                            if (path !== "")
                                firmwareDialog.openFor(path)
                        }
                    }
                }
                Label {
                    visible: meter.cardListedAt !== ""
                    opacity: 0.6
                    text: meter.cardFiles.length + " files · listed " + meter.cardListedAt
                }
                Repeater {
                    model: meter.cardFiles
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1   // what the button leaves
                            spacing: 0
                            Label {
                                text: modelData.name
                                font.family: "DejaVu Sans Mono"
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: (modelData.directory ? "folder" : modelData.sizeText)
                                      + (modelData.date !== "" ? "  ·  " + modelData.date : "")
                                      + (modelData.protected ? "  ·  read-only (calibration)" : "")
                                      + (modelData.firmware ? "  ·  firmware image" : "")
                                opacity: 0.65
                                font.pixelSize: 12
                            }
                        }
                        Button {
                            text: "Download"
                            flat: true
                            visible: !modelData.directory
                            enabled: root.canUse
                            onClicked: meter.cardDownload(modelData.name)
                        }
                    }
                }
            }
        }

        Heading { text: "TRANSFER"; visible: meter.cardBusy || meter.cardMessage !== "" }
        // The meter's prompt: the transfer waits for SETUP on the meter itself.
        Box {
            visible: meter.cardPhase === "confirm_on_meter"
            background: Rectangle { color: "#4A3A10"; radius: 6; border.color: Theme.suggest; border.width: 2 }
            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: "Confirm on the meter: press SETUP"
                    color: Theme.suggest
                    font.pixelSize: 17
                    font.bold: true
                }
                Note {
                    opacity: 0.9
                    text: "The meter asks before writing " + meter.cardFile + ". Press SETUP on the meter within 30 seconds "
                          + "to accept; any other key, or waiting, refuses."
                }
            }
        }
        Box {
            visible: meter.cardBusy
            ColumnLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.bold: true
                    text: {
                        switch (meter.cardOperation) {
                        case "list": return "Listing the card…"
                        case "download": return "Downloading " + meter.cardFile
                        case "upload": return "Uploading " + meter.cardFile
                        }
                        return ""
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: meter.cardOperation !== "list"
                    indeterminate: meter.cardProgress < 0 || meter.cardPhase === "requesting"
                                   || meter.cardPhase === "starting" || meter.cardPhase === "confirm_on_meter"
                    value: Math.max(0, meter.cardProgress)
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.8
                    text: {
                        switch (meter.cardPhase) {
                        case "requesting": return "Asking the meter…"
                        case "starting": return "Starting…"
                        case "confirm_on_meter": return "Waiting for SETUP on the meter"
                        case "finishing": return "Finishing…"
                        }
                        return [meter.cardProgressText, meter.cardRateText, meter.cardEtaText].filter(s => s !== "").join("  ·  ")
                    }
                }
                Button { text: "Cancel"; onClicked: meter.cardCancel() }
            }
        }
        Box {
            visible: !meter.cardBusy && meter.cardMessage !== ""
            background: Rectangle { color: meter.cardMessageOk ? "#1F3A26" : "#5A2A22"; radius: 6 }
            Label {
                anchors.fill: parent
                wrapMode: Text.Wrap
                text: meter.cardMessage
            }
        }

        Heading { text: "DEMO METER"; visible: meter.demo }
        Box {
            visible: meter.demo
            ColumnLayout {
                anchors.fill: parent
                Note {
                    text: "The demo meter has an in-memory card at the meter's speed. Writing an existing file or "
                          + "EEVBLOG.BIN brings up its SETUP prompt, which it accepts after a few seconds."
                }
                CheckBox {
                    text: "Don't press SETUP (the prompt refuses)"
                    checked: meter.cardDemoReject
                    onToggled: meter.cardDemoReject = checked
                }
            }
        }

        Note {
            Layout.topMargin: 8
            opacity: 0.5
            text: "About 190 bytes per second: a 92 KB firmware image takes about 8 minutes, the stock 2.05 (141 KB) "
                  + "about 13. The meter's readings "
                  + "pause during a transfer. CAL.BIN can't be written over Bluetooth."
        }
    }

    // ── Upload: name, overwrite warning, the meter's confirmation ───────────
    Dialog {
        id: uploadDialog
        property string path: ""
        function openFor(p, name) {
            path = p
            nameField.text = name
            open()
        }
        readonly property string name: nameField.text.trim().toUpperCase()
        readonly property string problem: meter.cardNameProblem(name)
        readonly property bool exists: problem === "" && meter.cardFileExists(name)
        readonly property bool firmware: name === "EEVBLOG.BIN"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 40)
        modal: true
        // Material's own background is drawn with an elevation effect, which some renderers skip.
        background: Rectangle { color: "#424242"; radius: 4; border.color: "#5A6168" }
        title: "Upload to the meter's SD card"

        ColumnLayout {
            width: parent.width
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
                text: uploadDialog.path
                opacity: 0.8
            }
            Label { text: meter.localFileText(uploadDialog.path); opacity: 0.8 }
            Label { text: "Name on the card (8.3):" }
            TextField {
                id: nameField
                Layout.fillWidth: true
                font.family: "DejaVu Sans Mono"
            }
            Label {
                visible: uploadDialog.problem !== ""
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.leadRed
                text: uploadDialog.problem
            }
            Label {
                visible: uploadDialog.exists
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.suggest
                font.bold: true
                text: uploadDialog.name + " is already on the card. Uploading replaces it."
            }
            Label {
                visible: uploadDialog.firmware && uploadDialog.problem === ""
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.leadRed
                font.bold: true
                text: "EEVBLOG.BIN is the firmware image: the meter flashes it the next time you run its firmware "
                      + "update procedure. Use Update firmware… for that."
            }
            Label {
                visible: uploadDialog.problem === "" && meter.cardNeedsConfirmation(uploadDialog.name)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "The meter will ask before writing it: press SETUP on the meter within 30 seconds to accept."
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Cancel"; flat: true; onClicked: uploadDialog.close() }
                Button {
                    text: uploadDialog.exists ? "Replace" : "Upload"
                    enabled: uploadDialog.problem !== "" ? false : !uploadDialog.firmware
                    highlighted: true
                    onClicked: {
                        uploadDialog.close()
                        meter.cardUpload(uploadDialog.path, uploadDialog.name)
                    }
                }
            }
        }
    }

    // ── Firmware: a strong warning before writing EEVBLOG.BIN ───────────────
    Dialog {
        id: firmwareDialog
        property string path: ""
        function openFor(p) {
            path = p
            understood.checked = false
            open()
        }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 40)
        modal: true
        // Material's own background is drawn with an elevation effect, which some renderers skip.
        background: Rectangle { color: "#424242"; radius: 4; border.color: "#5A6168" }
        title: "Update the meter's firmware"

        ColumnLayout {
            width: parent.width
            spacing: 8
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: warning.implicitHeight + 20
                color: "#5A2A22"
                radius: 6
                Label {
                    id: warning
                    anchors { fill: parent; margins: 10 }
                    wrapMode: Text.Wrap
                    text: "<b>This writes the file below to the meter's SD card as EEVBLOG.BIN.</b> The meter's "
                          + "bootloader flashes EEVBLOG.BIN, unchecked, the next time you run its firmware update "
                          + "procedure. A wrong or damaged image can leave the meter unusable until it is recovered "
                          + "from the SD card. Only use a firmware image built for the 121GW that you trust."
                          + (meter.cardFileExists("EEVBLOG.BIN") ? "<br><br>The card already has an EEVBLOG.BIN; it will be replaced." : "")
                    textFormat: Text.StyledText
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
                text: firmwareDialog.path
                opacity: 0.8
            }
            Label { text: meter.localFileText(firmwareDialog.path); opacity: 0.8 }
            Label {
                visible: meter.localFileSize(firmwareDialog.path) > 256 * 1024 || meter.localFileSize(firmwareDialog.path) < 1024
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.leadRed
                text: "That size doesn't look like a 121GW firmware image (they are 90 to 145 KB)."
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "The meter asks before writing EEVBLOG.BIN: press SETUP on the meter within 30 seconds to accept. "
                      + "Keep the meter and this computer close and the meter powered for the whole transfer."
            }
            CheckBox {
                id: understood
                text: "I understand the meter flashes this file on its next update"
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Cancel"; flat: true; onClicked: firmwareDialog.close() }
                Button {
                    text: "Upload as EEVBLOG.BIN"
                    enabled: understood.checked && meter.localFileSize(firmwareDialog.path) > 0
                    highlighted: true
                    onClicked: {
                        firmwareDialog.close()
                        meter.cardUpload(firmwareDialog.path, "EEVBLOG.BIN")
                    }
                }
            }
        }
    }
}
