// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore as BS


AutoSizingDialog {
    id: root
    title: qsTr("About")
    keepPaddingInSmallMode: true

    FontMetrics { id: fm }

    onOpened: { sl.flashScrollIndicators() }

    ScrollableLayout {
        id: sl
        anchors.fill: parent

        ColumnLayout {
            width: sl.width

            RowLayout {
                spacing: fm.averageCharacterWidth * 2
                Image {
                    source: "qrc:/assets/generated-app-icons/brickstore"
                    sourceSize.height: fm.height * 5
                    //sourceSize.width: sourceSize.height
                    horizontalAlignment: Image.AlignRight
                }
                Label {
                    text: BS.BrickStore.about.header
                    horizontalAlignment: Text.AlignLeft
                    Layout.fillWidth: true
                    textFormat: Text.RichText
                    wrapMode: Text.Wrap
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
            Label {
                text: BS.BrickStore.about.license
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
            Label {
                text: BS.BrickStore.about.translators
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
