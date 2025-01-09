// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


Flickable {
    id: root
    clip: true
    contentWidth: width - leftMargin - rightMargin // contentItem.children[0].width
    contentHeight: contentItem.children[0].implicitHeight

    implicitWidth: width
    implicitHeight: contentHeight

    interactive: contentHeight > height

    ScrollIndicator.vertical: ScrollIndicator { }
    FlashScrollIndicators { id: flasher; target: root }
    function flashScrollIndicators() { flasher.flash() }
}
