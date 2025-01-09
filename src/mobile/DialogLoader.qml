// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


Loader {
    id: root

    property bool autoUnload: true

    signal accepted()
    signal rejected()
    signal opened()
    signal closed()

    asynchronous: true
    active: false
    onLoaded: {
        item.onClosed.connect(() => { if (root) { root.closed(); root.active = !root.autoUnload } })
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
