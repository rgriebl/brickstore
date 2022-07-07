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

    property real relativeWidth: .75
    property real relativeHeight: .75

    property bool keepPaddingInSmallMode: false

    focus: true

    property int defaultTopPadding: 0
    property int defaultLeftPadding: 0
    property int defaultRightPadding: 0
    property int defaultBottomPadding: 0
    property int defaultFooterLeftPadding: 0
    property int defaultFooterRightPadding: 0
    property Item defaultHeader: null

    property Item smallHeader: ToolBar {
        id: toolBar
        width: root.width
        visible: root.header === this
        topPadding: Style.topScreenMargin

        // The text color might be off after switching themes:
        // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

        RowLayout {
            anchors.fill: parent
            ToolButton {
                Layout.leftMargin: Style.leftScreenMargin + Style.rightScreenMargin
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

    Connections {
        target: Style
        function onScreenMarginsChanged() {
            root.switchSmallStyle()
        }
    }
    Component.onCompleted: {
        if (header && ('textFormat' in header))
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
                if (footer) {
                    defaultFooterLeftPadding = footer.leftPadding
                    defaultFooterRightPadding = footer.rightPadding
                    defaultBottomPadding = footer.bottomPadding
                } else {
                    defaultBottomPadding = bottomPadding
                }
            }
            header = smallHeader
            if (keepPaddingInSmallMode) {
                topPadding = defaultTopPadding / 2
                leftPadding = defaultLeftPadding / 2 + Style.leftScreenMargin
                rightPadding = defaultRightPadding / 2 + Style.rightScreenMargin
                if (footer) {
                    footer.leftPadding = Style.leftScreenMargin
                    footer.rightPadding = Style.rightScreenMargin
                    footer.bottomPadding = Style.bottomScreenMargin
                } else {
                    bottomPadding = defaultBottomPadding / 2
                }
            } else {
                topPadding = 0
                leftPadding = Style.leftScreenMargin
                rightPadding = Style.rightScreenMargin
                if (footer) {
                    footer.leftPadding = Style.leftScreenMargin
                    footer.rightPadding = Style.rightScreenMargin
                    footer.bottomPadding = Style.bottomScreenMargin
                } else {
                    bottomPadding = Style.bottomScreenMargin
                }
            }
        } else {
            if (defaultHeader) {
                header = defaultHeader
                topPadding = defaultTopPadding
                leftPadding = defaultLeftPadding
                rightPadding = defaultRightPadding

                if (footer) {
                    footer.leftPadding = defaultFooterLeftPadding
                    footer.rightPadding = defaultFooterRightPadding
                    footer.bottomPadding = defaultBottomPadding
                } else {
                    bottomPadding = defaultBottomPadding
                }
            }
        }
    }
}

