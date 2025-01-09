// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import BrickStore as BS

MenuItem {
    property string actionName
    property bool autoHide: true
    action: BS.ActionManager.quickAction(actionName)
    icon.color: "transparent"
    visible: !autoHide || enabled
    height: visible ? implicitHeight : 0

    Tracer { }
}
