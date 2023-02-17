// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick.Controls // needed because of Qt bug (Overlay is not defined)
import Mobile
import BrickStore as BS


Dialog {
    id: root
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Math.max(fm.averageCharacterWidth * 70, implicitWidth), Overlay.overlay ? Overlay.overlay.width : 800)
    standardButtons: DialogButtonBox.Cancel | DialogButtonBox.Ok

    required property string text
    required property bool showTiers
    required property string currencyCode

    property alias fixed: fixedButton.checked
    property alias applyToTiers: tiers.checked

    property double maxPrice: BS.BrickStore.maxLocalPrice(root.currencyCode)

    property double value: 0
    property double minimum: fixed ? -maxPrice : -99
    property double maximum: fixed ? maxPrice : 1000
    property int decimals: fixed ? 3 : 2

    onValueChanged: dblSpin.value = value * dblSpin.factor

    onFixedChanged: dblSpin.value = 0

    FontMetrics {
        id: fm
        font: root.font
    }

    ScrollableLayout {
        id: scrollableLayout
        anchors.fill: parent

        ColumnLayout {
            id: layout
            width: scrollableLayout.width

            Label {
                text: root.text
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                SpinBox {
                    id: dblSpin
                    editable: true

                    property int factor: Math.pow(10, root.decimals)
                    property double readDoubleValue: value / factor

                    onReadDoubleValueChanged: root.value = readDoubleValue

                    from: root.minimum * factor
                    to: root.maximum * factor

                    validator: DoubleValidator {
                        id: dblValidator
                        bottom: Math.min(dblSpin.from, dblSpin.to)
                        top: Math.max(dblSpin.from, dblSpin.to)
                    }
                    textFromValue: function(value, locale) {
                        return Number(value / factor).toLocaleString(locale, 'f', root.decimals)
                    }

                    valueFromText: function(text, locale) {
                        return Number.fromLocaleString(locale, text) * factor
                    }
                    Layout.fillWidth: true
                }
                CheckButton {
                    text: "%"
                    checked: true
                }
                CheckButton {
                    id: fixedButton
                    text: root.currencyCode
                }
            }
            Label {
                text: "(" + (root.fixed ? qsTr("Fixed %1 amount").arg(root.currencyCode) : qsTr("Percent")) + ")"
                wrapMode: Text.Wrap
                color: Style.hintTextColor
                horizontalAlignment: Text.AlignRight
                Layout.fillWidth: true
            }
            Switch {
                id: tiers
                visible: root.showTiers
                checked: false
                text: qsTr("Also apply this change to &tier prices").replace("&", "")
            }
        }
    }
}
