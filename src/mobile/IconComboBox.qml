// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

// pragma ComponentBehavior: Bound  // not allowed: QTBUG-110980

import QtQuick.Controls


ComboBox {
    id: control
    property string iconNameRole
    property string iconSourceRole

    delegate: MenuItem {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]
        Material.foreground: control.currentIndex === index ? ListView.view.contentItem.Material.accent : ListView.view.contentItem.Material.foreground
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        icon.name: control.iconNameRole ? model[control.iconNameRole] : undefined
        icon.source: control.iconSourceRole ? model[control.iconSourceRole] : undefined
        icon.color: "transparent"
    }

    contentItem: MenuItem {
        // ignore all mouse/touch events and disable the background
        containmentMask: QtObject {
            function contains(point: point) : bool { return false }
        }
        background: null
        hoverEnabled: false
        icon.name: control.iconNameRole
                   ? (typeof control.model['get'] === 'function'
                      ? control.model.get(control.currentIndex)[control.iconNameRole]
                      : control.model[control.currentIndex][control.iconNameRole])
                   : undefined
        icon.source: control.iconSourceRole
                     ? (typeof control.model['get'] === 'function'
                        ? control.model.get(control.currentIndex)[control.iconSourceRole]
                        : control.model[control.currentIndex][control.iconSourceRole])
                     : undefined
        icon.color: "transparent"
        text: control.displayText
    }
}
