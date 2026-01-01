// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import Qt5Compat.GraphicalEffects

Page {
    id: root

    signal backClicked()

    property alias toolButtons: headerBar.rightItem

    header: HeaderBar {
        id: headerBar
        title: root.title
        leftItem: ToolButton {
            icon.name: "go-previous"
            onClicked: root.backClicked()
        }
    }
}
