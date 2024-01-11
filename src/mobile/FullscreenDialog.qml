// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import Qt5Compat.GraphicalEffects

Page {
    id: root

    signal backClicked()

    property alias toolButtons: toolbar.data

    header: ToolBar {
        topPadding: Style.topScreenMargin
        RowLayout {
            id: toolbar
            anchors.fill: parent
            ToolButton {
                icon.name: "go-previous"
                onClicked: root.backClicked()
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
}
