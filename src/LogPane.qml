import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

Item {
    id: root
    property bool follow: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors { leftMargin: 12; rightMargin: 12 }
                spacing: 8

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    Layout.alignment: Qt.AlignVCenter
                    color: backend.state === 1 ? theme.accent
                         : backend.state === 2 ? "#7bd88f"
                         : backend.state === 3 ? "#ff7b72"
                         : backend.state === 4 ? "#e5b567" : "#5d5d66"
                }

                Text {
                    text: Format.stateLabel(backend.state)
                    color: "#b4b4bd"
                    font.pixelSize: 11
                }

                Text {
                    visible: backend.elapsedMs > 0
                    text: "· " + Format.formatDuration(backend.elapsedMs, translator.language)
                    color: "#6f6f79"
                    font.pixelSize: 10
                }

                Item { Layout.fillWidth: true }

                TextButton {
                    text: root.follow ? qsTr("Following") : qsTr("Follow")
                    selected: root.follow
                    onClicked: {
                        root.follow = true
                        logList.positionViewAtEnd()
                    }
                }

                TextButton {
                    text: qsTr("Copy")
                    onClicked: backend.copyLog()
                }

                TextButton {
                    text: qsTr("Command")
                    onClicked: backend.copyCommand()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            visible: backend.state === 3 && backend.errorText !== ""
            implicitHeight: errorText.implicitHeight + 20
            color: "#241517"
            radius: Math.min(4, theme.radius)
            border.width: 1
            border.color: "#5b2a2e"

            Text {
                id: errorText
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: 10
                }
                text: backend.errorText
                color: "#ff9b93"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: logList
                anchors.fill: parent
                clip: true
                LayoutMirroring.enabled: false
                model: backend.log
                spacing: 0
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                topMargin: 4
                bottomMargin: 8

                onCountChanged: if (root.follow) Qt.callLater(positionViewAtEnd)
                onMovementEnded: root.follow = atYEnd

                delegate: Item {
                    width: ListView.view.width
                    height: logLine.implicitHeight + 3

                    Text {
                        id: logLine
                        x: 14
                        y: 1
                        width: parent.width - 28
                        text: model.text
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        font.family: "monospace"
                        font.pixelSize: 11
                        color: model.kind === 1 ? "#ff8a80"
                             : model.kind === 2 ? "#e5b567"
                             : model.kind === 3 ? theme.accent
                             : model.kind === 4 ? "#8fd18f" : "#b9b9c2"
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                visible: backend.log.count === 0 && backend.state !== 1
                spacing: 6
                width: Math.min(parent.width - 48, 420)

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("No output yet")
                    color: "#b0b0b8"
                    font.pixelSize: 13
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("The destruct command's output will stream here while it runs.")
                    color: "#6a6a73"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
