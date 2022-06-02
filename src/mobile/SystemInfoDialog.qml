import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore as BS


BrickStoreDialog {
    id: root
    title: qsTr("System Information")

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

    ScrollView {
        id: sv
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: sv.contentWidth

            Label {
                text: root.sysinfoMarkdown
                Layout.fillWidth: true
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
