// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import QtQuick.Window
import BrickStore as BS


Control {
    id: root
    anchors.fill: parent

    DialogLoader {
        id: aboutDialog
        source: "AboutDialog.qml"
    }
    DialogLoader {
        id: devConsole
        source: "DeveloperConsole.qml"
    }

    Popup {
        // iOS: black bar over the notch in portrait mode
        modal: false
        enabled: false
        closePolicy: Popup.NoAutoClose
        z: 1000
        background: Rectangle { color: "black" }
        visible: Style.leftScreenMargin || Style.rightScreenMargin
        width: Style.leftScreenMargin ? Style.leftScreenMargin : Style.rightScreenMargin
        y: 0
        x: Style.leftScreenMargin ? 0 : parent.width - width
        height: parent.height
    }

    SwipeView {
        id: homeAndView
        interactive: false
        anchors.fill: parent
        anchors.bottomMargin: Style.bottomScreenMargin
        anchors.leftMargin: Style.leftScreenMargin
        anchors.rightMargin: Style.rightScreenMargin

        StackView {
            id: homeStack

            initialItem: Page {
                header: ToolBar {
                    id: toolBar

                    topPadding: Style.topScreenMargin

                    // The text color might be off after switching themes:
                    // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

                    RowLayout {
                        anchors.fill: parent
                        ToolButton {
                            Layout.leftMargin: (Style.leftScreenMargin + Style.rightScreenMargin) / 2
                            icon.name: "application-menu"
                            onClicked: applicationMenu.open()

                            AutoSizingMenu {
                                id: applicationMenu
                                x: parent.width / 2
                                y: parent.height / 2
                                transformOrigin: Menu.TopLeft
                                modal: true
                                cascade: false

                                MenuItem { action: BS.ActionManager.quickAction("configure") }
                                MenuItem { action: BS.ActionManager.quickAction("update_database") }
                                MenuSeparator { }
                                MenuItem { action: BS.ActionManager.quickAction("help_systeminfo") }
                                MenuItem { action: BS.ActionManager.quickAction("help_about") }
                            }

                            onPressAndHold: debugMenu.open()

                            AutoSizingMenu {
                                id: debugMenu
                                x: parent.width / 2
                                y: parent.height / 2
                                transformOrigin: Menu.TopLeft
                                modal: true
                                cascade: false

                                MenuItem {
                                    text: "Show Tracers"
                                    checkable: true
                                    checked: BS.BrickStore.debug.showTracers
                                    onCheckedChanged: BS.BrickStore.debug.showTracers = checked
                                }
                                MenuSeparator { }
                                MenuItem {
                                    text: "Error Log..."
                                    onClicked: { devConsole.open() }
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            font.pointSize: ApplicationWindow.window.font.pointSize * 1.3
                            minimumPointSize: font.pointSize / 2
                            fontSizeMode: Text.Fit
                            text: ApplicationWindow.window.title
                            elide: Label.ElideLeft
                            horizontalAlignment: Qt.AlignLeft
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                Flickable {
                    anchors.fill: parent
                    clip: true

                    ScrollIndicator.vertical: ScrollIndicator { active: true }
                    contentHeight: flickGrid.implicitHeight
                    GridLayout {
                        id: flickGrid
                        width: parent.width
                        columnSpacing: 20
                        columns: Screen.primaryOrientation === Qt.PortraitOrientation ? 1 : 2

                        ColumnLayout {
                            Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                            Layout.minimumWidth: implicitWidth
                            Layout.preferredWidth: implicitWidth
                            Layout.maximumWidth: flickGrid.width / flickGrid.columns
                            Label {
                                text: qsTranslate("WelcomeWidget", "Document")
                                color: Style.hintTextColor
                                leftPadding: 8
                                Layout.fillWidth: true
                            }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_open") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_new") }

                            Label {
                                Layout.fillWidth: true
                                text: qsTranslate("WelcomeWidget", "Import items")
                                color: Style.hintTextColor
                                leftPadding: 8
                            }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_bl_inv") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_bl_store_inv") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_bl_order") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_bl_cart") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_bl_wanted") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_ldraw_model") }
                            ActionDelegate { action: BS.ActionManager.quickAction("document_import_bl_xml") }

                            Label {
                                text: qsTranslate("WelcomeWidget", "Currently Open Documents")
                                Layout.fillWidth: true
                                color: Style.hintTextColor
                                visible: BS.BrickStore.documents.count !== 0
                                leftPadding: 8
                            }

                            Repeater {
                                model: BS.BrickStore.documents

                                ActionDelegate {
                                    required property string fileNameOrTitle
                                    required property var document
                                    // preferably document should be of type Document, but that leads
                                    // to a weird warning whenever document is removed from the model

                                    icon.source: "qrc:/assets/generated-app-icons/brickstore_doc"
                                    text: fileNameOrTitle
                                    onClicked: root.setActiveDocument(document)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                            Layout.fillWidth: true
                            Layout.preferredWidth: implicitWidth
                            Label {
                                text: qsTranslate("WelcomeWidget", "Open recent files")
                                color: Style.hintTextColor
                                leftPadding: 8
                            }

                            Repeater {
                                model: BS.BrickStore.recentFiles

                                ActionDelegate {
                                    required property int index
                                    required property string fileName
                                    required property string filePath
                                    icon.source: "qrc:/assets/generated-app-icons/brickstore_doc"
                                    text: fileName
                                    infoText: filePath
                                    onClicked: BS.BrickStore.recentFiles.open(index)
                                }
                            }
                            ActionDelegate {
                                text: qsTranslate("WelcomeWidget", "No recent files")
                                visible: BS.BrickStore.recentFiles.count === 0
                                enabled: false
                            }
                        }
                    }
                }
            }
        }
        Item { id: viewProxy }
    }

    function indexOfDocument(doc : BS.Document) : int {
        for (let i = 0; i < views.length; ++i) {
            if (views[i].document === doc)
                return i
        }
        return -1
    }

    property var views: []
    property View currentView: null

    function setActiveDocument(doc : BS.Document) {
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
                view.forceActiveFocus()
            }
        }
        BS.ActionManager.activeDocument = view ? doc.document : null
        homeAndView.setCurrentIndex(view ? 1: 0)
    }

    function createViewForDocument(doc : BS.Document) {
        console.log("Document added:", doc.title, doc.fileName)
        doc.requestActivation.connect(() => { root.setActiveDocument(doc) })
        doc.closeAllViewsForDocument.connect(
                    () => {
                        let index = indexOfDocument(doc)
                        if (index >= 0) {
                            if (views[index] === currentView) {
                                setActiveDocument(null)
                            }
                            views.splice(index, 1).forEach(v => v.destroy())
                        }
                    })

        let viewComponent = Qt.createComponent("View.qml")
        if (viewComponent.status === Component.Ready) {
            let view = viewComponent.createObject(viewProxy, {
                                                      visible: false,
                                                      document: doc,
                                                      goHomeFunction: function() { setActiveDocument(null) },
                                                      })
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

    Connections {
        target: BS.BrickStore.documents
        function onDocumentAdded(doc : BS.Document) { createViewForDocument(doc) }
    }

    DialogLoader {
        id: announcementsDialog
        source: "AnnouncementsDialog.qml"
    }

    Connections {
        target: BS.BrickStore
        function onShowSettings(page : string) {
            setActiveDocument(null)
            homeStack.push("SettingsDialog.qml", { "page": page, "goBackFunction": () => { homeStack.pop() } })
        }
    }
    Connections {
        target: BS.Announcements
        function onNewAnnouncements() {
            announcementsDialog.open()
        }
    }

    Component.onDestruction:  {
        views.forEach(v => v.destroy())
        views = []
    }

    property var connectionContext

    Component.onCompleted: {
        // Handle Android's back button
        ApplicationWindow.contentItem.Keys.released.connect(function(e) {
            if (e.key === Qt.Key_Back) {
                e.accepted = true
                if (currentView)
                    setActiveDocument(null)
                else if (homeStack.depth > 1)
                    homeStack.pop()
                else
                    close()
            }
        })

        connectionContext = BS.ActionManager.connectQuickActionTable
                ({
                     "document_import_bl_inv": () => {
                         setActiveDocument(null)
                         homeStack.push("ImportInventoryDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                     },
                     "document_import_bl_order": () => {
                         BS.BrickStore.checkBrickLinkLogin().then(function(ok) {
                             if (ok) {
                                 setActiveDocument(null)
                                 homeStack.push("ImportOrderDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                             }
                         })
                     },
                     "document_import_bl_cart": () => {
                         BS.BrickStore.checkBrickLinkLogin().then(function(ok) {
                             if (ok) {
                                 setActiveDocument(null)
                                 homeStack.push("ImportCartDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                             }
                         })
                     },
                     "document_import_bl_wanted": () => {
                         BS.BrickStore.checkBrickLinkLogin().then(function(ok) {
                             if (ok) {
                                 setActiveDocument(null)
                                 homeStack.push("ImportWantedListDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                             }
                         })
                     },
                     "update_database": () => { BS.BrickStore.updateDatabase() },
                     "help_about": () => { aboutDialog.active = true },
                     "help_systeminfo": () => {
                         homeStack.push("SystemInfoDialog.qml", { "goBackFunction": () => { homeStack.pop() } })
                     },
                     "help_announcements": () => { },
                 })
        for (let i = 0; i < BS.BrickStore.documents.count; ++i) {
            createViewForDocument(BS.BrickStore.documents.document(i));
        }
    }
}
