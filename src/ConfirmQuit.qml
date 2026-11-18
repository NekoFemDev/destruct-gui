import QtQuick
import QtQuick.Layouts

// Quitting mid-job would kill the child on the way out, so the window asks
// first and says exactly what will happen to the output.
Item {
    id: root
    property bool shown: false
    signal cancelled()
    signal confirmed()

    visible: shown
    z: 210
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Rectangle {
        anchors.fill: parent
        color: "#b3000000"
        MouseArea { anchors.fill: parent; onClicked: root.cancelled() }
    }

    Rectangle {
        width: Math.min(parent.width - 64, 440)
        implicitHeight: panel.implicitHeight + 40
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
                margins: 20
            }
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("A job is running")
                color: "#f0f0f3"
                font.pixelSize: 16
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Quitting stops the destruct process and discards its unfinished output.")
                color: "#a0a0a9"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                spacing: 8

                Item { Layout.fillWidth: true }

                ActionButton {
                    implicitWidth: 116
                    text: qsTr("Keep working")
                    primary: false
                    onClicked: root.cancelled()
                }

                ActionButton {
                    implicitWidth: 178
                    text: qsTr("Cancel and quit")
                    danger: true
                    primary: false
                    onClicked: root.confirmed()
                }
            }
        }
    }
}
