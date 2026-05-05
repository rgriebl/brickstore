// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick.Controls.Material

Button {
    highlighted: checked
    checkable: true
    autoExclusive: true
    icon.color: "transparent"
    leftPadding: 16
    rightPadding: 16
    Material.accent: Material.primary

    property int value: 0
}
