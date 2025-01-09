// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore as BS


AutoSizingDialog {
    id: root
    title: qsTr("Announcements")
    relativeWidth: 7 / 8
    relativeHeight: 7 / 8


    footer: DialogButtonBox {
        Button {
            text: qsTr("Mark read")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    property string announcementMarkdown
    property var shownIds: []

    onAboutToShow: {
        let al = BS.Announcements.unreadAnnouncements()
        let md = ""
        shownIds = []
        al.forEach(function(a) {
            shownIds.push(a["id"])

            if (md === '')
                md = md + "\n\n___\n\n"
            md = md + "**" + a["title"] + "** &mdash; *"
                    + a["date"].toLocaleDateString(root.locale, Locale.ShortFormat)
                    + "*\n\n" + a["text"]
        })

        root.announcementMarkdown = (shownIds.length === 0) ? '' : md
    }

    onAccepted: {
        shownIds.forEach(function(id) {
            BS.Announcements.markAnnouncementRead(id)
        })
    }

    onOpened: { sl.flashScrollIndicators() }

    ScrollableLayout {
        id: sl
        anchors.fill: parent

        ColumnLayout {
            width: sl.width

            Label {
                text: root.announcementMarkdown
                Layout.fillWidth: true
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                padding: 8
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
