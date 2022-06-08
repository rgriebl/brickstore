import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import BrickStore as BS
import "qrc:/ldraw" as LDraw

Control {
    id: root
    property BS.Document document
    property var lot

    property bool single: false
    property bool is3D: false
    property BS.Picture picture
    property bool isUpdating: (picture && (picture.updateStatus === BS.BrickLink.UpdateStatus.Updating))

    onLotChanged: { updateInfo() }

    function updateInfo() {
        single = (lot !== undefined)

        if (picture)
            picture.release()
        picture = null

        if (single) {
            infoText.text = BS.BrickLink.itemHtmlDescription(lot.item, lot.color, infoText.palette.highlight)

            picture = BS.BrickLink.picture(lot.item, lot.color, true)
            if (picture)
                picture.addRef()

            Qt.callLater(function() {
                let has3D = info3D.renderController.setPartAndColor(lot.item, lot.color)
                root.is3D = has3D
            })
        } else {
            info3D.renderController.setPartAndColor(BS.BrickLink.noItem, BS.BrickLink.noColor)

            let stat = document.selectionStatistics()
            infoText.text = stat.asHtmlTable()
        }
    }

    Component.onDestruction: {
        if (picture)
            picture.release()
    }

    ColumnLayout {
        anchors.fill: parent
        Label {
            Layout.fillWidth: true

            id: infoText
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        StackLayout {
            clip: true

            currentIndex: root.is3D ? 1 : 0

            BS.QImageItem {
                id: infoImage
                fillColor: "white"
                image: root.picture && root.picture.isValid ? root.picture.image() : noImage
                property var noImage: BS.BrickLink.noImage(width, height)

                Label {
                    id: infoImageUpdating
                    anchors.fill: parent
                    visible: root.isUpdating
                    color: "black"
                    fontSizeMode: Text.Fit
                    font.pointSize: root.font.pointSize * 3
                    font.italic: true
                    minimumPointSize: root.font.pointSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Please wait... updating")
                }

                Glow {
                    anchors.fill: infoImageUpdating
                    visible: root.isUpdating
                    radius: 8
                    spread: 0.5
                    color: "white"
                    source: infoImageUpdating
                }
            }

            LDraw.PartRenderer {
                id: info3D
                renderController: BS.BrickStore.createRenderController(this)
            }
        }
    }
    RowLayout {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        visible: root.single
        Button {
            flat: true
            font.bold: true
            leftPadding: 8
            bottomPadding: 8
            topPadding: 16
            rightPadding: 16
            text: root.is3D ? "2D" : "3D"
            onClicked: root.is3D = !root.is3D
            background: null

            Component.onCompleted: {
                contentItem.color = "black"
            }
        }
        Item { Layout.fillWidth: true }
        Button {
            flat: true
            icon.name: root.is3D ? "zoom-fit-best" : "view-refresh"
            icon.color: "black"
            background: null
            rightPadding: 8
            bottomPadding: 8
            topPadding: 16
            leftPadding: 16
            onClicked: {
                if (root.is3D)
                    info3D.renderController.resetCamera();
                else if (root.picture)
                    root.picture.update(true);
            }
        }
    }
}
