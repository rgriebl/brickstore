import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore

BrickStoreDialog {
    id: root
    title: qsTr("About")

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
                    text: BrickStore.about.header
                    horizontalAlignment: Text.AlignLeft
                    Layout.fillWidth: true
                    textFormat: Text.RichText
                    wrapMode: Text.Wrap
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
            Label {
                text: BrickStore.about.license
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
            Label {
                text: BrickStore.about.translators
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
