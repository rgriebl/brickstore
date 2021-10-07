import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay

    property alias text: label.text
    property int clickedButton: 0

    Label {
        id: label
        horizontalAlignment: Text.AlignHCenter
    }

    Connections {
        target: footer
        function onClicked(button) {
            for (let b = DialogButtonBox.FirstButton; b <= DialogButtonBox.LastButton; b <<= 1) {
                if (root.standardButton(b) === button) {
                    root.clickedButton = b
                    break
                }
            }
        }
    }
}
