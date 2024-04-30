// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(Math.max(fm.averageCharacterWidth * 50, implicitWidth), Overlay.overlay?.width ?? 800)

    property alias text: label.text
    property int buttonResult: DialogButtonBox.NoButton

    onStandardButtonsChanged: updateClosePolicy()

    function updateClosePolicy() {
        let allButtons = []
        let rejectRoleButtons = []

        for (let i = DialogButtonBox.FirstButton; i <= DialogButtonBox.LastButton; i <<= 1) {
            let button = buttons.standardButton(i)
            if (button)
                allButtons.push(i)
            if (button?.DialogButtonBox.buttonRole === DialogButtonBox.RejectRole)
                rejectRoleButtons.push(i)
        }
        if (allButtons.length === 1)
            buttonResult = allButtons[0]
        if (rejectRoleButtons.length === 1)
            buttonResult = rejectRoleButtons[0]

        root.closePolicy = (allButtons.length < 2)
                ? Popup.CloseOnPressOutside | Popup.CloseOnEscape
                : (rejectRoleButtons.length === 1 ? Popup.CloseOnEscape
                                                  : Popup.NoAutoClose)
    }

    footer: DialogButtonBox {
        id: buttons
        visible: count > 0
        standardButtons: root.standardButtons

        onClicked: function(button) {
            for (let i = DialogButtonBox.FirstButton; i <= DialogButtonBox.LastButton; i <<= 1) {
                if (buttons.standardButton(i) === button) {
                    root.buttonResult = i
                    Qt.callLater(root.close)
                    break
                }
            }
        }
    }

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
    }
}
