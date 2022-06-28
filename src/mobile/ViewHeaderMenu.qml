pragma ComponentBehavior: Bound

import Mobile
import Qt.labs.qmlmodels
import BrickStore


AutoSizingMenu {
    id: root
    property int field
    required property Document document
    required property var model

    property string fieldName: document.model.headerData(field, Qt.Horizontal)

    modal: true
    cascade: false
    parent: Overlay.overlay
    anchors.centerIn: parent

    ActionDelegate { action: Action { text: qsTr("Sort ascending by %1").arg(root.fieldName)
            onTriggered: root.document.model.sort(root.field, Qt.AscendingOrder)
        } }
    ActionDelegate { action: Action { text: qsTr("Sort descending by %1").arg(root.fieldName)
            onTriggered: root.document.model.sort(root.field, Qt.DescendingOrder)
        } }
    MenuSeparator { }
    ActionDelegate { action: Action { text: qsTr("Additionally sort ascending by %1").arg(root.fieldName)
            onTriggered: root.document.model.sortAdditionally(root.field, Qt.AscendingOrder)
        } }
    ActionDelegate { action: Action { text: qsTr("Additionally sort descending by %1").arg(root.fieldName)
            onTriggered: root.document.model.sortAdditionally(root.field, Qt.DescendingOrder)
        } }
    MenuSeparator { }
    ActionDelegate { action: Action { text: qsTr("Resize column %1").arg(root.fieldName) }
        AutoSizingDialog {
            id: columnDialog
            title: qsTr("Configure Columns")

            ListView {
                anchors.fill: parent
                model: root.model.columnModel

                ScrollIndicator.vertical: ScrollIndicator { }

                delegate: ItemDelegate {
                    id: delegate
                    required property int index
                    required property string name
                    required property bool hidden

                    width: ListView.view.width
                    text: name
                    ToolButton {
                        id: moveDown
                        enabled: delegate.index < (root.model.columnModel.count - 1)
                        height: parent.height
                        width: height
                        anchors.right: parent.right
                        flat: true
                        icon.name: "go-down"
                        onClicked: {
                            root.model.columnModel.moveColumn(index, index + 1)
                        }
                    }
                    ToolButton {
                        id: moveUp
                        enabled: delegate.index > 0
                        height: parent.height
                        width: height
                        anchors.right: moveDown.left
                        flat: true
                        icon.name: "go-up"
                        onClicked: {
                            root.model.columnModel.moveColumn(index, index - 1)
                        }
                    }
                    ToolButton {
                        id: hide
                        height: parent.height
                        width: height
                        anchors.right: moveUp.left
                        flat: true
                        icon.name: delegate.hidden ? "view-hidden" : "view-visible"
                        onClicked: {
                            root.model.columnModel.hideColumn(index, !hidden)
                        }
                    }
                }
            }
        }
        onTriggered: {
            columnDialog.open()
        }
    }
    ActionDelegate { actionName: "view_column_layout_manage" }
    ActionDelegate { actionName: "view_column_layout_save"
        onTriggered: root.document.saveCurrentColumnLayout()
    }
    ActionDelegate { actionName: "view_column_layout_load"
        onTriggered: loadLayoutMenu.popup()
        AutoSizingMenu {
            id: loadLayoutMenu
            modal: true
            cascade: false
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: Overlay.overlay ? Overlay.overlay.width * 2 / 3 : 0

            Repeater {
                model: BrickStore.columnLayouts
                DelegateChooser {
                    role: "id"

                    DelegateChoice {
                        roleValue: "-"
                        MenuSeparator { }
                    }
                    DelegateChoice {
                        MenuItem {
                            required property string id
                            required property string name
                            text: name
                            onTriggered: Qt.callLater(root.document.setColumnLayoutFromId, id)
                        }
                    }
                }
            }
        }
    }
}
