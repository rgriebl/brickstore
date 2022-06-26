import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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



            component ColorLabel : Label {
                property color c
                property string t
                text: t
                leftPadding: height * 1.5
                Rectangle { width: parent.height; height: parent.height; color: parent.c }
            }

            ColorLabel { t: "foreground color                 "; c: Material.foreground }
            ColorLabel { t: "background color                 "; c: Material.background }
            ColorLabel { t: "backgroundColor color            "; c: Material.backgroundColor }
            ColorLabel { t: "accent color                     "; c: Material.accent }
            ColorLabel { t: "accentColor color                "; c: Material.accentColor }
            ColorLabel { t: "primary color                    "; c: Material.primary }
            ColorLabel { t: "primaryColor color               "; c: Material.primaryColor }
            ColorLabel { t: "primaryTextColor color           "; c: Material.primaryTextColor }
            ColorLabel { t: "primaryHighlightedTextColor color"; c: Material.primaryHighlightedTextColor }
            ColorLabel { t: "backgroundDimColor color         "; c: Material.backgroundDimColor }
            ColorLabel { t: "buttonColor color                "; c: Material.buttonColor }
            ColorLabel { t: "buttonDisabledColor color        "; c: Material.buttonDisabledColor }
            ColorLabel { t: "dialogColor color                "; c: Material.dialogColor }
            ColorLabel { t: "dividerColor color               "; c: Material.dividerColor }
            ColorLabel { t: "dropShadowColor color            "; c: Material.dropShadowColor }
            ColorLabel { t: "listHighlightColor color         "; c: Material.listHighlightColor }
            ColorLabel { t: "textSelectionColor color         "; c: Material.textSelectionColor }
            ColorLabel { t: "secondaryTextColor color         "; c: Material.secondaryTextColor }
            ColorLabel { t: "toolTextColor color              "; c: Material.toolTextColor }
            ColorLabel { t: "toolBarColor color               "; c: Material.toolBarColor }
            ColorLabel { t: "iconColor color                  "; c: Material.iconColor }
            ColorLabel { t: "hintTextColor color              "; c: Material.hintTextColor }


        }
    }
}
