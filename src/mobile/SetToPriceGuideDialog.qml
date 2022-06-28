import Mobile
import BrickLink as BL


Dialog {
    id: root
    title: qsTr("Set To Price Guide")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    standardButtons: DialogButtonBox.Cancel | DialogButtonBox.Ok
//    width: Overlay.overlay.width * 3 / 4
//    height: Overlay.overlay.height * 3 / 4

    property int time: BL.BrickLink.Time.PastSix
    property int price: BL.BrickLink.Price.WAverage
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

    component CheckButton : Button {
        checkable: true
        property int value
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
            CheckButton {
                ButtonGroup.group: timeGroup
                text: qsTr("Last 6 Months Sales")
                value: BL.BrickLink.Time.PastSix
            }
            CheckButton {
                ButtonGroup.group: timeGroup
                text: qsTr("Current Inventory")
                value: BL.BrickLink.Time.Current
            }
        }
        RowLayout {
            id: priceButtons
            CheckButton {
                ButtonGroup.group: priceGroup
                text: qsTr("Minimum")
                value: BL.BrickLink.Price.Lowest
            }
            CheckButton {
                ButtonGroup.group: priceGroup
                text: qsTr("Average")
                value: BL.BrickLink.Price.Average
            }
            CheckButton {
                ButtonGroup.group: priceGroup
                text: qsTr("Quantity Average")
                value: BL.BrickLink.Price.WAverage
            }
            CheckButton {
                ButtonGroup.group: priceGroup
                text: qsTr("Maximum")
                value: BL.BrickLink.Price.Highest
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
