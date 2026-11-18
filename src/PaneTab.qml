import QtQuick

// One tab in the right panel. Text and a 2 px accent underline; no Material
// control so the tab strip can stay flat and dense.
Item {
    id: root
    property string text: ""
    property bool active: false
    signal clicked()

    implicitWidth: label.implicitWidth + 28
    implicitHeight: 46

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.active ? "#f1f1f4" : hover.hovered ? "#c7c7ce" : "#898992"
        font.pixelSize: 12
        font.bold: root.active
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 2
        color: root.active ? theme.accent : "transparent"
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
