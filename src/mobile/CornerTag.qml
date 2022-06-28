import Mobile
import Qt5Compat.GraphicalEffects


Control {
    id: root
    property alias text: label.text
    property alias bold: label.font.bold
    property color color

    anchors.bottom: parent.bottom
    anchors.right: parent.right

    implicitWidth: gradient.implicitWidth
    implicitHeight: gradient.implicitHeight

    RadialGradient {
        id: gradient

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: Math.max(label.width, label.height) * 1.5
        height: width
        horizontalOffset: width / 2
        verticalOffset: height / 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.color }
            GradientStop { position: 0.6; color: root.color }
            GradientStop { position: 1; color: "transparent" }
        }
    }
    Label {
        id: label
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: 2
    }

}
