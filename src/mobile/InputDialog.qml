// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: DialogButtonBox.Cancel | DialogButtonBox.Ok
    width: Math.min(Math.max(fm.averageCharacterWidth * 50, implicitWidth), Overlay.overlay ? Overlay.overlay.width : 800)

    property string mode: "string"
    property alias text: label.text
    property string unit
    property bool isPassword: false

    property alias textValue: text.text

    property alias intValue: intSpin.value
    property alias intMinimum: intSpin.from
    property alias intMaximum: intSpin.to

    property double doubleValue
    property double doubleMinimum
    property double doubleMaximum
    property int doubleDecimals

    Component.onCompleted: dblSpin.value = root.doubleValue * dblSpin.factor

    FontMetrics {
        id: fm
        font: root.font
    }

    ColumnLayout {
        anchors.fill: parent

        Label {
            id: label
            horizontalAlignment: Text.AlignLeft
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        TextField {
            id: text
            visible: root.mode === "string"
            echoMode: root.isPassword ? TextInput.Password : TextInput.Normal
            inputMethodHints: root.isPassword ? Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase : 0
            Layout.fillWidth: true
        }
        RowLayout {
            SpinBox {
                id: intSpin
                visible: root.mode === "int"
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                from: intValidator.bottom
                to: intValidator.top
                validator: IntValidator { id: intValidator }
                Layout.fillWidth: true
            }
            Label {
                text: root.unit
                visible: intSpin.visible && root.unit !== ''
            }
        }
        RowLayout {
            SpinBox {
                id: dblSpin
                visible: root.mode === "double"
                editable: true
                inputMethodHints: Qt.ImhFormattedNumbersOnly

                property int factor: Math.pow(10, root.doubleDecimals)
                property double readDoubleValue: value / factor

                onReadDoubleValueChanged: root.doubleValue = readDoubleValue

                from: Math.round(root.doubleMinimum * factor)
                to: Math.round(root.doubleMaximum * factor)
                stepSize: factor

                validator: DoubleValidator {
                    id: dblValidator
                    bottom: Math.min(dblSpin.from, dblSpin.to)
                    top: Math.max(dblSpin.from, dblSpin.to)
                    decimals: root.doubleDecimals
                    notation: DoubleValidator.StandardNotation
                }
                textFromValue: function(value, locale) {
                    return Number(value / factor).toLocaleString(locale, 'f', root.doubleDecimals)
                }

                valueFromText: function(text, locale) {
                    return Math.round(Number.fromLocaleString(locale, text) * factor)
                }
                Layout.fillWidth: true
            }
            Label {
                text: root.unit
                visible: dblSpin.visible && root.unit !== ''
            }
        }
    }
}
