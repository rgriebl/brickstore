// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

//pragma ComponentBehavior: Bound

import Mobile
import BrickStore as BS


Control {
    id: root

    // we expect to be inside a HeaderView.delegate for a Document

    required property int index
    required property string display
    required property BS.Document document
    property int visualIndex: index
    property int logicalIndex: document.logicalColumn(index)
    property int sortStatus: 0
    property int sortCount: 0

    readonly property real cellPadding: 2

    signal showMenu(logicalColumn: int, visualColumn: int)

    Connections {
        target: root.document
        function onSortColumnsChanged() { root.checkSortStatus() }
    }
    Component.onCompleted: { checkSortStatus() }

    function checkSortStatus() {
        let sorted = 0
        let sc = document.sortColumns
        for (let i = 0; i < sc.length; ++i) {
            if (sc[i].column === logicalIndex) {
                sorted = (i + 1) * (sc[i].order === Qt.AscendingOrder ? -1 : 1);
                break
            }
        }
        root.sortCount = sc.length
        root.sortStatus = sorted
    }

    FontMetrics { id: fm }

    // based on Qt's Material header
    implicitWidth: 20
    implicitHeight: fm.height * 2

    Text {
        anchors.fill: parent

        id: headerText
        text: root.display
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: enabled ? Style.textColor : Style.hintTextColor
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: font.pixelSize / 4
        elide: Text.ElideMiddle
        clip: true
        padding: root.cellPadding
        font.pixelSize: height / 2
        font.bold: root.sortStatus === 1 || root.sortStatus === -1

        Rectangle {
            id: rect
            z: -1
            anchors.fill: parent
            color: Style.backgroundColor
            property color gradientColor: Style.accentColor
            property real d: root.sortCount && root.sortStatus ? (root.sortStatus < 0 ? -1 : 1) / root.sortCount : 0
            property real e: (root.sortCount - Math.abs(root.sortStatus) + 1) * d
            gradient: Gradient {
                GradientStop { position: 0; color: root.sortStatus <= 0 ? rect.color : rect.gradientColor }
                GradientStop { position: 0.5 + 0.4 * rect.e; color: rect.color }
                GradientStop { position: 1; color: root.sortStatus >= 0 ? rect.color : rect.gradientColor }
            }
        }
    }

    Rectangle {
        z: 1
        antialiasing: true
        height: 1 / Screen.devicePixelRatio
        color: Style.hintTextColor
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }
    Rectangle {
        z: 1
        antialiasing: true
        width: 1 / Screen.devicePixelRatio
        color: Style.hintTextColor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }
}

//                DragHandler {
//                    id: columnMoveHandler
//                    onActiveChanged: {
//                        if (!active) {
//                            let pos = centroid.position
//                            console.log("drop at " + pos.x)
//                            pos = parent.mapToItem(header.contentItem, pos)
//                            console.log("drop recalc at " + pos.x)
//                            let drop = header.contentItem.childAt(pos.x, 4)
//                            console.log("drop on " + drop)
//                            console.log("drop on   vindex " + drop.visualIndex + " / lindex " + drop.logicalIndex)
//                            console.log("drag from vindex " + parent.visualIndex + " / lindex " + parent.logicalIndex)

//                            if (drop === parent) {
//                                console.log("drop on myself")
//                                return
//                            }

//                            if (drop && (parent.visualIndex !== drop.visualIndex)) {
//                                document.moveColumn(parent.logicalIndex, visualIndex, drop.visualIndex)
//                            }
//                        }
//                    }
//                }
