// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import BrickLink as BL


// Shows a BrickLink.Picture, falling back to the "no image available" placeholder while the
// picture is still downloading, or when BrickLink has no picture for the item at all.
//
// The root is a Rectangle wrapping an Image, rather than an Image itself: Image inherits
// implicitWidth and implicitHeight from QQuickImplicitSizeItem, which redeclares them read-only,
// so a call site could not size it at all. The Rectangle doubles as the fill behind a letterboxed
// image, and costs no scene graph node while its color is transparent.

Rectangle {
    id: root

    property BL.Picture picture

    color: "transparent"

    // Deliberately typed as a string: Picture.imageUrl is empty unless there really is an image to
    // serve, but an empty url is not falsy in JS, so coerce it to get a usable test below.
    readonly property string imageUrl: root.picture?.imageUrl ?? ""

    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit

        // no sourceSize: all instances then share one decoded image and one texture per picture
        source: (root.imageUrl !== "") ? root.imageUrl : "image://bricklink/noimage"
    }
}
