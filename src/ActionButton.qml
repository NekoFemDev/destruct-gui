import QtQuick
import QtQuick.Layouts

// The one loud button in the window. primary is the accent-filled Run action;
// non-primary is the quiet outline used for Cancel and secondary actions.
Rectangle {
    id: root
    property string text: ""
    property string hint: ""
    property bool primary: true
    property bool danger: false
    signal clicked()

    implicitHeight: 44
    radius: theme.radius
    color: {
        if (!enabled)
            return "#202024"
        if (primary)
            return hover.hovered ? Qt.lighter(theme.brandAccent, 1.08) : theme.brandAccent
        if (danger)
            return hover.hovered ? "#3a2023" : "#211619"
        return hover.hovered ? "#24242a" : "#19191d"
    }
    border.width: primary ? 0 : 1
    border.color: danger ? "#5b2a2e" : "#34343b"
    opacity: enabled ? 1 : 0.6

    Behavior on color { ColorAnimation { duration: 90 } }

    RowLayout {
        anchors.centerIn: parent
        spacing: 10

        Text {
            text: root.text
            color: root.primary ? theme.brandAccentForeground
                                : root.danger ? "#ff8a80" : "#e7e7eb"
            font.pixelSize: 13
            font.bold: true
        }

        Rectangle {
            visible: root.hint !== ""
            implicitWidth: hintLabel.implicitWidth + 12
            implicitHeight: 20
            radius: Math.min(4, theme.radius)
            color: root.primary ? Qt.rgba(0, 0, 0, 0.16) : "#26262d"

            Text {
                id: hintLabel
                anchors.centerIn: parent
                text: root.hint
                color: root.primary ? theme.brandAccentForeground : "#9d9da5"
                font.pixelSize: 10
                opacity: 0.85
            }
        }
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
