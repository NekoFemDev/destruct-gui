import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

Item {
    id: root

    readonly property bool welcome: backend.inputPath === "" && backend.metadataPath === ""

    function scrollToLine(line) {
        if (line <= 0) {
            if (codeScroll.contentItem && codeScroll.contentItem.contentY !== undefined)
                codeScroll.contentItem.contentY = 0
            return
        }
        var lineHeight = Math.max(12, codeArea.font.pixelSize * 1.45)
        var target = Math.max(0, (line - 1) * lineHeight - 40)
        if (codeScroll.contentItem && codeScroll.contentItem.contentY !== undefined)
            codeScroll.contentItem.contentY = target
    }

    WelcomePane {
        anchors.fill: parent
        visible: root.welcome
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: !root.welcome

        // Header of the selected file.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 52
            color: "transparent"
            visible: backend.hasPreview

            RowLayout {
                anchors.fill: parent
                anchors { leftMargin: 14; rightMargin: 10 }
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: backend.previewName
                        color: "#e4e4e9"
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideMiddle
                    }

                    Text {
                        Layout.fillWidth: true
                        text: backend.previewFolder !== ""
                              ? backend.previewFolder + "  ·  " + Format.formatSize(backend.previewSize, translator.language)
                              : Format.formatSize(backend.previewSize, translator.language)
                        color: "#74747d"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }

                TextButton {
                    text: qsTr("Open")
                    onClicked: backend.openPreviewExternally()
                }

                TextButton {
                    text: qsTr("Reveal")
                    onClicked: backend.openOutputFolder()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#27272c"
            visible: backend.hasPreview
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 26
            visible: backend.hasPreview && backend.previewTruncated
            color: "#211f14"

            RowLayout {
                anchors.fill: parent
                anchors { leftMargin: 14; rightMargin: 10 }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Preview limited to the first 2 MB")
                    color: "#c7b071"
                    font.pixelSize: 10
                }

                TextButton {
                    text: qsTr("Open the full file")
                    onClicked: backend.openPreviewExternally()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                id: codeScroll
                anchors.fill: parent
                visible: backend.hasPreview && backend.previewTextual
                clip: true
                // Source code stays left-to-right even in a mirrored window.
                LayoutMirroring.enabled: false
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                TextArea {
                    id: codeArea
                    text: backend.previewText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    textFormat: TextEdit.PlainText
                    font.family: "monospace"
                    font.pixelSize: 11
                    color: "#cfcfd6"
                    selectionColor: theme.accent
                    selectedTextColor: theme.accentForeground
                    leftPadding: 14
                    rightPadding: 14
                    topPadding: 12
                    bottomPadding: 12
                    background: Rectangle { color: "#101012" }
                }
            }

            // A binary output (or an unsupported text encoding) still deserves
            // a clear explanation and a one-click way into the real file.
            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 60, 460)
                visible: backend.hasPreview && !backend.previewTextual
                spacing: 8

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Binary file")
                    color: "#c9c9d1"
                    font.pixelSize: 15
                    font.bold: true
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("DeStruct can produce machine-readable output that the preview does not render. Open it in an external editor.")
                    color: "#7d7d86"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                TextButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Open externally")
                    onClicked: backend.openPreviewExternally()
                }
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 60, 480)
                visible: !backend.hasPreview
                spacing: 8

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 44
                    height: 44
                    radius: Math.min(8, theme.radius)
                    color: "#1c1c21"
                    border.width: 1
                    border.color: "#2e2e34"

                    // A drawn magnifier rather than a Unicode glyph, so it
                    // stays the same shape in every font and language.
                    Item {
                        anchors.centerIn: parent
                        width: 22
                        height: 22

                        Rectangle {
                            x: 1
                            y: 1
                            width: 14
                            height: 14
                            radius: 7
                            color: "transparent"
                            border.width: 2
                            border.color: theme.accent
                        }

                        Rectangle {
                            x: 13
                            y: 13
                            width: 2
                            height: 8
                            color: theme.accent
                            rotation: -45
                            transformOrigin: Item.TopLeft
                        }
                    }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: backend.running
                          ? qsTr("Working...")
                          : backend.metadataPath !== "" && backend.inputPath === ""
                            ? qsTr("Choose libil2cpp.so to continue")
                            : backend.state === 3
                              ? qsTr("The job failed")
                              : qsTr("Nothing to preview yet")
                    color: "#c9c9d1"
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: backend.running
                          ? qsTr("The log shows progress. Files appear here as soon as the command finishes.")
                          : backend.state === 3
                            ? qsTr("Open the Log tab to see what destruct reported.")
                            : qsTr("Run the action, then choose a file in the Files tab — or press Ctrl+F to search the output.")
                    color: "#777780"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }
        }

        Connections {
            target: backend
            function onPreviewChanged() {
                Qt.callLater(function() { root.scrollToLine(backend.previewLine) })
            }
        }
    }
}
