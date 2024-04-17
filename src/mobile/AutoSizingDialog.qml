// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import QtQuick.Controls // needed because of Qt bug (Overlay is not defined)
import QtQuick.Window


Dialog {
    id: root
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Overlay.overlay ? Overlay.overlay.width * ((Style.smallSize || root.forceFullscreen) ? 1 : relativeWidth) : 0
    height: Overlay.overlay ? Overlay.overlay.height * ((Style.smallSize || root.forceFullscreen) ? 1 : relativeHeight) : 0

    property bool forceFullscreen: false

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
    property real defaultBackgroundRadius: 0

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
        defaultBackgroundRadius = (background && ('radius' in background)) ? background.radius : -1
        switchSmallStyle()
    }
    function switchSmallStyle() {
        if (Style.smallSize || root.forceFullscreen) {
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
            if (defaultBackgroundRadius >= 0) {
                background.radius = 0
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
            if (defaultBackgroundRadius >= 0) {
                background.radius = defaultBackgroundRadius
            }
        }
    }
}
