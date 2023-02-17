// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import BrickStore as BS

MenuItem {
    property string actionName
    action: BS.ActionManager.quickAction(actionName)
    icon.color: "transparent"
    visible: enabled
    height: visible ? implicitHeight : 0

    Tracer { }
}
