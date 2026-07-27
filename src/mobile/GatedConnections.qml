// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick

// Workaround for QTBUG-148459: keep the target unset until incubation has
// finished, so connectSignalsToMethods() cannot run (and crash) mid-incubation.
Connections {
    property QtObject realTarget: null
    property bool ready: false

    target: ready ? realTarget : null
    Component.onCompleted: ready = true
}
