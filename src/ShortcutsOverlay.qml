import QtQuick
import QtQuick.Layouts

// Every shortcut in the app, in one place, because a keyboard-first window
// that hides its keys is just a mouse app with extra steps.
Item {
    id: root
    property bool shown: false
    signal closed()

    visible: shown
    z: 200
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Rectangle {
        anchors.fill: parent
        color: "#b3000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.closed()
        }
    }

    Rectangle {
        width: Math.min(parent.width - 64, 620)
        implicitHeight: panel.implicitHeight + 44
        anchors.centerIn: parent
        radius: theme.radius
        color: "#18181b"
        border.width: 1
        border.color: "#393940"

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: panel
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
                margins: 22
            }
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Keyboard shortcuts")
                    color: "#f0f0f3"
                    font.pixelSize: 16
                    font.bold: true
                }

                TextButton {
                    text: qsTr("Close")
                    onClicked: root.closed()
                }
            }

            Repeater {
                model: [
                    { key: "Ctrl+O", what: qsTr("Open a file") },
                    { key: "Space / Ctrl+Enter", what: qsTr("Run the current action") },
                    { key: "Esc", what: qsTr("Cancel the job, or close an overlay") },
                    { key: "Ctrl+F", what: qsTr("Filter files and search their contents") },
                    { key: "Ctrl+1 / 2 / 3", what: qsTr("Switch to Preview, Files, or Log") },
                    { key: "Q / Ctrl+Q", what: qsTr("Quit") },
                    { key: "?", what: qsTr("Show this overlay") }
                ]

                Item {
                    Layout.fillWidth: true
                    implicitHeight: 30

                    Rectangle {
                        id: keyCap
                        width: keyLabel.implicitWidth + 14
                        height: 22
                        anchors.verticalCenter: parent.verticalCenter
                        radius: Math.min(4, theme.radius)
                        color: "#24242a"
                        border.width: 1
                        border.color: "#3a3a42"

                        Text {
                            id: keyLabel
                            anchors.centerIn: parent
                            text: modelData.key
                            color: theme.brandAccent
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Text {
                        anchors {
                            left: keyCap.right
                            leftMargin: 14
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                        text: modelData.what
                        color: "#b9b9c2"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
