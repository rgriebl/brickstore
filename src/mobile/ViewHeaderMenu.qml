import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels
import BrickStore


Menu {
    id: root
    property int field
    property Document document
    property string fieldName: document.model.headerData(field, Qt.Horizontal)
    modal: true
    cascade: false
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Overlay.overlay.width * 2 / 3

    ActionDelegate { action: Action { text: qsTr("Sort ascending by %1").arg(root.fieldName)
            onTriggered: document.model.sort(root.field, Qt.AscendingOrder)
        } }
    ActionDelegate { action: Action { text: qsTr("Sort descending by %1").arg(root.fieldName)
            onTriggered: document.model.sort(root.field, Qt.DescendingOrder)
        } }
    MenuSeparator { }
    ActionDelegate { action: Action { text: qsTr("Additionally sort ascending by %1").arg(root.fieldName)
            onTriggered: document.model.sortAdditionally(root.field, Qt.AscendingOrder)
        } }
    ActionDelegate { action: Action { text: qsTr("Additionally sort descending by %1").arg(root.fieldName)
            onTriggered: document.model.sortAdditionally(root.field, Qt.DescendingOrder)
        } }
    MenuSeparator { }
    ActionDelegate { action: Action { text: qsTr("Resize column %1").arg(root.fieldName) } }
    ActionDelegate { actionName: "view_column_layout_manage" }
    ActionDelegate { actionName: "view_column_layout_save" }
    ActionDelegate { actionName: "view_column_layout_load"
        onTriggered: loadLayoutMenu.popup()
        Menu {
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
                            text: model.name
                            property string id: model.id
                            onTriggered: Qt.callLater(root.document.setColumnLayoutFromId, id)
                        }
                    }
                }
            }
        }
    }
}
