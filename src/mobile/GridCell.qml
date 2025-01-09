// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

//pragma ComponentBehavior: Bound

import Mobile
import BrickStore as BS


Control {
    id: root

    // we expect to be inside a TableView.delegate
    required property int row       // from TableView
    required property int column    // from TableView
    required property bool selected // from TableView
    required property int textAlignment
    required property var display

    property string text
    property bool alternate: row & 1
    property color tint: "transparent"
    property int textLeftPadding: 0
    property int cellPadding: 2

    Label {
        id: label
        text: root.text

        anchors.fill: parent
        horizontalAlignment: root.textAlignment & 7

        clip: true
        leftPadding: root.cellPadding + root.textLeftPadding
        rightPadding: root.cellPadding + 1
        bottomPadding: 1
        topPadding: 0
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.Fit
        maximumLineCount: 2
        elide: Text.ElideRight
        wrapMode: Text.Wrap

        color: root.selected ? Style.primaryHighlightedTextColor : Style.textColor
    }

    Rectangle {
        anchors.fill: parent
        z: -1
        color: {
            let c = root.selected ? Style.primaryColor : Style.backgroundColor
            if (root.alternate)
                c = Qt.darker(c, 1.1)
            return Qt.tint(c, root.tint)
        }
    }

    Rectangle {
        visible: root.row !== 0
        z: 1
        antialiasing: true
        height: 1 / Screen.devicePixelRatio
        color: Style.hintTextColor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
    }
    Rectangle {
        z: 1
        antialiasing: true
        width: 1 / Screen.devicePixelRatio
        color: Style.hintTextColor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }
}
