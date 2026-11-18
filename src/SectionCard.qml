import QtQuick
import QtQuick.Layouts

// A quiet panel with a small caps title, used for every block in the left
// column so the window reads as one stack instead of a pile of borders.
Rectangle {
    id: root
    property string title: ""
    default property alias content: body.data

    color: "#151518"
    border.width: 1
    border.color: "#29292e"
    radius: theme.radius

    implicitHeight: outer.implicitHeight + 26

    ColumnLayout {
        id: outer
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 13
        }
        spacing: 9

        Text {
            visible: root.title !== ""
            text: root.title.toUpperCase()
            color: "#83838c"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1.1
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 8
        }
    }
}
