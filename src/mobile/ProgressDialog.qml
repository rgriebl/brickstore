// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


Dialog {
    id: root

    property alias text: progressLabel.text
    property alias done: progress.value
    property alias total: progress.to
    property alias cancelable: buttons.enabled

    signal requestCancel()

    modal: true
    standardButtons: DialogButtonBox.Cancel
    closePolicy: Popup.NoAutoClose // we can't just let the user close this dialog
    anchors.centerIn: Overlay.overlay

    ColumnLayout {
        Label {
            id: progressLabel
            wrapMode: Text.Wrap
        }
        ProgressBar {
            id: progress
            indeterminate: !to && !value
        }
    }
    footer: RowLayout {
        // we need to hide the box inside a layout to prevent the automatically
        // created connections to Dialog.accept and .reject
        DialogButtonBox {
            id: buttons
            standardButtons: root.standardButtons
            Layout.fillWidth: true

            onAccepted: root.accept()
            onRejected: root.requestCancel()
        }
    }
}
