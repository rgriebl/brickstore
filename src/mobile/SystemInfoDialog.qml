// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore as BS


FullscreenDialog {
    id: root
    title: qsTr("System Information")

    footer: TabBar {
        id: tabBar
        TabButton { text: root.title }
        TabButton { text: qsTr("Diagnostics") }
    }

    toolButtons: ToolButton {
        icon.name: "edit-copy"
        visible: (tabBar.currentIndex === 0)
        onClicked: { BS.Clipboard.setText(root.sysinfoMarkdown) }
        onPressAndHold: { BS.BrickStore.crash(false) }
        onDoubleClicked: { BS.BrickStore.crash(true) }
    }

    property var goBackFunction
    onBackClicked: { root.goBackFunction() }

    property string sysinfoMarkdown
    property string diagMarkdown

    Component.onCompleted: {
        let sysinfo = BS.SystemInfo.asMap()
        let md = "### BrickStore " + BS.BrickStore.versionNumber + " (build: " + BS.BrickStore.buildNumber + ")\n\n"

        delete sysinfo["os.type"]
        delete sysinfo["os.version"]
        delete sysinfo["hw.gpu.arch"]
        delete sysinfo["hw.memory"]
        delete sysinfo["brickstore.version"]

        Object.keys(sysinfo).forEach(function(k) {
            md = md + " * **" + k + "**: " + sysinfo[k] + "\n"
        })

        root.sysinfoMarkdown = md
        diagMarkdown = "```" + BS.SystemInfo.qtDiag() + "\n```\n"

        sl.flashScrollIndicators()
    }

    SwipeView {
        anchors.fill: parent
        anchors.margins: 8
        interactive: false

        clip: true
        currentIndex: tabBar.currentIndex

        ScrollableLayout {
            id: sl

            ColumnLayout {
                width: sl.width

                Label {
                    text: root.sysinfoMarkdown
                    Layout.fillWidth: true
                    textFormat: Text.MarkdownText
                    wrapMode: Text.WordWrap
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }
        ScrollableLayout {
            id: diag

            ColumnLayout {
                width: diag.width

                Label {
                    text: root.diagMarkdown
                    Layout.fillWidth: true
                    textFormat: Text.MarkdownText
                    wrapMode: Text.WordWrap
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }
    }
}
