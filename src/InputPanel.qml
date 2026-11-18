import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

SectionCard {
    id: root
    property bool dragHover: false
    title: qsTr("Input")

    // The empty state doubles as the drop target: one large, obvious place
    // to start, with the supported formats named below it.
    Rectangle {
        visible: backend.inputPath === ""
        Layout.fillWidth: true
        implicitHeight: 126
        radius: Math.min(6, theme.radius)
        color: hover.hovered || root.dragHover ? "#1d1d20" : "#121214"
        border.width: 1
        border.color: hover.hovered || root.dragHover ? theme.accent : "#34343b"

        Behavior on border.color { ColorAnimation { duration: 100 } }

        Column {
            anchors.centerIn: parent
            spacing: 5

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: backend.metadataPath !== ""
                      ? qsTr("Drop libil2cpp.so here")
                      : qsTr("Drop a file here")
                color: "#e4e4e9"
                font.pixelSize: 13
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("or press Ctrl+O")
                color: "#85858f"
                font.pixelSize: 11
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "JVM · DEX/APK · Hermes · Flutter · ELF · PE · IL2CPP · JS"
                color: "#5f5f68"
                font.pixelSize: 10
            }
        }

        HoverHandler { id: hover }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: backend.chooseInput()
        }
    }

    // A metadata file on its own is a valid first step for IL2CPP; the
    // window says so instead of leaving the panel looking empty.
    RowLayout {
        visible: backend.inputPath === "" && backend.metadataPath !== ""
        Layout.fillWidth: true
        spacing: 8

        Rectangle {
            width: 5
            height: 5
            radius: 3
            color: theme.accent
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Metadata chosen: %1").arg(Format.baseName(backend.metadataPath))
            color: "#bdbdc6"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        TextButton {
            text: qsTr("Change")
            onClicked: backend.chooseMetadata()
        }
    }

    Rectangle {
        visible: backend.inputPath !== ""
        Layout.fillWidth: true
        implicitHeight: 96
        radius: Math.min(6, theme.radius)
        color: "#121214"
        border.width: 1
        border.color: "#2e2e34"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: Math.min(6, theme.radius)
                color: "#232329"
                border.width: 1
                border.color: "#35353d"

                Text {
                    anchors.centerIn: parent
                    text: Format.shortTag(backend.formatKind, backend.inputName)
                    color: theme.accent
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 0.5
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: backend.inputName
                    color: "#ececef"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: Format.formatLabel(backend.formatKind, backend.architecture)
                    color: "#9c9ca5"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Text {
                    id: pathLabel
                    Layout.fillWidth: true
                    text: backend.inputPath
                    color: "#686871"
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
            }

            ColumnLayout {
                spacing: 4

                Text {
                    text: Format.formatSize(backend.inputSize, translator.language)
                    color: "#9c9ca5"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight
                }

                TextButton {
                    text: qsTr("Change")
                    Layout.alignment: Qt.AlignRight
                    onClicked: backend.chooseInput()
                }
            }
        }

        HoverHandler { id: cardHover }

        ToolTip.visible: cardHover.hovered
        ToolTip.delay: 500
        ToolTip.text: backend.inputPath
    }
}
