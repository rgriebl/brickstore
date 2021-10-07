import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore

Dialog {
    id: root
    title: qsTr("About")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Overlay.overlay.width * 3 / 4
    height: Overlay.overlay.height * 3 / 4

    FontMetrics { id: fm }

    ScrollView {
        id: sv
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: sv.contentWidth

            RowLayout {
                spacing: fm.averageCharacterWidth * 2
                Image {
                    source: "qrc:/assets/generated-app-icons/brickstore"
                    sourceSize.height: fm.height * 7
                    //sourceSize.width: sourceSize.height
                    horizontalAlignment: Image.AlignRight
                }
                Label {
                    text: BrickStore.about.header
                    horizontalAlignment: Text.AlignLeft
                    Layout.fillWidth: true
                    textFormat: Text.RichText
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
            Label {
                text: BrickStore.about.license
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
            Label {
                text: BrickStore.about.translators
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
