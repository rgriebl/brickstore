// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

pragma ComponentBehavior: Bound

import Mobile
import BrickLink as BL


Pane {
    id: root

    property BL.Item currentItem: BL.BrickLink.noItem

    property alias quantity: quantity.value
    property int condition: conditionNew.checked ? BL.BrickLink.Condition.New
                                                 : BL.BrickLink.Condition.Used
    property int extraParts: extraInclude.checked ? BL.BrickLink.Status.Include
                                                  : (extraExclude.checked ? BL.BrickLink.Status.Exclude
                                                                          : BL.BrickLink.Status.Extra)

    property int partOutTraits: 0
                                | (includeInstructions.checked ? BL.BrickLink.PartOutTraits.Instructions : 0)
                                | (includeOriginalBox.checked ? BL.BrickLink.PartOutTraits.OriginalBox : 0)
                                | (includeAlternates.checked ? BL.BrickLink.PartOutTraits.Alternates : 0)
                                | (includeCounterParts.checked ? BL.BrickLink.PartOutTraits.CounterParts : 0)
                                | (partOutSetsInSet.checked ? BL.BrickLink.PartOutTraits.SetsInSet : 0)
                                | (partOutMinifigs.checked ? BL.BrickLink.PartOutTraits.Minifigs : 0)

    Connections { // 2-way binding for ExtraConfig
        target: root
        function onConditionChanged() {
            switch (root.condition) {
            case BL.BrickLink.Condition.New:  conditionNew.checked = true; break
            case BL.BrickLink.Condition.Used: conditionUsed.checked = true; break
            }
        }

        function onExtraPartsChanged() {
            switch (root.extraParts) {
            case BL.BrickLink.Status.Include: extraInclude.checked = true; break
            case BL.BrickLink.Status.Exclude: extraExclude.checked = true; break
            case BL.BrickLink.Status.Extra:   extraExtra.checked = true; break
            }
        }

        function onPartOutTraitsChanged() {
            const pot = root.partOutTraits
            includeInstructions.checked = (pot & BL.BrickLink.PartOutTraits.Instructions)
            includeOriginalBox.checked  = (pot & BL.BrickLink.PartOutTraits.OriginalBox)
            includeAlternates.checked   = (pot & BL.BrickLink.PartOutTraits.Alternates)
            includeCounterParts.checked = (pot & BL.BrickLink.PartOutTraits.CounterParts)
            partOutSetsInSet.checked    = (pot & BL.BrickLink.PartOutTraits.SetsInSet)
            partOutMinifigs.checked     = (pot & BL.BrickLink.PartOutTraits.Minifigs)
        }
    }

    onCurrentItemChanged: layout._possibleTraits = root.currentItem.partOutTraits()

    ScrollableLayout {
        id: layout
        anchors.fill: parent

        property int _possibleTraits: BL.BrickLink.PartOutTraits.None

        ColumnLayout {
            enabled: !root.currentItem.isNull
            width: layout.width

            RowLayout {
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 0
                QImageItem {
                    implicitHeight: lfm.height * 5
                    implicitWidth: height * 4 / 3

                    property BL.Picture pic: BL.BrickLink.picture(root.currentItem, BL.BrickLink.noColor, true)
                    image: pic ? pic.image : BL.BrickLink.noImage(width, height)
                }
                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "<b>" + root.currentItem.id + "</b> " + root.currentItem.name
                          + " <i>(" + root.currentItem.itemType.name + ")</i>"
                    textFormat: Text.StyledText
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter

                    FontMetrics { id: lfm }
                }
            }
            GridLayout {
                id: importDetailsGrid
                columns: 2
                columnSpacing: 16
                Layout.rightMargin: 16
                ItemDelegate { text: qsTr("Quantity:") }
                SpinBox { id: quantity; editable: true; from: 1; to: 1000; value: 1 }

                ItemDelegate { text: qsTr("Condition:") }
                Flow {
                    Layout.fillWidth: true
                    spacing: importDetailsGrid.columnSpacing
                    CheckButton { text: qsTr("New"); checked: true; id: conditionNew }
                    CheckButton { text: qsTr("Used"); id: conditionUsed }
                }
                ItemDelegate {
                    text: qsTr("Extra parts:")
                    visible: layout._possibleTraits & BL.BrickLink.PartOutTraits.Extras
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: importDetailsGrid.columnSpacing
                    visible: layout._possibleTraits & BL.BrickLink.PartOutTraits.Extras
                    CheckButton { icon.name: "vcs-normal";  text: qsTr("Include"); id: extraInclude; checked: true }
                    CheckButton { icon.name: "vcs-removed"; text: qsTr("Exclude"); id: extraExclude }
                    CheckButton { icon.name: "vcs-added";   text: qsTr("Extra");   id: extraExtra }
                }

                ItemDelegate { text: qsTr("Include:") }
                Switch {
                    id: includeInstructions
                    text: qsTr("Instructions")
                    enabled: layout._possibleTraits & BL.BrickLink.PartOutTraits.Instructions
                }
                ItemDelegate { }
                Switch {
                    id: includeOriginalBox
                    text: qsTr("Original Box")
                    enabled: layout._possibleTraits & BL.BrickLink.PartOutTraits.OriginalBox
                }
                ItemDelegate { }
                Switch {
                    id: includeAlternates
                    text: qsTr("Alternates")
                    enabled: layout._possibleTraits & BL.BrickLink.PartOutTraits.Alternates
                }
                ItemDelegate { }
                Switch {
                    id: includeCounterParts
                    text: qsTr("Counterparts")
                    enabled: layout._possibleTraits & BL.BrickLink.PartOutTraits.CounterParts
                }

                ItemDelegate { text: qsTr("Also part out:") }
                Switch {
                    id: partOutSetsInSet
                    text: qsTr("Sets in set")
                    enabled: layout._possibleTraits & BL.BrickLink.PartOutTraits.SetsInSet
                }
                ItemDelegate { }
                Switch {
                    id: partOutMinifigs
                    text: qsTr("Minifigs")
                    enabled: layout._possibleTraits & BL.BrickLink.PartOutTraits.Minifigs
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
