import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.qmlmodels
import BrickStore 1.0
import QtQuick.Controls.Material
import "./uihelpers" as UIHelpers

ApplicationWindow {
    id: root
    visible: true
    title: "BrickStore"
    width: 800
    height: 600

    Material.theme: {
        switch (BrickStore.config.uiTheme) {
        case Config.Light: return Material.Light
        case Config.Dark: return Material.Dark
        default:
        case Config.SystemDefault: return Material.System
        }
    }

    Loader {
        id: aboutDialog

        active: false
        source: "AboutDialog.qml"
        onLoaded: {
            item.onClosed.connect(() => { aboutDialog.active = false })
            item.open()
        }
    }

    header: ToolBar {
        id: toolBar

        // The text color might be off after switching themes:
        // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: stack.depth > 1 ? "go-home" : "application-menu"
                onClicked: stack.depth > 1 ? stack.pop() : menu.open()

                AutoSizingMenu {
                    id: menu
                    x: 0
                    transformOrigin: Menu.TopLeft
                    modal: true
                    cascade: false

                    MenuItem { action: ActionManager.quickAction("configure") }
                    MenuItem {
                        action: ActionManager.quickAction("update_database")
                        onTriggered: { updateDatabase() }
                    }
                    MenuItem {
                        action: ActionManager.quickAction("help_about")
                        onTriggered: aboutDialog.active = true
                    }
                }
            }
            ToolButton {
                action: ActionManager.quickAction("edit_undo")
                display: Button.IconOnly
                visible: stack.depth > 1
            }
            ToolButton {
                action: ActionManager.quickAction("edit_redo")
                display: Button.IconOnly
                visible: stack.depth > 1
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                icon.name: "overflow-menu"
                onClicked: viewMenu.open()
                visible: stack.depth > 1

                AutoSizingMenu {
                    id: viewMenu
                    x: parent.width - width
                    transformOrigin: Menu.TopRight
                    modal: true
                    cascade: false

                    MenuItem { action: ActionManager.quickAction("document_save") }
                    MenuItem { action: ActionManager.quickAction("document_save_as") }
                    AutoSizingMenu {
                        title: ActionManager.quickAction("document_export").text
                        modal: true
                        cascade: false
                        MenuItem { action: ActionManager.quickAction("document_export_bl_xml") }
                        MenuItem { action: ActionManager.quickAction("document_export_bl_xml_clip") }
                        MenuItem { action: ActionManager.quickAction("document_export_bl_update_clip") }
                        MenuItem { action: ActionManager.quickAction("document_export_bl_wantedlist_clip") }
                    }
                }
            }
        }
        Label {
            anchors.centerIn: parent
            scale: 1.3
            text: stack.currentItem && stack.currentItem.title ? stack.currentItem.title : root.title
            elide: Label.ElideLeft
            horizontalAlignment: Qt.AlignHCenter
        }
    }


    StackView {
        id: stack

        anchors.fill: parent

        initialItem: Flickable {
            //anchors.fill: parent
            ScrollIndicator.vertical: ScrollIndicator { }
            contentHeight: flickGrid.implicitHeight
            GridLayout {
                id: flickGrid
                width: parent.width
                columnSpacing: 20
                columns: Screen.primaryOrientation === Qt.PortraitOrientation ? 1 : 2

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    Layout.fillWidth: true
                    Label {
                        text: qsTranslate("WelcomeWidget", "Document")
                        color: Material.hintTextColor
                        leftPadding: 8
                    }
                    ActionDelegate { action: ActionManager.quickAction("document_open") }
                    ActionDelegate { action: ActionManager.quickAction("document_new") }

                    Label {
                        text: qsTranslate("WelcomeWidget", "Import items")
                        color: Material.hintTextColor
                        leftPadding: 8
                    }
                    ActionDelegate { action: ActionManager.quickAction("document_import_bl_inv") }
                    ActionDelegate { action: ActionManager.quickAction("document_import_bl_xml") }
                    ActionDelegate { action: ActionManager.quickAction("document_import_bl_order") }
                    ActionDelegate { action: ActionManager.quickAction("document_import_bl_store_inv") }
                    ActionDelegate { action: ActionManager.quickAction("document_import_bl_cart") }
                    ActionDelegate { action: ActionManager.quickAction("document_import_ldraw_model") }

                    Label {
                        text: qsTranslate("WelcomeWidget", "Currently Open Documents")
                        color: Material.hintTextColor
                        visible: BrickStore.documents.count !== 0
                        leftPadding: 8
                    }

                    Repeater {
                        model: BrickStore.documents

                        ActionDelegate {
                            required property string fileNameOrTitle
                            required property Document document
                            icon.source: "qrc:/assets/generated-app-icons/brickstore_doc"
                            text: fileNameOrTitle
                            onClicked: setActiveDocument(document)
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    Layout.fillWidth: true
                    Label {
                        text: qsTranslate("WelcomeWidget", "Open recent files")
                        color: Material.hintTextColor
                        leftPadding: 8
                    }

                    Repeater {
                        model: BrickStore.recentFiles

                        ActionDelegate {
                            required property int index
                            required property string fileName
                            required property string filePath
                            icon.source: "qrc:/assets/generated-app-icons/brickstore_doc"
                            text: fileName
                            infoText: filePath
                            onClicked: BrickStore.recentFiles.open(index)
                        }
                    }
                    ActionDelegate {
                        text: qsTranslate("WelcomeWidget", "No recent files")
                        visible: BrickStore.recentFiles.count === 0
                        enabled: false
                    }
                }
            }
        }
    }

    function indexOfView(view) {
        for (let i = 0; i < views.length; ++i) {
            if (views[i] === view)
                return i
        }
        return -1
    }
    function indexOfDocument(doc) {
        for (let i = 0; i < views.length; ++i) {
            if (views[i].document === doc)
                return i
        }
        return -1
    }

    property var views: []

    function setActiveDocument(doc) {
        let index = indexOfDocument(doc)
        console.log("Go to doc: " + doc + " @" + index)
        if (index >= 0) {
            let view = views[index]
            if (stack.depth == 1) {
                console.log("pushing");
                stack.push(view)
                console.log("done");
            } else if (stack.get(1) !== view) {
                stack.replace(stack.get(1), view)
            }
            ActionManager.setActiveDocument(view.document)
        } else {
            stack.pop(null)
            ActionManager.setActiveDocument(null)
        }
    }

    Connections {
        target: BrickStore.documents

        function onDocumentAdded(doc) {
            console.log("Added doc: " + doc)
            doc.requestActivation.connect(() => { root.setActiveDocument(doc) })

            let viewComponent = Qt.createComponent("View.qml")
            if (viewComponent.status === Component.Ready) {
                let view = viewComponent.createObject(stack, { visible: false, document: doc })
                if (view) {
                    views.push(view)
                    setActiveDocument(doc)
                } else {
                    console.error("could not create a View item")
                }
            } else {
                console.error(viewComponent.errorString())
            }
        }
        function onDocumentRemoved(doc) {
            let index = indexOfDocument(doc)
            console.log("Removed doc: " + doc + " @" + index)
            if (index >= 0)
                views.splice(index, 1).forEach(v => v.destroy())
        }
    }

    Loader {
        id: settings
        function openPage(page) {
            pageName = page
            active = true
        }

        active: false
        source: "SettingsDialog.qml"
        onLoaded: {
            item.onClosed.connect(() => { settings.active = false })
            item.openPage(pageName)
        }
        property string pageName
    }

    Connections {
        target: BrickStore
        function onShowSettings(page) { settings.openPage(page) }
    }

    Component.onDestruction:  {
        views.forEach(v => v.destroy())
        views = []
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            if (!BrickStore.databaseValid)
                updateDatabase()
        })
    }

    function updateDatabase() {
        BrickStore.updateDatabase()
    }
}
