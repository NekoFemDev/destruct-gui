import QtQuick

// A small flat button for secondary actions in headers and panels.
Rectangle {
    id: root
    property string text: ""
    property bool danger: false
    property bool selected: false
    signal clicked()

    implicitWidth: label.implicitWidth + 18
    implicitHeight: 26
    radius: Math.min(4, theme.radius)
    color: hover.hovered ? (danger ? "#3a2224" : "#26262b")
                         : selected ? "#2a2a2f" : "transparent"

    Behavior on color { ColorAnimation { duration: 80 } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.danger ? "#ff8a80" : root.selected ? "#ececef" : "#a9a9b1"
        font.pixelSize: 12
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
