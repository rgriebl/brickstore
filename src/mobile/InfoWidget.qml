import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import BrickStore as BS
import BrickLink as BL
import Mobile
import LDraw

Control {
    id: root
    property BS.Document document
    property var lot

    property bool single: false
    property bool is3D: false
    property BL.Picture picture
    property bool isUpdating: (picture && (picture.updateStatus === BL.BrickLink.UpdateStatus.Updating))

    onLotChanged: { updateInfo() }

    function updateInfo() {
        single = (lot !== undefined)

        if (picture)
            picture.release()
        picture = null

        if (single) {
            infoText.text = ''
            picture = BL.BrickLink.picture(lot.item, lot.color, true)
            if (picture)
                picture.addRef()

            Qt.callLater(function() {
                let has3D = info3D.renderController.setPartAndColor(lot.item, lot.color)
                if (root.is3D && !has3D)
                    root.is3D = false
                else if (!root.is3D && has3D && (!root.picture || !root.picture.isValid))
                    root.is3D = true;
            })
        } else {
            info3D.renderController.setPartAndColor(BL.BrickLink.noItem, BL.BrickLink.noColor)

            let stat = document.selectionStatistics()
            infoText.text = stat.asHtmlTable()
        }
    }

    Component.onDestruction: {
        if (picture)
            picture.release()
    }

    StackLayout {
        anchors.fill: parent
        clip: true
        currentIndex: !root.single ? 0
                                   : (!root.is3D ? (!root.picture || !root.picture.isValid ? 1
                                                                                           : 2)
                                                 : 3)
        Label {
            id: infoText
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            leftPadding: 8
        }

        Label {
            id: infoNoImage
            color: "#DA4453"
            fontSizeMode: Text.Fit
            font.pointSize: root.font.pointSize * 3
            font.italic: true
            minimumPointSize: root.font.pointSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: qsTr("No image available")
        }

        QImageItem {
            id: infoImage
            fillColor: "white"
            image: root.picture && root.picture.isValid ? root.picture.image : noImage
            property var noImage: BL.BrickLink.noImage(0, 0)

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

        PartRenderer {
            id: info3D
            renderController: RenderController { }
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
            onClicked: { root.is3D = !root.is3D }
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
