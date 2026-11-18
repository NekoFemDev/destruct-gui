import QtQuick

// A switch-shaped option row. It is not a Material CheckBox on purpose: the
// whole row is clickable, focusable with Tab, and the visual state always
// comes straight from the backend property.
Item {
    id: root
    property string label: ""
    property string description: ""
    property bool active: false
    property bool available: true
    signal toggled()

    implicitHeight: description !== "" ? 42 : 26
    opacity: available ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        color: hover.hovered && root.available ? "#ffffff08" : "transparent"
        radius: Math.min(4, theme.radius)
    }

    Rectangle {
        id: track
        x: 0
        y: 4
        width: 32
        height: 18
        radius: 9
        color: root.active && root.available ? theme.accent : "#35353d"

        Behavior on color { ColorAnimation { duration: 90 } }

        Rectangle {
            id: knob
            y: 2
            x: root.active ? 16 : 2
            width: 14
            height: 14
            radius: 7
            color: root.active ? theme.accentForeground : "#a4a4ac"

            Behavior on x { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 90 } }
        }
    }

    Column {
        anchors {
            left: track.right
            leftMargin: 10
            right: parent.right
            top: parent.top
        }
        spacing: 1

        Text {
            width: parent.width
            text: root.label
            color: "#dedee4"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Text {
            visible: root.description !== ""
            width: parent.width
            text: root.description
            color: "#85858f"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }

    HoverHandler { id: hover; enabled: root.available }

    MouseArea {
        id: toggleArea
        anchors.fill: parent
        enabled: root.available
        cursorShape: Qt.PointingHandCursor
        activeFocusOnTab: root.available
        onClicked: root.toggled()
        // Space is the window's Run shortcut, so Enter is the keyboard way
        // to flip a focused option.
        Keys.onReturnPressed: root.toggled()
        Keys.onEnterPressed: root.toggled()
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: toggleArea.activeFocus ? 1 : 0
        border.color: theme.accent
        radius: Math.min(4, theme.radius)
    }
}
