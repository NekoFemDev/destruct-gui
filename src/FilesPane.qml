import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

// The output browser: filter by path, or search inside every text file. Both
// modes share one field, so there is one obvious place to type.
Item {
    id: root
    signal openFile(string path, int line)
    property bool caseSensitive: false

    function focusFilter() {
        filterField.forceActiveFocus()
        filterField.selectAll()
    }

    function startSearch() {
        if (filterField.text.trim().length < 2) {
            backend.clearSearch()
            return
        }
        backend.searchOutput(filterField.text, root.caseSensitive)
    }

    // A new job produces a new file set; carrying the previous filter into it
    // would make an empty list look like a failed run.
    Connections {
        target: backend
        function onInputChanged() {
            if (filterField.text !== "")
                filterField.text = ""
        }
        function onStateChanged() {
            if (backend.state === 1 && filterField.text !== "")
                filterField.text = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 54
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors { leftMargin: 12; rightMargin: 12 }
                spacing: 6

                TextField {
                    id: filterField
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    implicitHeight: 32
                    font.pixelSize: 12
                    color: "#d6d6dc"
                    selectByMouse: true
                    leftPadding: 10
                    rightPadding: 10

                    background: Rectangle {
                        color: "#121214"
                        radius: Math.min(4, theme.radius)
                        border.width: 1
                        border.color: filterField.activeFocus ? theme.brandAccent : "#2e2e34"
                    }

                    onTextChanged: {
                        if (backend.searchQuery !== "")
                            backend.clearSearch()
                        backend.filterFiles(text)
                    }
                    onAccepted: root.startSearch()

                    Text {
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                            leftMargin: parent.leftPadding + 2
                            right: parent.right
                            rightMargin: parent.rightPadding
                        }
                        visible: filterField.text === ""
                        text: qsTr("Filter files, Enter searches contents")
                        color: "#686871"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: 30
                    implicitHeight: 30
                    radius: Math.min(4, theme.radius)
                    color: root.caseSensitive ? theme.brandAccent : "#1b1b1f"
                    border.width: 1
                    border.color: root.caseSensitive ? theme.brandAccent : "#323239"

                    Text {
                        anchors.centerIn: parent
                        text: "Aa"
                        color: root.caseSensitive ? theme.brandAccentForeground : "#8f8f98"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.caseSensitive = !root.caseSensitive
                    }
                }

                TextButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Find")
                    onClicked: root.startSearch()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors { leftMargin: 14; rightMargin: 12 }
                spacing: 6

                Text {
                    text: backend.searchQuery !== ""
                          ? (backend.searching ? qsTr("Searching...")
                                               : Format.matchCountLabel(backend.searchCount))
                          : Format.filteredCountLabel(backend.files.count, backend.fileCount,
                                                      translator.language)
                    color: "#777780"
                    font.pixelSize: 10
                }

                BusyIndicator {
                    visible: backend.searching
                    running: backend.searching
                    implicitWidth: 14
                    implicitHeight: 14
                }

                Item { Layout.fillWidth: true }

                Text {
                    visible: backend.searchQuery === "" && backend.fileCount > 0
                    text: qsTr("Enter searches inside the files")
                    color: "#5f5f68"
                    font.pixelSize: 10
                }

                TextButton {
                    visible: backend.searchQuery !== ""
                    text: qsTr("Clear")
                    onClicked: backend.clearSearch()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#27272c"
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: fileList
                anchors.fill: parent
                visible: backend.searchQuery === ""
                clip: true
                model: backend.files
                spacing: 0
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: row
                    width: ListView.view.width
                    height: 46
                    color: rowHover.hovered ? "#1d1d21"
                                            : previewActive ? "#18181c" : "transparent"

                    readonly property bool previewActive: backend.previewRelativePath === model.relativePath

                    Rectangle {
                        width: 2
                        height: parent.height
                        color: row.previewActive ? theme.brandAccent : "transparent"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors {
                            leftMargin: 14
                            rightMargin: 14
                            topMargin: 5
                            bottomMargin: 5
                        }
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: model.fileName
                                color: "#d9d9df"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: model.folder !== ""
                                text: model.folder
                                color: "#6e6e77"
                                font.pixelSize: 10
                                elide: Text.ElideMiddle
                            }
                        }

                        Text {
                            text: Format.formatSize(model.size, translator.language)
                            color: "#777780"
                            font.pixelSize: 10
                        }
                    }

                    HoverHandler { id: rowHover }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openFile(model.relativePath, 0)
                    }
                }
            }

            ListView {
                id: searchList
                anchors.fill: parent
                visible: backend.searchQuery !== ""
                clip: true
                model: backend.searchResults
                spacing: 0
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: searchRow
                    width: ListView.view.width
                    height: 48
                    color: searchHover.hovered ? "#1d1d21" : "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors {
                            leftMargin: 14
                            rightMargin: 14
                            topMargin: 5
                            bottomMargin: 5
                        }
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: model.file + ":" + model.line
                            color: theme.brandAccent
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }

                        Text {
                            Layout.fillWidth: true
                            text: model.text
                            color: "#a9a9b2"
                            font.family: "monospace"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    HoverHandler { id: searchHover }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openFile(model.file, model.line)
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 6
                width: Math.min(parent.width - 48, 420)

                visible: backend.searchQuery === ""
                         ? backend.fileCount === 0
                         : (!backend.searching && backend.searchCount === 0)

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: backend.searchQuery !== ""
                          ? qsTr("No matches")
                          : backend.running ? qsTr("Results will appear when the job finishes")
                                            : backend.state === 3 ? qsTr("The job failed. Check the log")
                                                                 : backend.inputPath === "" ? qsTr("Choose a file first")
                                                                                            : qsTr("No files yet. Run the action to produce output")
                    color: "#b0b0b8"
                    font.pixelSize: 13
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: backend.searchQuery !== ""
                          ? qsTr("Try a shorter query, or turn off case matching.")
                          : qsTr("The output folder will show up here.")
                    color: "#6a6a73"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }

            Text {
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    leftMargin: 14
                    bottomMargin: 8
                }
                visible: backend.files.truncated
                text: qsTr("Only the first 50,000 files are listed")
                color: "#b58a4d"
                font.pixelSize: 10
            }
        }
    }
}
