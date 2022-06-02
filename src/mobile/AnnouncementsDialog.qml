import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS

BrickStoreDialog {
    id: root
    title: qsTr("Announcements")
    relativeWidth: 7 / 8
    relativeHeight: 7 / 8


    footer: DialogButtonBox {
        Button {
            text: qsTr("Mark read")
            flat: true //TODO: Material
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


    ScrollView {
        id: sv
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: sv.contentWidth

            Label {
                text: root.announcementMarkdown
                Layout.fillWidth: true
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
