// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickStore as BS
import BrickLink as BL


AutoSizingDialog {
    id: root
    title: qsTr("Settings")
    relativeWidth: 3 / 4
    relativeHeight: 3 / 4
    property string page    

    footer: TabBar {
        id: tabBar

        position: TabBar.Footer
        currentIndex: swipeView.currentIndex

        TabButton { text: qsTr("General");   property string pageName: "general" }
        TabButton { text: qsTr("UI");        property string pageName: "ui" }
        TabButton { text: qsTr("BrickLink"); property string pageName: "bricklink" }
    }

    Component.onCompleted: {
        for (let i = 0; i < tabBar.count; ++i) {
            if (tabBar.contentChildren[i].pageName === page) {
                tabBar.setCurrentIndex(i)
                break
            }
        }
    }

    SwipeView {
        id: swipeView
        anchors.fill: parent

        currentIndex: tabBar.currentIndex
        interactive: false
        clip: true

        ScrollableLayout {
            id: langScroller

            SwipeView.onIsCurrentItemChanged: { if (SwipeView.isCurrentItem) flashScrollIndicators() }

            ColumnLayout {
                width: langScroller.width
                GridLayout {
                    columns: 2
                    rowSpacing: root.spacing / 2
                    columnSpacing: root.spacing
                    Layout.leftMargin: root.spacing
                    Layout.rightMargin: root.spacing

                    Label {
                        text: qsTr("Language")
                        font.pixelSize: langCombo.font.pixelSize
                    }
                    IconComboBox {
                        Layout.fillWidth: true
                        id: langCombo
                        model: BS.Config.availableLanguages
                        textRole: "localName"
                        valueRole: "language"
                        iconSourceRole: "flagPath"
                        enabled: BS.Config.availableLanguages.length > 0
                        onActivated: { BS.Config.language = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BS.Config.language) }
                    }

                    Label {
                        text: qsTr("Weights")
                        font.pixelSize: weightsCombo.font.pixelSize
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: weightsCombo
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { value: Locale.MetricSystem,   text: qsTr("Metric (g)") },
                            { value: Locale.ImperialSystem, text: qsTr("Imperial (oz)") },
                        ]

                        onActivated: { BS.Config.measurementSystem = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BS.Config.measurementSystem) }
                    }

                    Label {
                        text: qsTr("Default currency")
                        font.pixelSize: currencyCombo.font.pixelSize
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        id: currencyCombo
                        model: BS.Currency.currencyCodes

                        onActivated: { BS.Config.defaultCurrencyCode = currentValue }
                        Component.onCompleted: { currentIndex = indexOfValue(BS.Config.defaultCurrencyCode) }
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
        ScrollableLayout {
            id: uiScroller

            SwipeView.onIsCurrentItemChanged: { if (SwipeView.isCurrentItem) flashScrollIndicators() }

            ColumnLayout {
                width: uiScroller.width
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
                    Label {
                        text: qsTr("Item image size")
                        font.pixelSize: langCombo.font.pixelSize
                    }
                    SpinBox {
                        Layout.fillWidth: true
                        from: 50
                        to: 200
                        stepSize: 25

                        onValueModified: { BS.Config.rowHeightPercent = value }
                        Component.onCompleted: { value = 25 * Math.round(BS.Config.rowHeightPercent / 25) }
                    }
                }
            }
        }
        ScrollableLayout {
            id: blScroller

            SwipeView.onIsCurrentItemChanged: { if (SwipeView.isCurrentItem) flashScrollIndicators() }

            ColumnLayout {
                width: blScroller.width
                GridLayout {
                    columns: 2
                    rowSpacing: root.spacing / 2
                    columnSpacing: root.spacing
                    Layout.leftMargin: root.spacing
                    Layout.rightMargin: root.spacing

                    Label {
                        text: qsTr("Username")
                        font.pixelSize: blUsername.font.pixelSize
                    }
                    TextField {
                        id: blUsername
                        Layout.fillWidth: true
                        text: BS.Config.brickLinkUsername
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                        onEditingFinished: BS.Config.brickLinkUsername = text
                    }
                    Label {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        text: qsTr("Your username is required here - not your email address.")
                        wrapMode: Text.Wrap
                        leftPadding: 16
                        rightPadding: 16
                        font.bold: true
                        color: Qt.rgba(1, .4, .4)
                        visible: blUsername.text.indexOf('@') >= 0
                    }
                    Label {
                        text: qsTr("Password")
                        font.pixelSize: blPassword.font.pixelSize
                    }
                    TextField {
                        id: blPassword
                        Layout.fillWidth: true
                        text: BS.Config.brickLinkPassword
                        echoMode: TextInput.PasswordEchoOnEdit
                        onEditingFinished: BS.Config.brickLinkPassword = text
                    }
                    Label {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        text: qsTr("BrickLink's maximum password length is 15.")
                        wrapMode: Text.Wrap
                        leftPadding: 16
                        rightPadding: 16
                        font.bold: true
                        color: Qt.rgba(1, .4, .4)
                        visible: blPassword.text.length > 15
                    }

                    Label {
                        text: qsTr("Price-guide")
                        font.pixelSize: vatTypeCombo.font.pixelSize
                    }
                    IconComboBox {
                        Layout.fillWidth: true
                        id: vatTypeCombo
                        textRole: "text"
                        valueRole: "value"
                        iconNameRole: "icon"

                        model: ListModel { id: vatTypeModel }
                        Component.onCompleted: {
                            let svt = BL.BrickLink.supportedVatTypes
                            for (let i = 0; i < svt.length; ++i) {
                                vatTypeModel.append({
                                                        "text": BL.BrickLink.descriptionForVatType(svt[i]),
                                                        "icon": BL.BrickLink.iconForVatType(svt[i]),
                                                        "value": svt[i]
                                                    })
                            }
                            currentIndex = indexOfValue(BL.BrickLink.currentVatType)
                        }
                        onActivated: { BL.BrickLink.currentVatType = currentValue }
                    }
                }
            }
        }
    }
}
