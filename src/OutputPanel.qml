import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

SectionCard {
    id: root
    title: qsTr("Output folder")

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        TextField {
            id: outputField
            Layout.fillWidth: true
            implicitHeight: 34
            text: backend.outputPath
            font.pixelSize: 11
            color: "#c9c9d1"
            selectByMouse: true
            leftPadding: 9
            rightPadding: 9
            onEditingFinished: {
                if (text.trim() === "")
                    text = backend.outputPath
                else
                    backend.setOutputPath(text)
            }

            background: Rectangle {
                color: "#121214"
                radius: Math.min(4, theme.radius)
                border.width: 1
                border.color: outputField.activeFocus ? theme.accent : "#2e2e34"
            }

            Text {
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: outputField.leftPadding + 2
                    right: outputField.right
                    rightMargin: outputField.rightPadding
                }
                visible: outputField.text === ""
                text: qsTr("Choose an output folder")
                color: "#686871"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Connections {
                target: backend
                function onOutputChanged() {
                    if (!outputField.activeFocus)
                        outputField.text = backend.outputPath
                }
            }
        }

        TextButton {
            text: qsTr("Choose")
            onClicked: backend.chooseOutput()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: backend.command === "js"
                  ? qsTr("Writes %1 inside this folder.").arg(Format.stem(backend.inputPath) + ".deobf.js")
                  : qsTr("Existing files are overwritten.")
            color: "#6f6f78"
            font.pixelSize: 10
            wrapMode: Text.Wrap
        }

        TextButton {
            visible: backend.fileCount > 0
            text: qsTr("Reveal")
            onClicked: backend.openOutputFolder()
        }
    }

    Text {
        Layout.fillWidth: true
        visible: ["outputMissing", "outputIsFile", "outputNotWritable"].indexOf(backend.validationCode) >= 0
        text: Format.validationMessage(backend.validationCode, backend.validationDetail)
        color: "#ff8a80"
        font.pixelSize: 10
        wrapMode: Text.Wrap
    }
}
