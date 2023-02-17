// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore as BS


AutoSizingDialog {
    id: root
    title: qsTr("System Information")
    keepPaddingInSmallMode: true

    footer: TabBar {
        id: tabBar
        TabButton { text: root.title }
        TabButton { text: qsTr("Colors") }
        TabButton { text: qsTr("Diagnostics") }
    }

    property string sysinfoMarkdown
    property string diagMarkdown

    onAboutToShow: {
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
    }

    onOpened: { sl.flashScrollIndicators() }

    SwipeView {
        anchors.fill: parent
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
            id: colors

            ColumnLayout {
                width: colors.width

                component ColorLabel : Label {
                    id: label
                    property color c
                    property string t
                    text: t + " [" + c.toString().toUpperCase() + "]"
                    leftPadding: height * 1.5
                    Rectangle { width: parent.height; height: parent.height; color: label.c }
                }

                ColorLabel { t: "textColor"; c: Style.textColor }
                ColorLabel { t: "backgroundColor"; c: Style.backgroundColor }
                ColorLabel { t: "accentColor"; c: Style.accentColor }
                ColorLabel { t: "accentTextColor"; c: Style.accentTextColor }
                ColorLabel { t: "primaryColor"; c: Style.primaryColor }
                ColorLabel { t: "primaryTextColor"; c: Style.primaryTextColor }
                ColorLabel { t: "primaryHighlightedTextColor"; c: Style.primaryHighlightedTextColor }
                ColorLabel { t: "hintTextColor"; c: Style.hintTextColor }
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
