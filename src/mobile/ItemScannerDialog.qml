// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtMultimedia
import Mobile
import BrickLink as BL
import BrickStore as BS
import Scanner as Scanner

pragma ComponentBehavior: Bound

AutoSizingDialog {
    id: root

    required property BS.Document document

    property string addButtonText: qsTr("Add")
    property string addButtonIconName: "edit-additems"

    property BL.Item currentItem

    BS.ExtraConfig {
        category: "ItemScannerDialog"

        property alias zoom: itemList.zoom
        property alias backendId: capture.currentBackendId
        property alias cameraId: capture.currentCameraId
    }

    Scanner.Capture {
        id: capture
        videoOutput: viewFinder

        onCaptureAndScanFinished: function(items) {
            let f = ""
            items.forEach(function (item) { f += (f ? "," : "") + item.itemType.id + item.id })
            itemList.filterText = "scan:" + f
            pages.currentIndex = 1
        }

        onStateChanged: statusText.update()
        Component.onCompleted: statusText.update()
    }
    MediaDevices { id: mediaDevices }

    footer: Control {
        horizontalPadding: Style.smallSize ? 24 : 48
        verticalPadding: 6

        contentItem: StackLayout {
            currentIndex: pages.currentIndex

            Label {
                id: statusText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.StyledText
                wrapMode: Text.WordWrap

                Tracer { }

                function update() {
                    switch (capture.state) {
                    case Scanner.Capture.State.Idle:
                        if (mediaDevices.videoInputs.length)
                            text = qsTr("Tap on the camera preview to capture an image.")
                        else
                            text = "<b>" + qsTr("There is no camera connected.") + "</b>"
                        break
                    case Scanner.Capture.State.NoMatch:
                        text = "<b>" + qsTr("No matching item found - try again.") + "</b>"
                        break
                    case Scanner.Capture.State.Error:
                        text = "<b>" + qsTr("An error occurred:") + "</b><br>" + capture.lastError
                        break
                    }
                }
            }

            RowLayout {
                Button {
                    flat: true
                    icon.name: "help-about"
                    text: qsTr("Info")
                    enabled: !itemList.currentItem.isNull
                    onClicked: {
                        let item = itemList.currentItem
                        infoDialog.selection = [{ "item": item, "color": item?.defaultColor, "quantity": 1 }]
                        infoDialog.open()
                    }

                    SelectionInfoDialog { id: infoDialog }
                }
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    icon.name: "view-refresh"
                    text: qsTr("Retry")
                    onClicked: pages.currentIndex = 0
                }
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    icon.name: root.addButtonIconName
                    text: root.addButtonText
                    enabled: !itemList.currentItem.isNull
                    onClicked: { root.currentItem = itemList.currentItem; root.close() }
                }
            }
        }
    }

    SwipeView {
        id: pages
        interactive: false
        anchors.fill: parent
        anchors.leftMargin: -root.leftPadding
        anchors.rightMargin: -root.rightPadding
        anchors.bottomMargin: -root.bottomPadding
        clip: true

        Pane {
            padding: 0

                Scanner.CameraPreview {
                    id: viewFinder
                    anchors.fill: parent
                    property int buttonInset: 10
                    progress: capture.progress
                    onClicked: capture.captureAndScan()

                    RowLayout {
                        anchors {
                            top: parent.top
                            left: parent.left
                            right: parent.right
                        }

                        Button {
                            id: cameraButton
                            topInset: viewFinder.buttonInset
                            bottomInset: viewFinder.buttonInset
                            leftInset: viewFinder.buttonInset
                            rightInset: viewFinder.buttonInset
                            icon.name: "camera-photo"
                            icon.color: "transparent"
                            onClicked: cameraMenu.open()

                            AutoSizingMenu {
                                id: cameraMenu
                                Repeater {
                                    model: mediaDevices.videoInputs
                                    MenuItem {
                                        required property var modelData
                                        text: modelData.description
                                        icon: cameraButton.icon
                                        onTriggered: capture.currentCameraId = modelData.id
                                    }
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: backendButton
                            topInset: viewFinder.buttonInset
                            bottomInset: viewFinder.buttonInset
                            leftInset: viewFinder.buttonInset
                            rightInset: viewFinder.buttonInset
                            icon.source: Scanner.Core.backendFromId(capture.currentBackendId)?.icon
                            icon.color: "transparent"
                            onClicked: backendMenu.open()

                            AutoSizingMenu {
                                id: backendMenu
                                Repeater {
                                    model: Scanner.Core.availableBackends
                                    MenuItem {
                                        required property var modelData
                                        text: modelData.name
                                        icon.source: modelData.icon
                                        icon.color: "transparent"
                                        onTriggered: capture.currentBackendId = modelData.id
                                    }
                                }
                            }
                        }

                }
            }
        }
        Pane {
            padding: 0
            ItemList {
                id: itemList
                anchors.fill: parent
            }
        }
    }
}
