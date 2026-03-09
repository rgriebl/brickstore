// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import QtQuick.Window
import QtQuick.Controls.Material


ApplicationWindow {
    id: root
    visible: true
    title: "BrickStore"
    width: 1280
    height: 720

    // colored statusbar background
    flags: Qt.Window | ((Style.isAndroid || Style.isIOS) ? Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint : 0)

    Binding { // used to apply the dark/light theme for the complete app (Style is a singleton)
        target: Style
        property: "rootWindow"
        value: root
    }

    Rectangle {
        // color the top status bar
        z: 1000
        color: Material.primaryColor
        visible: root.SafeArea.margins.top > 0
        height: root.SafeArea.margins.top
        width: root.width
        y: -height
    }
    Rectangle {
        // show a black bar over the notch / camera cutout
        z: 1000
        color: "black"
        visible: root.SafeArea.margins.left > 0
        width: root.SafeArea.margins.left
        height: root.height
        x: -width
    }
    Rectangle {
        // show a black bar over the notch / camera cutout
        z: 1000
        color: "black"
        visible: root.SafeArea.margins.right > 0
        width: root.SafeArea.margins.right
        height: root.height
        x: root.width - root.SafeArea.margins.left - root.SafeArea.margins.right
    }

    Loader {
        id: loader
        asynchronous: true
        anchors.fill: parent
        active: true
        source: "MainWindow.qml"
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: loader.status !== Loader.Ready
        visible: loader.status !== Loader.Ready
    }
}
