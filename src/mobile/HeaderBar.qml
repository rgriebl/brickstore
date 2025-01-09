// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import Mobile
import BrickLink as BL

ToolBar {
    id: root

    topPadding: Style.topScreenMargin
    implicitHeight: topPadding + row.implicitHeight + bottomPadding

    property alias title: titleText.text

    // The text color might be off after switching themes:
    // https://codereview.qt-project.org/c/qt/qtquickcontrols2/+/311756

    ProgressBar {
        id: progressBar
        anchors {
            margins: 2
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        Material.accent: Material.toolTextColor

        to: 0
        visible: to && (value < to)
        Connections {
            target: BL.BrickLink
            function onTransferProgress(progress, total) {
                progressBar.to = total
                progressBar.value = progress
            }
        }
        // NumberAnimation on value {
        //     duration: 2000
        //     from: 0
        //     to: 1
        //     loops: Animation.Infinite
        // }
    }

    property Item leftItem: Item { }
    property Item centerItem: Label {
        id: titleText
        Layout.fillWidth: true
        font.pointSize: ApplicationWindow.window?.font.pointSize * 1.3
        minimumPointSize: font.pointSize / 2
        fontSizeMode: Text.Fit
        elide: Label.ElideLeft
        horizontalAlignment: Qt.AlignLeft
        color: Material.toolTextColor
    }
    property Item rightItem: Item { }

    RowLayout {
        id: row
        anchors.fill: parent
        data: [root.leftItem, root.centerItem, root.rightItem]
    }
}
