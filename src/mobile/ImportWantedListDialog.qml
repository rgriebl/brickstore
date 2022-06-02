import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS


Page {
    id: root
    title: qsTr("Import Wanted List")

    property var goBackFunction

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: "go-previous"
                onClicked: goBackFunction()
            }
            Label {
                Layout.fillWidth: true
                font.pointSize: root.font.pointSize * 1.3
                minimumPointSize: font.pointSize / 2
                fontSizeMode: Text.Fit
                text: root.title
                elide: Label.ElideLeft
                horizontalAlignment: Qt.AlignLeft
            }
            ToolButton {
                icon.name: "view-refresh"
                onClicked: BS.BrickLink.wantedLists.startUpdate()
                enabled: BS.BrickLink.wantedLists.updateStatus !== BS.BrickLink.UpdateStatus.Updating
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: table
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true

            ScrollIndicator.vertical: ScrollIndicator { active: true }

            model: BS.SortFilterProxyModel {
                id: sortFilterModel
                sourceModel: BS.BrickLink.wantedLists
                sortOrder: Qt.AscendingOrder
                sortColumn: 0

                Component.onCompleted: {
                    sortRoleName = "name"
                }
            }

            delegate: ItemDelegate {
                id: delegate
                property int xspacing: 16
                width: ListView.view.width
                height: layout.height + xspacing

                required property BS.WantedList wantedList

                GridLayout {
                    id: layout
                    x: xspacing
                    y: xspacing / 2
                    width: parent.width - 2 * xspacing
                    columnSpacing: xspacing
                    rowSpacing: xspacing / 2
                    columns: 2

                    Label {
                        id: label
                        text: wantedList.name
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: wantedList.description
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: qsTr("%1 items (%2 lots)")
                        .arg(Number(wantedList.itemCount).toLocaleString())
                        .arg(Number(wantedList.lotCount).toLocaleString())
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("%1 items left, %2% filled")
                        .arg(Number(wantedList.itemLeftCount).toLocaleString())
                        .arg(wantedList.filled * 100)
                        Layout.alignment: Qt.AlignRight
                    }
                }

                Rectangle {
                    z: 1
                    antialiasing: true
                    height: 1 / Screen.devicePixelRatio
                    color: "grey"
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.rightMargin: parent.xspacing / 2
                    anchors.leftMargin: anchors.rightMargin
                }
                onClicked: {
                    BS.BrickLink.wantedLists.startFetchLots(wantedList)
                    root.goBackFunction()
                }
            }
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() { BS.BrickLink.wantedLists.startUpdate() })
    }
}
