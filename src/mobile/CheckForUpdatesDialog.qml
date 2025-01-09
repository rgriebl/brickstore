// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


AutoSizingDialog {
    id: root
    title: qsTr("Update")
    relativeWidth: 7 / 8
    relativeHeight: 7 / 8

    required property string changeLog
    required property string releaseUrl

    footer: DialogButtonBox {
        Button {
            text: qsTr("Show")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        Qt.openUrlExternally(root.releaseUrl)
    }

    onOpened: { sl.flashScrollIndicators() }

    ScrollableLayout {
        id: sl
        anchors.fill: parent

        ColumnLayout {
            width: sl.width

            Label {
                text: root.changeLog
                Layout.fillWidth: true
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                padding: 8
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
