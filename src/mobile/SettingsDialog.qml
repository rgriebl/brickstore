import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BrickStore

Dialog {
    id: root
    title: qsTr("Settings")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Overlay.overlay.width * 2 / 3
    height: Overlay.overlay.height * 2 / 3
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
                    spacing: 16
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Label {
                        text: qsTr("Language")
                        font.pixelSize: langCombo.font.pixelSize
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: langCombo
                        model: BrickStore.config.availableLanguages
                        textRole: "language"
                        valueRole: "language"
                        enabled: BrickStore.config.availableLanguages.length > 0
                        delegate: ItemDelegate {
                            text: langCombo.nameForLang(modelData)
                        }
                        displayText: nameForLang(BrickStore.config.availableLanguages[currentIndex])

                        function nameForLang(map) {
                            let name = map.name
                            if (map.localName)
                                name = map.localName + " (" + name + ")"
                            return name
                        }

                        onActivated: { BrickStore.config.language = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BrickStore.config.language) }
                    }
                }

                SwitchDelegate {
                    text: qsTr("Open Browser on Export")
                    checked: BrickStore.config.openBrowserOnExport
                    Layout.fillWidth: true
                    onToggled: BrickStore.config.openBrowserOnExport = checked
                }
                SwitchDelegate {
                    text: qsTr("Show input errors")
                    checked: BrickStore.config.showInputErrors
                    Layout.fillWidth: true
                    onToggled: BrickStore.config.showInputErrors = checked
                }
            }
        }
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
                ButtonGroup {
                    id: styleGroup
                    onClicked: (button) => BrickStore.config.uiTheme = button.theme
                }

                RadioDelegate {
                    text: qsTr("Follow the system's theme")
                    checked: BrickStore.config.uiTheme === theme
                    ButtonGroup.group: styleGroup
                    Layout.fillWidth: true
                    property int theme: Config.SystemDefault
                }
                RadioDelegate {
                    text: qsTr("Use a light theme")
                    checked: BrickStore.config.uiTheme === theme
                    ButtonGroup.group: styleGroup
                    Layout.fillWidth: true
                    property int theme: Config.Light
                }
                RadioDelegate {
                    text: qsTr("Use a dark theme")
                    checked: BrickStore.config.uiTheme === theme
                    ButtonGroup.group: styleGroup
                    Layout.fillWidth: true
                    property int theme: Config.Dark
                }
            }
        }
        ScrollView {
            contentWidth: availableWidth
            ColumnLayout {
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
                        text: BrickStore.config.brickLinkUsername
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                        onEditingFinished: BrickStore.config.brickLinkUsername = text
                    }
                    ItemDelegate {
                        text: qsTr("Password")
                        font.pixelSize: blPassword.font.pixelSize
                        onClicked: { blPassword.focus = true }
                    }
                    TextField {
                        id: blPassword
                        Layout.fillWidth: true
                        text: BrickStore.config.brickLinkPassword
                        echoMode: TextInput.PasswordEchoOnEdit
                        onEditingFinished: BrickStore.config.brickLinkPassword = text
                    }
                }
            }
        }
    }
}
