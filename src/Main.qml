import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

ApplicationWindow {
    id: win
    width: 1220
    height: 820
    minimumWidth: 1000
    minimumHeight: 640
    visible: true
    title: backend.inputName !== "" ? qsTr("%1 — DeStruct").arg(backend.inputName) : "DeStruct"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property string accent: theme.accent
    readonly property string accentFg: theme.accentForeground
    readonly property bool rtl: Qt.application.layoutDirection === Qt.RightToLeft
    readonly property bool overlayOpen: helpVisible || quitConfirmVisible
    readonly property bool textEditing: activeFocusItem instanceof TextInput
                                          || activeFocusItem instanceof TextEdit

    property string tab: "preview"
    property bool helpVisible: false
    property bool quitConfirmVisible: false
    property bool quitConfirmed: false
    property bool dragHover: false

    readonly property int tabIndex: tab === "preview" ? 0 : tab === "files" ? 1 : 2

    function requestQuit() {
        if (backend.running)
            quitConfirmVisible = true
        else
            Qt.quit()
    }

    function openSelectedFile(path, line) {
        backend.openPreview(path, line)
        tab = "preview"
    }

    Shortcut {
        sequences: [StandardKey.Open]
        context: Qt.ApplicationShortcut
        enabled: !win.overlayOpen && !backend.running
        onActivated: backend.chooseInput()
    }

    // Space is the main action, but only when the keyboard focus is not in a
    // text field, or every space typed into a path would start a job.
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: !win.textEditing && !win.overlayOpen && !backend.running && backend.canRun
        onActivated: backend.run()
    }

    Shortcut {
        sequence: "Ctrl+Return"
        context: Qt.ApplicationShortcut
        enabled: !win.overlayOpen && backend.canRun
        onActivated: backend.run()
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        enabled: !win.textEditing
        onActivated: win.helpVisible = !win.helpVisible
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (win.quitConfirmVisible)
                win.quitConfirmVisible = false
            else if (backend.running)
                backend.cancel()
        }
    }

    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: !win.textEditing && !win.overlayOpen
        onActivated: win.requestQuit()
    }

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: win.requestQuit()
    }

    Shortcut {
        sequence: "Ctrl+1"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = "preview"
    }

    Shortcut {
        sequence: "Ctrl+2"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = "files"
    }

    Shortcut {
        sequence: "Ctrl+3"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = "log"
    }

    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        enabled: !win.overlayOpen
        onActivated: {
            win.tab = "files"
            filesPane.focusFilter()
        }
    }

    onClosing: function(close) {
        if (backend.running && !win.quitConfirmed) {
            close.accepted = false
            win.quitConfirmVisible = true
        }
    }

    Connections {
        target: backend
        function onStateChanged() {
            if (backend.state === 1) {
                win.tab = "log"
            } else if (backend.state === 2) {
                if (backend.primaryRelativePath !== "" && !backend.collectionResult) {
                    backend.openPreview(backend.primaryRelativePath, 0)
                    win.tab = "preview"
                } else {
                    win.tab = "files"
                }
            } else if (backend.state === 3) {
                win.tab = "log"
            }
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: win.rtl
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ----------------------------------------------------------
            // Header
            // ----------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 58
                color: "#101012"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: "#26262b"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors { leftMargin: 18; rightMargin: 14 }
                    spacing: 10

                    Rectangle {
                        width: 30
                        height: 30
                        radius: Math.min(6, theme.radius)
                        color: theme.accent

                        Text {
                            anchors.centerIn: parent
                            text: "D"
                            color: theme.accentForeground
                            font.pixelSize: 17
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 0

                        Text {
                            text: "DeStruct"
                            color: "#f1f1f4"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Text {
                            text: qsTr("multi-format decompiler")
                            color: "#777780"
                            font.pixelSize: 10
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        id: binaryPill
                        implicitWidth: binaryRow.implicitWidth + 22
                        implicitHeight: 28
                        radius: Math.min(4, theme.radius)
                        color: binaryHover.hovered ? "#202025" : "#17171a"
                        border.width: 1
                        border.color: backend.binaryReady ? "#2c3a2e" : "#3d2f22"

                        RowLayout {
                            id: binaryRow
                            anchors.centerIn: parent
                            spacing: 7

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: backend.binaryProbing ? "#b08a4d"
                                     : backend.binaryReady ? "#7bd88f" : "#e5a15c"
                            }

                            Text {
                                text: backend.binaryProbing
                                      ? qsTr("Checking destruct...")
                                      : backend.binaryReady
                                        ? (backend.binaryVersion !== ""
                                           ? "destruct " + backend.binaryVersion
                                           : qsTr("destruct ready"))
                                        : qsTr("Locate destruct")
                                color: backend.binaryReady ? "#a9c5ad" : "#d8b080"
                                font.pixelSize: 10
                                font.family: "monospace"
                            }
                        }

                        HoverHandler { id: binaryHover }

                        MouseArea {
                            anchors.fill: parent
                            enabled: !backend.running
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: backend.chooseBinary()
                        }

                        ToolTip.visible: binaryHover.hovered
                        ToolTip.delay: 400
                        ToolTip.text: backend.binaryPath !== ""
                                      ? backend.binaryPath
                                      : qsTr("Choose the destruct binary")
                    }

                    Rectangle {
                        width: 1
                        height: 20
                        color: "#2a2a30"
                    }

                    Rectangle {
                        width: 28
                        height: 28
                        radius: Math.min(4, theme.radius)
                        color: helpHover.hovered ? "#242429" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "?"
                            color: "#a9a9b1"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        HoverHandler { id: helpHover }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: win.helpVisible = true
                        }

                        ToolTip.visible: helpHover.hovered
                        ToolTip.text: qsTr("Keyboard shortcuts")
                    }
                }
            }

            // ----------------------------------------------------------
            // Main panes
            // ----------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    anchors.fill: parent
                    anchors {
                        leftMargin: 16
                        rightMargin: 16
                        topMargin: 14
                        bottomMargin: 12
                    }
                    spacing: 14

                    // --------------------------------------------------
                    // Left column: the job
                    // --------------------------------------------------
                    ColumnLayout {
                        Layout.preferredWidth: 392
                        Layout.minimumWidth: 340
                        Layout.maximumWidth: 460
                        Layout.fillHeight: true
                        spacing: 12

                        ScrollView {
                            id: leftScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                            ColumnLayout {
                                width: leftScroll.availableWidth
                                spacing: 12

                                InputPanel {
                                    Layout.fillWidth: true
                                    opacity: backend.running ? 0.62 : 1
                                    dragHover: win.dragHover
                                    Behavior on opacity { NumberAnimation { duration: 120 } }
                                    enabled: !backend.running
                                }

                                OptionsPanel {
                                    Layout.fillWidth: true
                                    opacity: backend.running ? 0.62 : 1
                                    visible: backend.inputPath !== "" || backend.metadataPath !== ""
                                    enabled: !backend.running
                                    Behavior on opacity { NumberAnimation { duration: 120 } }
                                }

                                OutputPanel {
                                    Layout.fillWidth: true
                                    opacity: backend.running ? 0.62 : 1
                                    visible: backend.inputPath !== "" || backend.metadataPath !== ""
                                    enabled: !backend.running
                                    Behavior on opacity { NumberAnimation { duration: 120 } }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: backend.running && backend.activity !== ""
                            text: backend.activity
                            color: "#6f6f79"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: (backend.inputPath !== "" || backend.metadataPath !== "")
                                     && backend.validationCode !== "" && !backend.running
                            text: Format.validationMessage(backend.validationCode,
                                                           backend.validationDetail)
                            color: "#ff8a80"
                            font.pixelSize: 10
                            wrapMode: Text.Wrap
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            visible: !backend.running
                                     && (backend.inputPath !== "" || backend.metadataPath !== "")
                            text: Format.runLabel(backend.command, backend.decompile)
                            hint: "Space"
                            enabled: backend.canRun
                            onClicked: backend.run()
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            visible: backend.running
                            text: backend.cancelling ? qsTr("Stopping...") : qsTr("Cancel")
                            hint: backend.cancelling ? "" : "Esc"
                            primary: false
                            danger: true
                            enabled: !backend.cancelling
                            onClicked: backend.cancel()
                        }
                    }

                    // --------------------------------------------------
                    // Right column: results
                    // --------------------------------------------------
                    Rectangle {
                        id: rightPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: theme.radius
                        color: "#131315"
                        border.width: 1
                        border.color: "#29292e"
                        clip: true

                        Rectangle {
                            id: progressLine
                            visible: backend.running
                            y: 0
                            height: 2
                            width: rightPanel.width * 0.35
                            color: theme.accent
                            radius: 1

                            SequentialAnimation on x {
                                running: backend.running
                                loops: Animation.Infinite
                                NumberAnimation {
                                    from: 0
                                    to: rightPanel.width * 0.62
                                    duration: 950
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    from: rightPanel.width * 0.62
                                    to: 0
                                    duration: 950
                                    easing.type: Easing.InOutQuad
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                spacing: 0

                                PaneTab {
                                    text: qsTr("Preview")
                                    active: win.tab === "preview"
                                    onClicked: win.tab = "preview"
                                }

                                PaneTab {
                                    text: backend.fileCount > 0
                                          ? qsTr("Files (%1)").arg(backend.fileCount)
                                          : qsTr("Files")
                                    active: win.tab === "files"
                                    onClicked: win.tab = "files"
                                }

                                PaneTab {
                                    text: qsTr("Log")
                                    active: win.tab === "log"
                                    onClicked: win.tab = "log"
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    Layout.rightMargin: 14
                                    text: Format.stateLabel(backend.state)
                                    color: "#6f6f79"
                                    font.pixelSize: 10
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#27272c"
                            }

                            StackLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                currentIndex: win.tabIndex

                                PreviewPane {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                }

                                FilesPane {
                                    id: filesPane
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    onOpenFile: function(path, line) {
                                        win.openSelectedFile(path, line)
                                    }
                                }

                                LogPane {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                }
                            }
                        }
                    }
                }
            }

            // ----------------------------------------------------------
            // Status bar
            // ----------------------------------------------------------
            Rectangle {
                id: statusBar
                Layout.fillWidth: true
                implicitHeight: 28
                color: "#0c0c0e"

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: "#232328"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors { leftMargin: 16; rightMargin: 16 }
                    spacing: 8

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: backend.state === 1 ? theme.accent
                             : backend.state === 2 ? "#7bd88f"
                             : backend.state === 3 ? "#ff7b72"
                             : backend.state === 4 ? "#e5b567" : "#5d5d66"
                    }

                    Text {
                        text: Format.stateLabel(backend.state)
                        color: "#8d8d96"
                        font.pixelSize: 10
                    }

                    Text {
                        visible: backend.elapsedMs > 0
                        text: "· " + Format.formatDuration(backend.elapsedMs, translator.language)
                        color: "#63636c"
                        font.pixelSize: 10
                    }

                    Text {
                        visible: backend.running && backend.activity !== ""
                        Layout.fillWidth: true
                        text: backend.activity
                        color: "#63636c"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: !backend.running || backend.activity === ""
                    }

                    Text {
                        visible: backend.state === 2
                        text: Format.fileCountLabel(backend.fileCount)
                              + " · " + Format.formatSize(backend.resultBytes, translator.language)
                        color: "#7f9a84"
                        font.pixelSize: 10
                    }

                    Text {
                        visible: backend.outputPath !== ""
                        text: backend.outputPath
                        color: "#5f5f68"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 380
                    }

                    Text {
                        visible: backend.outputPath === ""
                        text: qsTr("Ctrl+O open · Space run · ? shortcuts")
                        color: "#5f5f68"
                        font.pixelSize: 10
                    }
                }
            }
        }

        // Drop anywhere in the window.
        Rectangle {
            anchors.fill: parent
            visible: win.dragHover
            color: "#d90e0e10"
            z: 150

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: theme.radius
                color: "transparent"
                border.width: 2
                border.color: theme.accent
            }

            Column {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Release to open")
                    color: "#f2f2f5"
                    font.pixelSize: 20
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Files become the input; a folder becomes the output.")
                    color: "#9b9ba5"
                    font.pixelSize: 11
                }
            }
        }

        DropArea {
            anchors.fill: parent
            z: 151

            onEntered: win.dragHover = true
            onExited: win.dragHover = false
            onDropped: function(drag) {
                win.dragHover = false
                if (!drag.hasUrls)
                    return
                for (var i = 0; i < drag.urls.length; ++i)
                    backend.acceptDrop(drag.urls[i])
            }
        }

        // A quiet toast for picker failures and clipboard confirmations.
        Rectangle {
            visible: backend.noticeText !== ""
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 42
            }
            width: Math.min(parent.width - 80, noticeText.implicitWidth + 28)
            height: 34
            radius: Math.min(6, theme.radius)
            color: "#232328"
            border.width: 1
            border.color: "#3b3b43"
            z: 160

            Text {
                id: noticeText
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: 14
                }
                text: backend.noticeText
                color: "#d6d6dc"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            MouseArea {
                anchors.fill: parent
                onClicked: backend.dismissNotice()
            }
        }
    }

    ShortcutsOverlay {
        anchors.fill: parent
        shown: win.helpVisible
        onClosed: win.helpVisible = false
    }

    ConfirmQuit {
        anchors.fill: parent
        shown: win.quitConfirmVisible
        onCancelled: win.quitConfirmVisible = false
        onConfirmed: {
            win.quitConfirmed = true
            backend.cancel()
            Qt.quit()
        }
    }
}
