import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS


BrickStoreDialog {
    id: root
    title: qsTr("Settings")
    relativeWidth: 2 / 3
    relativeHeight: 2 / 3
    property string page    

    footer: TabBar {
        id: tabBar

        position: TabBar.Footer
        currentIndex: swipeView.currentIndex

        TabButton { text: qsTr("General");   property string pageName: "general" }
        TabButton { text: qsTr("UI");        property string pageName: "ui" }
        TabButton { text: qsTr("BrickLink"); property string pageName: "bricklink" }
    }

    function openPage(page) {
        for (let i = 0; i < tabBar.contentItem.children.length; ++i) {
            if (tabBar.contentItem.children[i].pageName === page) {
                tabBar.setCurrentIndex(i)
                break
            }
        }
        open()
    }

    spacing: 16

    SwipeView {
        id: swipeView
        anchors.fill: parent
        currentIndex: tabBar.currentIndex
        clip: true

        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                RowLayout {
                   spacing: root.spacing
                   Layout.leftMargin: root.spacing
                   Layout.rightMargin: root.spacing
                    Label {
                        text: qsTr("Language")
                        font.pixelSize: langCombo.font.pixelSize
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: langCombo
                        model: BS.Config.availableLanguages
                        textRole: "language"
                        valueRole: "language"
                        enabled: BS.Config.availableLanguages.length > 0
                        delegate: ItemDelegate {
                            width: parent.width
                            text: langCombo.nameForLang(modelData)
                        }
                        displayText: nameForLang(BS.Config.availableLanguages[currentIndex])

                        function nameForLang(map) {
                            let name = map.name
                            if (map.localName)
                                name = map.localName + " (" + name + ")"
                            return name
                        }

                        onActivated: { BS.Config.language = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BS.Config.language) }
                    }
                }

                SwitchDelegate {
                    text: qsTr("Open Browser on Export")
                    checked: BS.Config.openBrowserOnExport
                    Layout.fillWidth: true
                    onToggled: BS.Config.openBrowserOnExport = checked
                }
                SwitchDelegate {
                    text: qsTr("Show input errors")
                    checked: BS.Config.showInputErrors
                    Layout.fillWidth: true
                    onToggled: BS.Config.showInputErrors = checked
                }
            }
        }
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                GridLayout {
                    columns: 2
                    rowSpacing: root.spacing / 2
                    columnSpacing: root.spacing
                    Layout.leftMargin: root.spacing
                    Layout.rightMargin: root.spacing
                    Label {
                        text: qsTr("Theme")
                        font.pixelSize: langCombo.font.pixelSize
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: themeCombo
                        model: [
                            { value: 0, text: qsTr("System default") },
                            { value: 1, text: qsTr("Light") },
                            { value: 2, text: qsTr("Dark") },
                        ]
                        textRole: "text"
                        valueRole: "value"

                        onActivated: { BS.Config.uiTheme = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BS.Config.uiTheme) }
                    }
                    Label {
                        text: qsTr("UI Layout")
                        font.pixelSize: langCombo.font.pixelSize
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: uisizeCombo
                        model: [
                            { value: 0, text: qsTr("Automatic") },
                            { value: 1, text: qsTr("Small") },
                            { value: 2, text: qsTr("Large") },
                        ]
                        textRole: "text"
                        valueRole: "value"

                        onActivated: { BS.Config.mobileUISize = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BS.Config.mobileUISize) }
                    }
                }
            }
        }
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                GridLayout {
                    columns: 2
                    ItemDelegate {
                        text: qsTr("Username")
                        font.pixelSize: blUsername.font.pixelSize
                        onClicked: { blUsername.focus = true }
                    }
                    TextField {
                        id: blUsername
                        Layout.fillWidth: true
                        text: BS.Config.brickLinkUsername
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                        onEditingFinished: BS.Config.brickLinkUsername = text
                    }
                    ItemDelegate {
                        text: qsTr("Password")
                        font.pixelSize: blPassword.font.pixelSize
                        onClicked: { blPassword.focus = true }
                    }
                    TextField {
                        id: blPassword
                        Layout.fillWidth: true
                        text: BS.Config.brickLinkPassword
                        echoMode: TextInput.PasswordEchoOnEdit
                        onEditingFinished: BS.Config.brickLinkPassword = text
                    }
                }
            }
        }
    }
}
