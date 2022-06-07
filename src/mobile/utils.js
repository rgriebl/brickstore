//.pragma library

function flashScrollIndicator(flickable) {
    if (flickable.ScrollIndicator && flickable.ScrollIndicator.vertical)
        flickable.ScrollIndicator.vertical.active = true
    else
        return

    let timer = Qt.createQmlObject("import QtQuick;Timer{}", flickable)
    timer.interval = 1000
    timer.repeat = false
    timer.triggered.connect(function() {
        timer.destroy()
        if (!flickable.moving)
            flickable.ScrollIndicator.vertical.active = false
    })
    timer.start()
}
