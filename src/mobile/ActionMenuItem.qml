import BrickStore as BS

MenuItem {
    property string actionName
    action: BS.ActionManager.quickAction(actionName)
    icon.color: "transparent"
    visible: enabled
    height: visible ? implicitHeight : 0

    Tracer { }
}
