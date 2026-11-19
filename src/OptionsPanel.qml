import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

SectionCard {
    id: root
    title: qsTr("Action")

    property bool advancedOpen: false
    readonly property bool elf: backend.command === "elf"

    Material.theme: Material.Dark
    Material.accent: theme.brandAccent
    Material.roundedScale: theme.radius

    ComboBox {
        id: commandBox
        Layout.fillWidth: true
        implicitHeight: 38
        font.pixelSize: 12
        enabled: backend.availableCommands.length > 1 && !backend.running
        model: backend.availableCommands
        displayText: Format.commandLabel(backend.command, backend.decompile)
        currentIndex: Math.max(0, backend.availableCommands.indexOf(backend.command))

        background: Rectangle {
            color: "#121214"
            radius: Math.min(4, theme.radius)
            border.width: 1
            border.color: commandBox.activeFocus ? theme.brandAccent : "#34343b"
        }

        delegate: ItemDelegate {
            width: commandBox.width
            height: 34
            text: Format.commandLabel(modelData, modelData === "elf" ? backend.decompile : false)
            font.pixelSize: 12
            highlighted: commandBox.highlightedIndex === index
            onClicked: {
                commandBox.currentIndex = index
                backend.command = modelData
            }
        }

        Connections {
            target: backend
            function onCommandChanged() {
                commandBox.currentIndex = Math.max(0, backend.availableCommands.indexOf(backend.command))
            }
            function onCommandsChanged() {
                commandBox.currentIndex = Math.max(0, backend.availableCommands.indexOf(backend.command))
            }
            function onInputChanged() {
                root.advancedOpen = false
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: Format.commandHint(backend.command, backend.decompile)
        color: "#85858f"
        font.pixelSize: 11
        wrapMode: Text.Wrap
        visible: text !== ""
    }

    // ------------------------------------------------------------------
    // IL2CPP metadata
    // ------------------------------------------------------------------
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 5
        visible: backend.command === "il2cpp"

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "global-metadata.dat"
                color: "#c9c9d1"
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            TextButton {
                text: qsTr("Choose")
                onClicked: backend.chooseMetadata()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 32
            color: "#121214"
            radius: Math.min(4, theme.radius)
            border.width: 1
            border.color: backend.metadataPath === "" ? "#4a3a24" : "#2e2e34"

            Text {
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: 9
                    rightMargin: 9
                }
                text: backend.metadataPath !== ""
                      ? backend.metadataPath
                      : qsTr("No metadata file chosen")
                color: backend.metadataPath !== "" ? "#b9b9c2" : "#8a7a5a"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: backend.chooseMetadata()
            }
        }
    }

    // ------------------------------------------------------------------
    // ELF decompiler
    // ------------------------------------------------------------------
    OptionToggle {
        Layout.fillWidth: true
        visible: root.elf
        label: qsTr("Decompile to C")
        description: backend.aarch64
                     ? qsTr("Recover functions, control flow, and struct field access.")
                     : qsTr("C recovery needs an AArch64 binary; this file is %1.")
                       .arg(backend.architecture !== "" ? backend.architecture : qsTr("another architecture"))
        active: backend.decompile
        available: backend.aarch64 && !backend.running
        onToggled: backend.decompile = !backend.active
    }

    OptionToggle {
        Layout.fillWidth: true
        visible: root.elf && backend.decompile
        label: qsTr("Split functions")
        description: qsTr("One .c file per function, plus functions.json.")
        active: backend.splitFunctions
        available: !backend.running
        onToggled: backend.splitFunctions = !backend.active
    }

    OptionToggle {
        Layout.fillWidth: true
        visible: root.elf && backend.decompile
        label: qsTr("Cross-references")
        description: qsTr("Add callers and callees, and write xrefs.json.")
        active: backend.crossReferences
        available: !backend.running
        onToggled: backend.crossReferences = !backend.active
    }

    TextButton {
        visible: root.elf && backend.decompile
        text: root.advancedOpen ? qsTr("Fewer options") : qsTr("More options")
        onClicked: root.advancedOpen = !root.advancedOpen
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: root.elf && backend.decompile && root.advancedOpen

        OptionToggle {
            Layout.fillWidth: true
            label: qsTr("Registers as variables")
            description: qsTr("Name values arg0..arg7 and var_N instead of x0/w0.")
            active: backend.regsAsVars
            available: !backend.running
            onToggled: backend.regsAsVars = !backend.active
        }

        OptionToggle {
            Layout.fillWidth: true
            label: qsTr("Simplify control flow")
            description: qsTr("Drop unreachable blocks and merge linear chains (experimental).")
            active: backend.simplifyCfg
            available: !backend.running
            onToggled: backend.simplifyCfg = !backend.active
        }

        OptionToggle {
            Layout.fillWidth: true
            label: qsTr("Export LLVM IR")
            description: qsTr("Also write <name>.decompiled.ll next to the C.")
            active: backend.emitLLVM
            available: !backend.running
            onToggled: backend.emitLLVM = !backend.active
        }

        OptionToggle {
            Layout.fillWidth: true
            label: qsTr("No location comments")
            description: qsTr("Leave out the address of each lifted function.")
            active: backend.noLocationComments
            available: !backend.running
            onToggled: backend.noLocationComments = !backend.active
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("IL2CPP offsets")
                color: "#c9c9d1"
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            TextButton {
                visible: backend.offsetsPath !== ""
                text: qsTr("Clear")
                onClicked: backend.setOffsetsPath("")
            }

            TextButton {
                text: qsTr("Choose")
                onClicked: backend.chooseOffsets()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 30
            color: "#121214"
            radius: Math.min(4, theme.radius)
            border.width: 1
            border.color: "#2e2e34"

            Text {
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: 9
                    rightMargin: 9
                }
                text: backend.offsetsPath !== ""
                      ? backend.offsetsPath
                      : qsTr("Optional offsets.json or offset.hpp dump")
                color: backend.offsetsPath !== "" ? "#b9b9c2" : "#66666f"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: backend.chooseOffsets()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("Managed metadata")
                color: "#c9c9d1"
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            TextButton {
                visible: backend.elfMetadataPath !== ""
                text: qsTr("Clear")
                onClicked: backend.setElfMetadataPath("")
            }

            TextButton {
                text: qsTr("Choose")
                onClicked: backend.chooseElfMetadata()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 30
            color: "#121214"
            radius: Math.min(4, theme.radius)
            border.width: 1
            border.color: "#2e2e34"

            Text {
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: 9
                    rightMargin: 9
                }
                text: backend.elfMetadataPath !== ""
                      ? backend.elfMetadataPath
                      : qsTr("Optional; auto-detected next to libil2cpp.so")
                color: backend.elfMetadataPath !== "" ? "#b9b9c2" : "#66666f"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: backend.chooseElfMetadata()
            }
        }
    }

    // ------------------------------------------------------------------
    // Shared options
    // ------------------------------------------------------------------
    OptionToggle {
        Layout.fillWidth: true
        visible: backend.command !== "elf-info"
        label: qsTr("Verbose log")
        description: qsTr("Print progress for every class or function.")
        active: backend.verbose
        available: !backend.running
        onToggled: backend.verbose = !backend.active
    }

    Text {
        Layout.fillWidth: true
        visible: backend.command === "hermes"
        text: qsTr("Hermes always writes .hasm text. Edit the file, then reassemble it with destruct assemble.")
        color: "#6f6f78"
        font.pixelSize: 10
        wrapMode: Text.Wrap
    }

    Text {
        Layout.fillWidth: true
        visible: backend.command === "js"
        text: qsTr("Writes <name>.deobf.js into the output folder.")
        color: "#6f6f78"
        font.pixelSize: 10
        wrapMode: Text.Wrap
    }
}
