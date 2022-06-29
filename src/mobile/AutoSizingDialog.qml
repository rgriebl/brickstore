import Mobile
import QtQuick.Controls // needed because of Qt bug (Overlay is not defined)
import QtQuick.Window


Dialog {
    id: root
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Overlay.overlay ? Overlay.overlay.width * (Style.smallSize ? 1 : relativeWidth) : 0
    height: Overlay.overlay ? Overlay.overlay.height * (Style.smallSize ? 1 : relativeHeight) : 0

//    property bool forceSmall: false

    property real relativeWidth: .75
    property real relativeHeight: .75

    property bool keepPaddingInSmallMode: false

    focus: true

//    // ~ in mm
//    property size physicalScreenSize: Qt.size(Math.min(Screen.width, Screen.height) / Screen.pixelDensity,
//                                              Math.max(Screen.width, Screen.height) / Screen.pixelDensity)

//    // small is defined as "smaller than 8cm x 12cm"
//    property bool small: (physicalScreenSize.width < 80) || (physicalScreenSize.height < 120) || forceSmall

    property int defaultTopPadding: 0
    property int defaultLeftPadding: 0
    property int defaultRightPadding: 0
    property int defaultBottomPadding: 0
    property Item defaultHeader: null

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
                textFormat: Text.RichText
                elide: Label.ElideLeft
                horizontalAlignment: Qt.AlignLeft
            }
        }
    }

    //TODO: fix after Key_Back handling is fixed in 6.4 for Popup
    onOpened: contentItem.forceActiveFocus()

    Connections {
        target: Style
        function onSmallSizeChanged() {
            root.switchSmallStyle()
        }
    }
    Component.onCompleted: {
        header.textFormat = Text.RichText
        switchSmallStyle()
        contentItem.focus = true
        contentItem.Keys.released.connect(function(e) {
            if (e.key === Qt.Key_Back) {
                e.accept = true
                close()
            }
        })
    }
    function switchSmallStyle() {
        if (Style.smallSize) {
            if (!defaultHeader) {
                defaultHeader = header
                defaultTopPadding = topPadding
                defaultLeftPadding = leftPadding
                defaultRightPadding = rightPadding
                defaultBottomPadding = bottomPadding
            }
            header = smallHeader
            if (keepPaddingInSmallMode) {
                topPadding = defaultTopPadding / 2
                leftPadding = defaultLeftPadding / 2
                rightPadding = defaultRightPadding / 2
                bottomPadding = defaultBottomPadding / 2
            } else {
                topPadding = 0
                leftPadding = 0
                rightPadding = 0
                bottomPadding = 0
            }
        } else {
            if (defaultHeader) {
                header = defaultHeader
                topPadding = defaultTopPadding
                leftPadding = defaultLeftPadding
                rightPadding = defaultRightPadding
                bottomPadding = defaultBottomPadding
            }
        }
    }
}

