// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import BrickLink as BL


// Shows a BrickLink.Color swatch. Size this from the outside: the swatch is generated at exactly
// the size needed, because the glitter and speckle patterns do not survive scaling.

Image {
    id: root

    required property BL.Color color

    fillMode: Image.PreserveAspectFit

    // sourceSize is in logical pixels - the provider is handed the size scaled by the
    // devicePixelRatio automatically
    sourceSize: Qt.size(root.width, root.height)

    // Only ask once we have a real size: a swatch generated at zero width or height is a null
    // image, which QQuickImage reports as a provider failure.
    source: ((root.width > 0) && (root.height > 0))
            ? ("image://bricklink/color/" + root.color.id) : ""
}
