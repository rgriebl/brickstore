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
    Loader {
        id: settingsDialog
        function openPage(page) {
            pageName = page
            active = true
        }

        active: false
        source: "SettingsDialog.qml"
        onLoaded: {
            item.onClosed.connect(() => { settingsDialog.active = false })
            item.openPage(pageName)
        }
        property string pageName
    }

    SwipeView {
        id: homeAndView
        interactive: false
        anchors.fill: parent

        StackView {
            id: homeStack

            initialItem: Page {
                header: ToolBar {
                    id: toolBar

                    // The text color might be off after switching themes:
                    // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

                    RowLayout {
                        anchors.fill: parent
                        ToolButton {
                            icon.name: "application-menu"
                            onClicked: applicationMenu.open()

                            AutoSizingMenu {
                                id: applicationMenu
                                x: 0
                                transformOrigin: Menu.TopLeft
                                modal: true
                                cascade: false

                                MenuItem { action: ActionManager.quickAction("configure") }
                                MenuItem { action: ActionManager.quickAction("update_database") }
                                MenuItem { action: ActionManager.quickAction("help_about") }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Label {
                        anchors.centerIn: parent
                        scale: 1.3
                        text: root.currentView && root.currentView.title ? root.currentView.title : root.title
                        elide: Label.ElideLeft
                        horizontalAlignment: Qt.AlignHCenter
                    }
                }

                Flickable {
                    anchors.fill: parent

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
                                    required property var document
                                    // preferably document should be of type Document, but that leads
                                    // to a weird warning whenever document is removed from the model

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
        }
        Item { id: viewProxy }
    }

    function indexOfDocument(doc) {
        for (let i = 0; i < views.length; ++i) {
            if (views[i].document === doc)
                return i
        }
        return -1
    }

    property var views: []
    property View currentView: null

    function setActiveDocument(doc) {
        let index = indexOfDocument(doc)
        let view = index >= 0 ? views[index] : null

        if (currentView !== view) {
            if (currentView) {
                currentView.parent = null
                currentView.visible = false
            }
            currentView = view
            if (view) {
                view.parent = viewProxy
                view.anchors.fill = viewProxy
                view.visible = true
            }
        }
        ActionManager.activeDocument = view ? doc : null
        homeAndView.setCurrentIndex(view ? 1: 0)
    }

    Connections {
        target: BrickStore.documents

        function onDocumentAdded(doc) {
            doc.requestActivation.connect(() => { root.setActiveDocument(doc) })
            doc.closeAllViews.connect(
                        () => {
                            let index = indexOfDocument(doc)
                            if (index >= 0) {
                                if (views[index] === currentView) {
                                    setActiveDocument(null)
                                }
                                views.splice(index, 1).forEach(v => v.destroy())
                                doc.deref()
                            }
                        })

            let viewComponent = Qt.createComponent("View.qml")
            if (viewComponent.status === Component.Ready) {
                let view = viewComponent.createObject(viewProxy, { visible: false, document: doc })
                if (view) {
                    doc.ref()
                    views.push(view)
                    setActiveDocument(doc)
                } else {
                    console.error("could not create a View item")
                }
            } else {
                console.error(viewComponent.errorString())
            }
        }
    }

    Connections {
        target: BrickStore
        function onShowSettings(page) { settingsDialog.openPage(page) }
    }

    Component.onDestruction:  {
        views.forEach(v => v.destroy())
        views = []
    }

    property var connectionContext

    Component.onCompleted: {
        connectionContext = ActionManager.connectQuickActionTable
                ({
                     "document_import_bl_inv": () => {
                         setActiveDocument(null)
                         homeStack.push("ImportInventoryDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                     },
                     "document_import_bl_order": () => {
                         setActiveDocument(null)
                         homeStack.push("ImportOrderDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                     },
                     "document_import_bl_cart": () => {
                         setActiveDocument(null)
                         homeStack.push("ImportCartDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                     },
                     "update_database": () => { BrickStore.updateDatabase() },
                     "help_about": () => { aboutDialog.active = true }
                 })
    }
}
