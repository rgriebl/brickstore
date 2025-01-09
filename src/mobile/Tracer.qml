// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import BrickStore as BS


MouseArea {
    id: root
    property bool fill: false

    anchors.fill: parent
    visible: BS.BrickStore.debug.showTracers
    acceptedButtons: Qt.RightButton
    propagateComposedEvents: true

    readonly property color color: randomColor()
    function randomColor() : color {
        return Qt.rgba(Math.random(), Math.random(), Math.random(), 1.0)
    }

    onPressAndHold: {
        console.log('Tracing' + root.parent + ':')
        let indent = '* ';
        for (let p = root.parent; p; p = p.parent) {
            console.log(indent + p)
            indent = '  ' + indent;
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: .5
        color: root.fill ? Qt.rgba(root.color.r, root.color.g, root.color.b, 0.3) : 'transparent'
        border.color: root.color
        border.width: 1
        opacity: 0.8
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: parent.width
        anchors.margins: 4
        horizontalAlignment: Text.AlignRight
        font.pixelSize: 10
        text: root.parent.objectName
        color: root.color
    }
}
