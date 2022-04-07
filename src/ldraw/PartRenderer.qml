import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import BrickStore

import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var renderController
    property alias rc: root.renderController

    Connections {
        target: rc
        function onQmlResetCamera() {
            modelAnimation.enabled = true
            rootNode.rotation = rc.standardRotation
            scaleToFit()
            modelAnimation.enabled = false
        }
        function onPartOrColorChanged() {
            root.scaleToFit()
        }
    }

    onWidthChanged: scaleToFit()
    onHeightChanged: scaleToFit()

    function scaleToFit() {
        let d = rc.radius
        let view = (width < height) ? width : height
        let z = (d > 0) ? (view / d) : 1
        rootNode.scale = Qt.vector3d(z, z, z)
        rootNode.position = Qt.vector3d(0, 0, 0)
    }

    View3D {
        id: view
        anchors.fill: parent

        renderMode: View3D.Offscreen

        environment: SceneEnvironment {
            id: env
            clearColor: "white"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.SSAA
            antialiasingQuality: SceneEnvironment.VeryHigh
            lightProbe: Texture { source: "qrc:/ldraw/lightbox.hdr" }
            probeExposure: rc.lightProbeExposure
            tonemapMode: SceneEnvironment.TonemapModeLinear
            aoStrength: rc.aoStrength * 100
            aoDistance: rc.aoDistance * 100
            aoSoftness: rc.aoSoftness * 100
        }

        OrthographicCamera {
            id: camera
            z: 300
        }

        Node {
            id: rootNode
            rotation: rc.standardRotation
            pivot: rc.center

            Behavior on rotation {
                id: modelAnimation
                QuaternionAnimation { duration: 500 }
                enabled: false
            }
            Behavior on position {
                Vector3dAnimation { duration: 500 }
                enabled: modelAnimation.enabled
            }
            Behavior on scale {
                Vector3dAnimation { duration: 500 }
                enabled: modelAnimation.enabled
            }

            Model {
                source: "#Sphere"
                visible: rc.showBoundingSpheres
                position: rc.center
                property real radius: rc.radius * 2 / 100
                scale: Qt.vector3d(radius, radius, radius)
                materials: PrincipledMaterial {
                    baseColor: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
                    opacity: 0.3
                }
            }

            Repeater3D {
                model: rc.surfaces.length

                Model {
                    property var ldrawGeometry: rc.surfaces[index]

                    source: "#Sphere"
                    visible: rc.showBoundingSpheres
                    position: ldrawGeometry ? ldrawGeometry.center : Qt.vector3d(0, 0, 0)
                    property real radius: ldrawGeometry ? (ldrawGeometry.radius * 2 / 100) : 1
                    scale: Qt.vector3d(radius, radius, radius)
                    materials: PrincipledMaterial {
                        baseColor: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
                        opacity: 0.3
                    }
                }
            }

            Repeater3D {
                model: rc.surfaces.length
                Model {
                    id: model
                    property var ldrawGeometry: rc.surfaces[index]

                    geometry: ldrawGeometry
                    materials: PrincipledMaterial {
                        id: material
                        property real colorId      : model.ldrawGeometry ? model.ldrawGeometry.color.id : -1
                        property color color       : model.ldrawGeometry ? model.ldrawGeometry.color.color : "black"
                        property real luminance    : model.ldrawGeometry ? model.ldrawGeometry.color.luminance : 0
                        property bool chrome       : model.ldrawGeometry && model.ldrawGeometry.color.chrome
                        property bool metal        : model.ldrawGeometry && model.ldrawGeometry.color.metal
                        property bool matteMetallic: model.ldrawGeometry && model.ldrawGeometry.color.matteMetallic
                        property bool pearlescent  : model.ldrawGeometry && model.ldrawGeometry.color.pearlescent
                        property bool rubber       : model.ldrawGeometry && model.ldrawGeometry.color.rubber
                        property bool transparent  : model.ldrawGeometry && model.ldrawGeometry.color.transparent

                        baseColor: (colorId == 0) ? Qt.lighter(color) : color

                        cullMode     : transparent ? Material.NoCulling          : Material.BackFaceCulling
                        depthDrawMode: transparent ? Material.NeverDepthDraw     : Material.AlwaysDepthDraw
                        alphaMode    : transparent ? PrincipledMaterial.Blend    : PrincipledMaterial.Opaque
                        blendMode    : transparent ? PrincipledMaterial.Multiply : PrincipledMaterial.SourceOver

                        emissiveFactor: Qt.vector3d(luminance, luminance, luminance)

                        metalness: chrome ? rc.chromeMetalness
                                          : metal ? rc.metalMetalness
                                                  : matteMetallic ? rc.matteMetallicMetalness
                                                                  : pearlescent ? rc.pearlescentMetalness
                                                                                : rubber ? rc.rubberMetalness
                                                                                         : rc.plainMetalness
                        roughness: chrome ? rc.chromeRoughness
                                          : metal ? rc.metalRoughness
                                                  : matteMetallic ? rc.matteMetallicRoughness
                                                                  : pearlescent ? rc.pearlescentRoughness
                                                                                : rubber ? rc.rubberRoughness
                                                                                         : rc.plainRoughness
                    }
                }
            }
        }

        Timer {
            id: animation
            running: rc.animationActive
            repeat: true
            interval: 16
            onTriggered: {
                rootNode.rotate(rc.tumblingAnimationAngle,
                                rc.tumblingAnimationAxis, Node.LocalSpace)
            }
        }

        HoverHandler {
            cursorShape: Qt.OpenHandCursor
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onSingleTapped: (eventPoint, button) => {
                                rc.requestContextMenu(eventPoint.scenePosition)
                            }
        }

        TapHandler {
            onDoubleTapped: {
                settings.visible = !settings.visible
            }
        }

        WheelHandler {
            target: null
            cursorShape: Qt.SizeVerCursor
            onWheel: (event) => {
                         let d = 1.0 + (event.angleDelta.y / 1200.0)
                         rootNode.scale = rootNode.scale.times(d)
                     }
        }

        DragHandler {
            property vector3d pressPosition
            property bool animationWasActive: false

            acceptedButtons: Qt.RightButton

            target: null
            dragThreshold: 0
            cursorShape: Qt.SizeAllCursor

            onActiveChanged: {
                if (active) {
                    animationWasActive = rc.animationActive
                    rc.animationActive = false
                    pressPosition = rootNode.position
                } else {
                    rc.animationActive = animationWasActive
                }
            }
            onActiveTranslationChanged: {
                if (!active)
                    return
                let offset = Qt.vector3d(2 * activeTranslation.x, -2 * activeTranslation.y, 0)
                rootNode.position = pressPosition.plus(offset)
            }
        }

        DragHandler {
            property quaternion pressRotation
            property point pressPos
            property bool animationWasActive: false

            target: null
            dragThreshold: 0
            cursorShape: Qt.ClosedHandCursor

            onActiveChanged: {
                if (active) {
                    animationWasActive = rc.animationActive
                    rc.animationActive = false
                    pressRotation = rootNode.rotation
                    pressPos = centroid.pressPosition
                } else {
                    rc.animationActive = animationWasActive
                }
            }
            onActiveTranslationChanged: {
                if (!active)
                    return
                let mousePos = Qt.point(pressPos.x + activeTranslation.x,
                                        pressPos.y + activeTranslation.y)

                rootNode.rotation = rc.rotateArcBall(pressPos, mousePos, pressRotation,
                                                     Qt.size(view.width, view.height))
            }
        }
    }
    ApplicationWindow {
        id: settings
        visible: false
        flags: Qt.Dialog
        title: "3D Render Settings"

        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth

            GridLayout {
                width: parent.width
                anchors.fill: parent
                anchors.margins: 11
                columnSpacing: 9
                rowSpacing: 6
                columns: 3

                component VSection: Label {
                    Layout.columnSpan: 3
                    font.bold: true
                }

                component VLabel: Label {
                    Layout.leftMargin: 11
                }

                component VSlider: Slider {
                    Layout.fillWidth: true
                }

                component VValue: Label {
                    required property var value
                    property string unit: " %"
                    property real factor: 100
                    text: Math.round(value * 100) / 100 * factor + unit
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }

                VSection { text: "Debug" }
                VLabel { text: "Show bounding spheres" }
                Switch { checked: rc.showBoundingSpheres; onToggled: rc.showBoundingSpheres = checked; Layout.columnSpan: 2 }

                VSection { text: "Tumbling Animation" }
                VLabel { text: "Angle" }
                VSlider { value: rc.tumblingAnimationAngle; onValueChanged: rc.tumblingAnimationAngle = value }
                VValue { value: rc.tumblingAnimationAngle; factor: 1; unit: "°" }

                VSection { text: "Ambient Occlusion" }
                VLabel { text: "Strength" }
                VSlider { value: rc.aoStrength; onValueChanged: rc.aoStrength = value }
                VValue { value: rc.aoStrength }
                VLabel { text: "Softness" }
                VSlider { value: rc.aoSoftness; onValueChanged: rc.aoSoftness = value }
                VValue { value: rc.aoSoftness }
                VLabel { text: "Distance" }
                VSlider { value: rc.aoDistance; onValueChanged: rc.aoDistance = value }
                VValue { value: rc.aoDistance }

                VSection { text: "Plain Material" }
                VLabel { text: "Metalness" }
                VSlider { value: rc.plainMetalness; onValueChanged: rc.plainMetalness = value }
                VValue { value: rc.plainMetalness }
                VLabel { text: "Roughness" }
                VSlider { value: rc.plainRoughness; onValueChanged: rc.plainRoughness = value }
                VValue { value: rc.plainRoughness }

                VSection { text: "Chrome Material" }
                VLabel { text: "Metalness" }
                VSlider { value: rc.chromeMetalness; onValueChanged: rc.chromeMetalness = value }
                VValue { value: rc.chromeMetalness }
                VLabel { text: "Roughness" }
                VSlider { value: rc.chromeRoughness; onValueChanged: rc.chromeRoughness = value }
                VValue { value: rc.chromeRoughness }

                VSection { text: "Metal Material" }
                VLabel { text: "Metalness" }
                VSlider { value: rc.metalMetalness; onValueChanged: rc.metalMetalness = value }
                VValue { value: rc.metalMetalness }
                VLabel { text: "Roughness" }
                VSlider { value: rc.metalRoughness; onValueChanged: rc.metalRoughness = value }
                VValue { value: rc.metalRoughness }

                VSection { text: "Matte Metallic Material" }
                VLabel { text: "Metalness" }
                VSlider { value: rc.matteMetallicMetalness; onValueChanged: rc.matteMetallicMetalness = value }
                VValue { value: rc.matteMetallicMetalness }
                VLabel { text: "Roughness" }
                VSlider { value: rc.matteMetallicRoughness; onValueChanged: rc.matteMetallicRoughness = value }
                VValue { value: rc.matteMetallicRoughness }

                VSection { text: "Pearlescent Material" }
                VLabel { text: "Metalness" }
                VSlider { value: rc.pearlescentMetalness; onValueChanged: rc.pearlescentMetalness = value }
                VValue { value: rc.pearlescentMetalness }
                VLabel { text: "Roughness" }
                VSlider { value: rc.pearlescentRoughness; onValueChanged: rc.pearlescentRoughness = value }
                VValue { value: rc.pearlescentRoughness }

                VSection { text: "Rubber Material" }
                VLabel { text: "Metalness" }
                VSlider { value: rc.rubberMetalness; onValueChanged: rc.rubberMetalness = value }
                VValue { value: rc.rubberMetalness }
                VLabel { text: "Roughness" }
                VSlider { value: rc.rubberRoughness; onValueChanged: rc.rubberRoughness = value }
                VValue { value: rc.rubberRoughness }

            }
        }
    }
}
