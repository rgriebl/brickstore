import Mobile
import BrickStore as BS


AutoSizingDialog {
    id: root
    title: qsTr("System Information")
    keepPaddingInSmallMode: true

    footer: DialogButtonBox {
        Button {
            text: qsTr("Copy to clipboard")
            flat: true //TODO: Material
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    property string sysinfoMarkdown

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
    }

    onAccepted: {
        BS.Clipboard.setText(root.sysinfoMarkdown);
    }

    onOpened: { sl.flashScrollIndicators() }

    ScrollableLayout {
        id: sl
        anchors.fill: parent

        ColumnLayout {
            width: sl.width

            Label {
                text: root.sysinfoMarkdown
                Layout.fillWidth: true
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }

            Item { Layout.preferredHeight: 32 }

            component ColorLabel : Label {
                property color c
                property string t
                text: t
                leftPadding: height * 1.5
                Rectangle { width: parent.height; height: parent.height; color: parent.c }
            }

            ColorLabel { t: "textColor                  "; c: Style.textColor }
            ColorLabel { t: "backgroundColor            "; c: Style.backgroundColor }
            ColorLabel { t: "accentColor                "; c: Style.accentColor }
            ColorLabel { t: "accentTextColor            "; c: Style.accentTextColor }
            ColorLabel { t: "primaryColor               "; c: Style.primaryColor }
            ColorLabel { t: "primaryTextColor           "; c: Style.primaryTextColor }
            ColorLabel { t: "primaryHighlightedTextColor"; c: Style.primaryHighlightedTextColor }
            ColorLabel { t: "hintTextColor              "; c: Style.hintTextColor }
        }
    }
}
