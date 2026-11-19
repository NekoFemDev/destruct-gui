import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

// The first thing the window shows: one clear starting point, the formats
// DeStruct understands, and the jobs that were run before.
Item {
    id: root

    Flickable {
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: content.implicitHeight + 80
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: content
            y: 40
            width: Math.min(parent.width - 56, 620)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 22

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 64
                height: 64
                source: "qrc:/destruct-gui.png"
                sourceSize.width: 160
                sourceSize.height: 160
                smooth: true
                fillMode: Image.PreserveAspectFit
            }

            Column {
                width: parent.width
                spacing: 7

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Drop a file to start")
                    color: "#f0f0f3"
                    font.pixelSize: 23
                    font.bold: true
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("One window for JVM, Android, Hermes, Flutter, native ELF, Unity IL2CPP, and JavaScript.")
                    color: "#8b8b95"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            Flow {
                width: parent.width
                spacing: 6
                layoutDirection: Qt.LeftToRight

                Repeater {
                    model: ["JVM", "DEX / APK", "Hermes", "Flutter", "ELF", "IL2CPP", "JavaScript"]

                    Rectangle {
                        width: chip.implicitWidth + 18
                        height: 24
                        radius: Math.min(4, theme.radius)
                        color: "#1a1a1e"
                        border.width: 1
                        border.color: "#303036"

                        Text {
                            id: chip
                            anchors.centerIn: parent
                            text: modelData
                            color: "#a7a7b0"
                            font.pixelSize: 10
                        }
                    }
                }
            }

            TextButton {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Open a file")
                onClicked: backend.chooseInput()
            }

            Column {
                width: parent.width
                spacing: 0
                visible: backend.recentFiles.length > 0

                RowLayout {
                    width: parent.width
                    height: 34

                    Text {
                        text: qsTr("Recent files").toUpperCase()
                        color: "#83838c"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.1
                    }

                    Item { Layout.fillWidth: true }

                    TextButton {
                        text: qsTr("Clear")
                        onClicked: backend.clearRecent()
                    }
                }

                Repeater {
                    model: backend.recentFiles

                    Rectangle {
                        width: content.width
                        height: 46
                        radius: Math.min(4, theme.radius)
                        color: recentHover.hovered ? "#1d1d21" : "transparent"

                        Rectangle {
                            anchors {
                                top: parent.top
                                bottom: parent.bottom
                                left: parent.left
                                topMargin: 7
                                bottomMargin: 7
                            }
                            width: 2
                            color: "#2e2e34"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors {
                                leftMargin: 14
                                rightMargin: 10
                            }
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: Format.baseName(modelData)
                                    color: "#d5d5dc"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: Format.directoryName(modelData)
                                    color: "#6c6c75"
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                }
                            }

                            Text {
                                text: "→"
                                color: "#5f5f68"
                                font.pixelSize: 12
                            }
                        }

                        HoverHandler { id: recentHover }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.useRecent(modelData)
                        }
                    }
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Press ? for every shortcut, or Ctrl+O to open a file.")
                color: "#5f5f68"
                font.pixelSize: 10
            }
        }
    }
}
