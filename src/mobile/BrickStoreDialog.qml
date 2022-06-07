import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Dialog {
    id: root
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Overlay.overlay.width * (Style.smallSize ? 1 : relativeWidth)
    height: Overlay.overlay.height * (Style.smallSize ? 1 : relativeHeight)

//    property bool forceSmall: false

    property real relativeWidth: .75
    property real relativeHeight: .75

    focus: true

//    // ~ in mm
//    property size physicalScreenSize: Qt.size(Math.min(Screen.width, Screen.height) / Screen.pixelDensity,
//                                              Math.max(Screen.width, Screen.height) / Screen.pixelDensity)

//    // small is defined as "smaller than 8cm x 12cm"
//    property bool small: (physicalScreenSize.width < 80) || (physicalScreenSize.height < 120) || forceSmall

    property Item smallHeader: ToolBar {
        id: toolBar
        width: root.width
        visible: root.header === this

        // The text color might be off after switching themes:
        // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.name: "go-previous"
                onClicked: root.close()
            }
            Label {
                Layout.fillWidth: true
                font.pointSize: root.font.pointSize * 1.3
                minimumPointSize: font.pointSize / 2
                fontSizeMode: Text.Fit
                text: root.title
                elide: Label.ElideLeft
                horizontalAlignment: Qt.AlignLeft
            }
        }
    }

    //TODO: fix after Key_Back handling is fixed in 6.4 for Popup
    onOpened: contentItem.forceActiveFocus()
    Component.onCompleted: {
        if (Style.smallSize)
            header = smallHeader
        contentItem.focus = true
        contentItem.Keys.released.connect(function(e) {
            if (e.key === Qt.Key_Back) {
                e.accept = true
                close()
            }
        })
    }
}

