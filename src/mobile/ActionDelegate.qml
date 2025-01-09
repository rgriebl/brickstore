// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile
import BrickStore


ItemDelegate {
    id: control

    property string actionName
    property string infoText

    action: ActionManager.quickAction(actionName)
    Layout.fillWidth: true
    icon.color: "transparent"

    Tracer { }

    height: visible ? implicitHeight : 0

//    contentItem: Control {
//        implicitWidth: icon.implicitWidth + control.spacing + label.implicitWidth
//        implicitHeight: Math.max(icon.implicitHeight, label.implicitHeight)
//        IconImage {
//            id: icon
//            anchors {
//                left: parent.left
//                verticalCenter: parent.verticalCenter
//            }
//            name: control.icon.name
//            source: control.icon.source
//            sourceSize.width:  control.icon.width
//            sourceSize.height: control.icon.height
//            color: control.icon.color
//            cache: control.icon.cache
//        }
//        Label {
//            id: label
//            anchors {
//                left: parent.left
//                leftMargin: icon.width + (icon.width ? control.spacing : 0)
//                verticalCenter: parent.verticalCenter
//            }
//            text: control.text
//            font: control.font
//            color: control.enabled ? control.Material.foreground : control.Material.hintTextColor
//        }
//    }
}
