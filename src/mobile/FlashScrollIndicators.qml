import QtQml
import QtQuick
import QtQuick.Controls

QtObject {
    id: root
    property Item target: parent
    property bool trigger: false

    onTriggerChanged: {
        if (trigger)
            flash()
    }

    function flash() {
        _timer.stop()
        if (_internalFlash(true))
            _timer.start()
    }

    function _internalFlash(active : bool) : bool {
        if (target.ScrollIndicator.vertical)
            target.ScrollIndicator.vertical.active = active
        else if (target.ScrollBar.vertical)
            target.ScrollBar.vertical.active = active
        else
            return false
        return true
    }

    property Timer _timer: Timer {
        interval: 1000
        repeat: false
        onTriggered: {
            if (!root.target.moving)
                root._internalFlash(false)
        }
    }
}
