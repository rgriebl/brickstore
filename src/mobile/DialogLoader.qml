import Mobile


Loader {
    id: root

    property bool autoUnload: true

    signal accepted()
    signal rejected()
    signal opened()

    asynchronous: true
    active: false
    onLoaded: {
        item.onClosed.connect(() => { if (root) root.active = !root.autoUnload })
        item.onAccepted.connect(() => { root.accepted() })
        item.onRejected.connect(() => { root.rejected() })
        item.open()
        opened()
    }
    function open() {
        if (status === Loader.Ready) {
            item.open()
            opened()
        } else {
            active = true
        }
    }
}
