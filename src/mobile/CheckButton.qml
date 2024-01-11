// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

Button {
    flat: !checked
    highlighted: checked
    checkable: true
    autoExclusive: true
    icon.color: "transparent"
    leftPadding: 16
    rightPadding: 16

    property int value: 0
}
