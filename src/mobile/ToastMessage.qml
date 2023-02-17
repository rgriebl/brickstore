// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick.Controls // needed because of Qt bug (Overlay is not defined)
import Mobile

Popup {
    id: root
    property string message
    property int timeout

    closePolicy: Popup.NoAutoClose

    anchors.centerIn: Overlay.overlay

    leftMargin: Overlay.overlay ? Overlay.overlay.width / 5 : 0
    rightMargin: Overlay.overlay ? Overlay.overlay.width / 5 : 0
    bottomMargin: Overlay.overlay ? Overlay.overlay.height / 5 : 0
    topMargin: Overlay.overlay ? Overlay.overlay.height * 4 / 5 : 0

    leftPadding: font.pixelSize
    topPadding: 4
    bottomPadding: 4
    rightPadding: leftPadding

    contentItem: Label {
        text: root.message
        wrapMode: Text.Wrap
        font.bold: true
        color: Style.accentTextColor
    }

    background: Rectangle {
        radius: height / 2
        color: Style.accentColor
        MouseArea {
            z: 1000
            anchors.fill: parent
            onClicked: { root.close() }
        }
    }

    Timer {
        interval: root.timeout
        running: true
        onTriggered: root.close()
    }
}
