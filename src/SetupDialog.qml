import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The first-run setup dialog: without destruct there is no decompiler, so the
// window explains the two honest ways forward - download and build the CLI
// from source, or point the GUI at an existing binary.
Item {
    id: root
    property bool shown: false
    signal closed()

    visible: shown
    z: 205
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    readonly property var installer: backend.setup
    readonly property bool running: installer.running
    readonly property bool failed: !installer.running && installer.error !== ""
    readonly property bool succeeded: !installer.running && installer.succeeded
    readonly property bool ask: !running && !failed && !succeeded
    readonly property bool showLog: running || failed || succeeded

    function shortPath(path) {
        return path.replace(/^\/home\/[^/]+/, "~")
    }

    function logColor(kind) {
        return kind === 1 ? "#ff8a80"
             : kind === 2 ? "#e5b567"
             : kind === 3 ? theme.brandAccent
             : kind === 4 ? "#c9b95a" : "#b9b9c2"
    }

    Rectangle {
        anchors.fill: parent
        color: "#c0000000"

        MouseArea {
            anchors.fill: parent
            onClicked: if (!root.running) root.closed()
        }
    }

    Rectangle {
        width: Math.min(parent.width - 56, 620)
        implicitHeight: content.implicitHeight + 40
        anchors.centerIn: parent
        radius: theme.radius
        color: "#18181b"
        border.width: 1
        border.color: "#3a3a41"

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: content
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
                margins: 20
            }
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: root.running ? qsTr("Setting up DeStruct")
                                      : root.failed ? qsTr("DeStruct setup failed")
                                      : root.succeeded ? qsTr("DeStruct is built")
                                      : qsTr("DeStruct CLI is not installed")
                    color: "#f0f0f3"
                    font.pixelSize: 16
                    font.bold: true
                    elide: Text.ElideRight
                }

                TextButton {
                    visible: !root.running
                    text: qsTr("Close")
                    onClicked: root.closed()
                }
            }

            // ----------------------------------------------------------
            // Ask
            // ----------------------------------------------------------
            Text {
                Layout.fillWidth: true
                visible: root.ask
                text: root.installer.buildSupported
                      ? qsTr("destruct is the command-line decompiler this window drives. It was not found in /usr/bin or on your PATH.\n\nDownload the source from GitHub and build it here, or choose an existing destruct binary.")
                      : qsTr("destruct is the command-line decompiler this window drives. It was not found on your PATH.\n\nAutomatic download and build is available on Linux only. Choose an existing destruct.exe.")
                color: "#a7a7b0"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                lineHeight: 1.15
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.ask && root.installer.buildSupported
                implicitHeight: destination.implicitHeight + 20
                color: "#121214"
                radius: Math.min(4, theme.radius)
                border.width: 1
                border.color: "#2e2e34"

                Text {
                    id: destination
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        margins: 10
                    }
                    text: qsTr("Source and build: %1").arg(root.shortPath(root.installer.sourceDirectory))
                    color: "#b9b9c2"
                    font.pixelSize: 10
                    font.family: "monospace"
                    wrapMode: Text.WrapAnywhere
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.ask && root.installer.buildSupported
                text: qsTr("Needs curl, tar, make, Go, and a C compiler. The first build also downloads Capstone, so it can take a few minutes. The source and binary stay in this folder; Go uses its usual caches.")
                color: "#777780"
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.ask
                spacing: 8

                ActionButton {
                    Layout.fillWidth: true
                    visible: root.installer.buildSupported
                    text: qsTr("Download and build")
                    onClicked: root.installer.install()
                }

                ActionButton {
                    implicitWidth: 168
                    Layout.fillWidth: !root.installer.buildSupported
                    text: qsTr("Choose binary...")
                    primary: !root.installer.buildSupported
                    onClicked: backend.chooseBinary()
                }

                TextButton {
                    text: qsTr("Later")
                    onClicked: root.closed()
                }
            }

            // ----------------------------------------------------------
            // Running
            // ----------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                visible: root.running
                spacing: 10

                BusyIndicator {
                    running: root.running
                    implicitWidth: 18
                    implicitHeight: 18
                }

                Text {
                    Layout.fillWidth: true
                    text: root.installer.status
                    color: "#c9c9d1"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            // ----------------------------------------------------------
            // Error
            // ----------------------------------------------------------
            Text {
                Layout.fillWidth: true
                visible: root.failed
                text: root.installer.error
                color: "#ff8a80"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.succeeded && !backend.binaryReady
                text: qsTr("The build finished, but destruct did not run. Check the log below or choose another binary.")
                color: "#e5b567"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            // ----------------------------------------------------------
            // Build log
            // ----------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                visible: root.showLog
                implicitHeight: 170
                color: "#101012"
                radius: Math.min(4, theme.radius)
                border.width: 1
                border.color: "#2a2a30"
                clip: true

                ListView {
                    id: setupLog
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: root.installer.log
                    spacing: 1
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    onCountChanged: Qt.callLater(positionViewAtEnd)

                    delegate: Item {
                        width: setupLog.width
                        height: logLine.implicitHeight + 2

                        Text {
                            id: logLine
                            x: 0
                            width: parent.width
                            text: model.text
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                            font.family: "monospace"
                            font.pixelSize: 10
                            color: root.logColor(model.kind)
                        }
                    }
                }
            }

            // ----------------------------------------------------------
            // Actions after a failure / finish
            // ----------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                visible: root.failed
                spacing: 8

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Retry")
                    onClicked: root.installer.install()
                }

                ActionButton {
                    implicitWidth: 168
                    text: qsTr("Choose binary...")
                    primary: false
                    onClicked: backend.chooseBinary()
                }

                TextButton {
                    text: qsTr("Close")
                    onClicked: root.closed()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.succeeded
                spacing: 8

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Done")
                    onClicked: root.closed()
                }

                ActionButton {
                    implicitWidth: 168
                    text: qsTr("Choose binary...")
                    primary: false
                    onClicked: backend.chooseBinary()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.running
                spacing: 8

                Item { Layout.fillWidth: true }

                ActionButton {
                    implicitWidth: 150
                    text: root.installer.cancelling ? qsTr("Cancelling...") : qsTr("Cancel")
                    primary: false
                    danger: true
                    enabled: !root.installer.cancelling
                    onClicked: root.installer.cancel()
                }
            }
        }
    }
}
