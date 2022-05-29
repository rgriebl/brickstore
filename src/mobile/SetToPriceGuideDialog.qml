import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BrickStore

Dialog {
    id: root
    title: qsTr("Set To Price Guide")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    standardButtons: DialogButtonBox.Cancel | DialogButtonBox.Ok
//    width: Overlay.overlay.width * 3 / 4
//    height: Overlay.overlay.height * 3 / 4

    property int time: BrickLink.Time.PastSix
    property int price: BrickLink.Price.WAverage
    property bool forceUpdate: false

    ButtonGroup {
        id: timeGroup
        onCheckedButtonChanged: time = checkedButton.value
    }
    ButtonGroup {
        id: priceGroup
        onCheckedButtonChanged: price = checkedButton.value
    }

    function checkButton(buttons, value) {
        for (let i = 0; i < buttons.length; ++i) {
            if (buttons[i].value === value) {
                buttons[i].checked = true
                break
            }
        }
    }

    Component.onCompleted: {
        force.checked = root.forceUpdate
        checkButton(timeButtons.children, root.time)
        checkButton(priceButtons.children, root.price)
    }

    ColumnLayout {
        anchors.fill: parent
        Label { text: qsTr("The prices of all selected items will be set to Price Guide values.<br /><br />Select which part of the price guide should be used:") }
        RowLayout {
            id: timeButtons
            Button {
                ButtonGroup.group: timeGroup
                checkable: true
                text: qsTr("Last 6 Months Sales")
                property int value: BrickLink.Time.PastSix
            }
            Button {
                ButtonGroup.group: timeGroup
                checkable: true
                text: qsTr("Current Inventory")
                property int value: BrickLink.Time.Current
            }
        }
        RowLayout {
            id: priceButtons
            Button {
                ButtonGroup.group: priceGroup
                checkable: true
                text: qsTr("Minimum")
                property int value: BrickLink.Price.Lowest
            }
            Button {
                ButtonGroup.group: priceGroup
                checkable: true
                text: qsTr("Average")
                property int value: BrickLink.Price.Average
            }
            Button {
                ButtonGroup.group: priceGroup
                checkable: true
                text: qsTr("Quantity Average")
                property int value: BrickLink.Price.WAverage
            }
            Button {
                ButtonGroup.group: priceGroup
                checkable: true
                text: qsTr("Maximum")
                property int value: BrickLink.Price.Highest
            }
        }
        Label {
            text: qsTr("Advanced options")
            MouseArea {
                anchors.fill: parent
                onClicked: { advanced.visible = !advanced.visible }
            }
        }
        ColumnLayout {
            id: advanced
            enabled: visible
            visible: false
            Label { text: qsTr("Only use these options if you know what you are doing!") }
            Switch {
                id: force
                text: qsTr("Download even if already in cache.")
                onToggled: root.forceUpdate = checked
            }
        }
    }
}
