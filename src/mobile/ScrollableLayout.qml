import Mobile
import "utils.js" as Utils


Flickable {
    id: root
    clip: true
    contentWidth: contentItem.children[0].width
    contentHeight: contentItem.children[0].implicitHeight

    implicitWidth: contentWidth
    implicitHeight: contentHeight

    interactive: contentHeight > height

    function flashScrollIndicators() { Utils.flashScrollIndicators(root) }

    ScrollIndicator.vertical: ScrollIndicator { }
}
