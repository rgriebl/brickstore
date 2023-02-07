// pragma ComponentBehavior: Bound  // not allowed: QTBUG-110980

import QtQuick.Controls


ComboBox {
    id: control
    property string iconNameRole
    property string iconSourceRole

    delegate: ItemDelegate {
        width: control.width
        font: control.font
        icon.name: control.iconNameRole
                   ? (Array.isArray(control.model) ? modelData[control.iconNameRole] : model[control.iconNameRole])
                   : undefined
        icon.source: control.iconSourceRole
                     ? (Array.isArray(control.model) ? modelData[control.iconSourceRole] : model[control.iconSourceRole])
                     : undefined
        icon.color: "transparent"
        text: control.textRole
              ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
              : modelData
        highlighted: control.highlightedIndex === index
    }
    contentItem: ItemDelegate {
        // ignore all mouse/touch events and disable the background
        containmentMask: QtObject {
            function contains(point: point) : bool { return false }
        }
        font: control.font
        background: null
        hoverEnabled: false
        icon.name: control.iconNameRole
                   ? (Array.isArray(control.model) ? control.model[control.currentIndex][control.iconNameRole]
                                                   : model.get(control.currentIndex)[control.iconNameRole])
                   : undefined
        icon.source: control.iconSourceRole
                     ? (Array.isArray(control.model) ? control.model[control.currentIndex][control.iconSourceRole]
                                                     : model.get(control.currentIndex)[control.iconSourceRole])
                     : undefined
        icon.color: "transparent"
        text: control.displayText
    }
}
