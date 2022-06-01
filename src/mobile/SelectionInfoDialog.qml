import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore

Dialog {
    id: root
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: single ? parent.width * 3 / 4 : implicitWidth
    height: single ? parent.height * 3 / 4 : implicitHeight

    property Document document
    property bool single: false
    property bool isUpdating: false
    property bool is3D: button3D.checked
    property Picture picture
    property var renderController

    function updateStats() {
        let selected = document.selectedLots.length
        single = (selected === 1)

        if (single) {
            let lot = BrickLink.lot(document.selectedLots[0])
            infoText.text = BrickLink.itemHtmlDescription(lot.item, lot.color, infoText.palette.highlight)
            root.title = ''

            if (picture)
                picture.release()
            picture = BrickLink.picture(lot.item, lot.color, true)
            if (picture)
                picture.addRef()
            renderController.setPartAndColor(lot.item, lot.color)

            isUpdating = (picture && (picture.updateStatus === BrickLink.UpdateStatus.Updating))
        } else {
            root.title = (selected === 0) ? qsTr("Document statistics")
                                          : qsTr("Multiple lots selected")
            let stat = document.selectionStatistics()
            infoText.text = stat.asHtmlTable()
        }
    }

    Component.onCompleted: {
        renderController = BrickStore.createRenderController()
        renderController.parent = root
        info3D.setSource("qrc:/ldraw/PartRenderer.qml", { "renderController": renderController })

        document.model.statisticsChanged.connect(function() { root.updateStats() })
    }
    Component.onDestruction: {
        if (picture)
            picture.release()
    }

    onAboutToShow: { updateStats() }
    onClosed: {
        infoImage.clear()
        isUpdating = false
    }

    Connections {
        target: root.picture
        function onUpdateStatusChanged() {
            isUpdating = (picture && (picture.updateStatus === BrickLink.UpdateStatus.Updating))
        }
    }

    ColumnLayout {
        anchors.fill: parent
        Label {
            id: infoText
            textFormat: Text.RichText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        StackLayout {
            visible: root.single
            clip: true

            currentIndex: root.is3D ? 2 : (root.isUpdating ? 0 : 1)

            Label {
                id: infoNoImage
                color: "black"
                background: Rectangle { color: "white" }
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "<center><i>" + qsTr("Please wait... updating") + "</i></center>"
            }
            QImageItem {
                id: infoImage
                fillColor: "white"
                image: root.picture && root.picture.isValid ? root.picture.image() : noImage
                property var noImage: BrickLink.noImage(width, height)
            }
            Loader {
                id: info3D
                asynchronous: true
            }
        }

        RowLayout {
            visible: root.single

            function markText(text : string, marked : bool) {
                if (marked)
                    text = "\u2308" + text + "\u230b"
                return text
            }

            Button {
                Layout.fillWidth: true
                autoExclusive: true
                checkable: true
                checked: true
                text: parent.markText("2D", checked)
            }
            Button {
                id: button3D
                Layout.fillWidth: true
                autoExclusive: true
                checkable: true
                text: parent.markText("3D", checked)
            }
            Button {
                Layout.fillWidth: true
                icon.name: root.is3D ? "zoom-fit-best" : "view-refresh"
                onClicked: {
                    if (root.is3D)
                        root.renderController.resetCamera();
                    else if (root.picture)
                        root.picture.update(true);
                }
            }
        }
    }
}
