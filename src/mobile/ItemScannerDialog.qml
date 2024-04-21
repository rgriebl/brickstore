// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtMultimedia
import Mobile
import BrickLink as BL
import BrickStore as BS
import Scanner as Scanner


AutoSizingDialog {
    id: root
    relativeWidth: .8
    relativeHeight: .8

    required property BS.Document document

    Scanner.Capture {
        id: capture
        videoOutput: viewFinder

        onCaptureAndScanFinished: function(items) {
            console.log("scanned", items.length)
        }

        onStateChanged: statusText.update()
        Component.onCompleted: statusText.update()
    }
    MediaDevices { id: mediaDevices }

    ColumnLayout {
        anchors.fill: parent

        Scanner.CameraPreview {
            Layout.fillWidth: true
            Layout.fillHeight: true

            id: viewFinder
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

        Label {
            Layout.fillWidth: true

            id: statusText
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.StyledText
            wrapMode: Text.WordWrap

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
    }
}
