// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import QtQuick3D
import LDraw

//TODO: enable as soon as Qt 6.2 is not supported anymore
//pragma ComponentBehavior: Bound

Item {
    id: root

    property RenderController renderController: RenderController { }

    Connections {
        target: root.renderController
        function onQmlResetCamera() {
            root.animateScaleToFit()
        }
        function onItemOrColorChanged() {
            root.scaleToFit()
        }
    }

    implicitWidth: 200
    implicitHeight: 200

    onWidthChanged: scaleToFit()
    onHeightChanged: scaleToFit()

    function scaleToFit() {
        if (!renderController)
            return

        let r = root.renderController.radius
        let z = 1
        if (view.camera == pcamera) {
            let r_max = pcamera.z * Math.sin(pcamera.fieldOfView * Math.PI / 180 / 2)
            z = r_max / r
            if (pcamera.fieldOfViewOrientation == PerspectiveCamera.Vertical && (width < height))
                z = z * width / height
            else if (pcamera.fieldOfViewOrientation == PerspectiveCamera.Horizontal && (height < width))
                z = z * height / width
        } else {
            let size = (width < height) ? width : height
            z = (r > 0) ? (size / r) : 1
        }

        rootNode.scale = Qt.vector3d(z, z, z)
        rootNode.position = Qt.vector3d(0, 0, 0)
    }

    function animateScaleToFit() {
        modelAnimation.enabled = true
        rootNode.rotation = RenderSettings.defaultRotation
        scaleToFit()
        modelAnimation.enabled = false
    }

    View3D {
        id: view
        anchors.fill: parent

        environment: SceneEnvironment {
            id: env
            clearColor: root.renderController.clearColor
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: (RenderSettings.antiAliasing === RenderSettings.NoAA) ?
                                  SceneEnvironment.NoAA : SceneEnvironment.SSAA
            antialiasingQuality: (RenderSettings.antiAliasing === RenderSettings.MediumAA)
                                 ? SceneEnvironment.Medium
                                 : ((RenderSettings.antiAliasing === RenderSettings.VeryHighAA)
                                    ? SceneEnvironment.VeryHigh
                                    : SceneEnvironment.High)
            lightProbe: Texture { source: "./textures/lightbox.ktx" }
            probeExposure: 0.9
            aoStrength: RenderSettings.aoStrength * 100
            aoDistance: RenderSettings.aoDistance * 100
            aoSoftness: RenderSettings.aoSoftness * 100
        }

        DirectionalLight {
           brightness: RenderSettings.additionalLight
        }

        camera: RenderSettings.orthographicCamera ? ocamera : pcamera

        OrthographicCamera {
            id: ocamera
            z: 3000
        }

        PerspectiveCamera {
            id: pcamera
            z: 3000
            fieldOfView: RenderSettings.fieldOfView
        }

        Node {
            id: rootNode
            rotation: RenderSettings.defaultRotation
            pivot: root.renderController.center

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

            Loader3D {
                asynchronous: true
                active: RenderSettings.showBoundingSpheres
                sourceComponent: Model {
                    source: "#Sphere"
                    position: root.renderController.center
                    property real radius: root.renderController.radius * 2 / 100
                    scale: Qt.vector3d(radius, radius, radius)
                    materials: PrincipledMaterial {
                        baseColor: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
                        opacity: 0.3
                    }
                }
            }

            Loader3D {
                asynchronous: true
                active: RenderSettings.showBoundingSpheres
                sourceComponent: Repeater3D {
                    model: root.renderController.surfaces

                    Model {
                        required property RenderGeometry modelData

                        source: "#Sphere"
                        visible: RenderSettings.showBoundingSpheres
                        position: modelData ? modelData.center : Qt.vector3d(0, 0, 0)
                        property real radius: modelData ? (modelData.radius * 2 / 100) : 1
                        scale: Qt.vector3d(radius, radius, radius)
                        materials: PrincipledMaterial {
                            baseColor: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
                            opacity: 0.3
                        }
                    }
                }
            }

            Repeater3D {
                model: root.renderController.surfaces

                Model {
                    id: model
                    required property RenderGeometry modelData

                    geometry: modelData
                    materials: PrincipledMaterial {
                        id: material
                        property color color       : model.modelData ? model.modelData.color : "pink"
                        property real luminance    : model.modelData ? model.modelData.luminance : 0
                        property bool isChrome     : model.modelData && model.modelData.isChrome
                        property bool isMetallic   : model.modelData && model.modelData.isMetallic
                        property bool isPearl      : model.modelData && model.modelData.isPearl
                        property bool isTransparent: (color.a < 1)
                        property var textureData   : model.modelData ? model.modelData.textureData : null

                        property var texture: Texture { textureData: material.textureData }

                        lighting: RenderSettings.lighting ? PrincipledMaterial.FragmentLighting : PrincipledMaterial.NoLighting

                        baseColorMap: textureData ? texture : null
                        baseColor   : textureData ? "white" : color

                        cullMode     : isTransparent ? Material.NoCulling          : Material.BackFaceCulling
                        depthDrawMode: isTransparent ? Material.NeverDepthDraw     : Material.AlwaysDepthDraw
                        alphaMode    : isTransparent ? PrincipledMaterial.Blend    : PrincipledMaterial.Opaque
                        //blendMode    : isTransparent ? PrincipledMaterial.Multiply : PrincipledMaterial.SourceOver

                        emissiveFactor: Qt.vector3d(luminance, luminance, luminance)

                        metalness: isChrome ? RenderSettings.chromeMetalness
                                            : isMetallic ? RenderSettings.metallicMetalness
                                                         : isPearl ? RenderSettings.pearlMetalness
                                                                   : RenderSettings.plainMetalness
                        roughness: isChrome ? RenderSettings.chromeRoughness
                                            : isMetallic ? RenderSettings.metallicRoughness
                                                         : isPearl ? RenderSettings.pearlRoughness
                                                                   : RenderSettings.plainRoughness
                    }
                }
            }

            Model {
                id: lines
                geometry: root.renderController.lineGeometry
                instancing: RenderSettings.renderLines ? root.renderController.lines : null
                visible: RenderSettings.renderLines
                depthBias: -10

                materials: CustomMaterial {
                    property real customLineWidth: RenderSettings.lineThickness * rootNode.scale.x / 50
                    property size resolution: Qt.size(view.width, view.height)

                    cullMode: Material.BackFaceCulling
                    shadingMode: CustomMaterial.Unshaded
                    vertexShader: "./shaders/custom-line.vert"
                    fragmentShader: "./shaders/custom-line.frag"
                }
            }
        }

        Timer {
            id: animation
            running: root.renderController.tumblingAnimationActive
            repeat: true
            interval: 16
            onTriggered: {
                rootNode.rotate(RenderSettings.tumblingAnimationAngle,
                                RenderSettings.tumblingAnimationAxis, Node.LocalSpace)
            }
        }

        HoverHandler {
            id: hovered
            cursorShape: Qt.SizeAllCursor
        }
        Timer {
            interval: 2000
            running: hovered.hovered && !moveHandler.active && !pinchHandler.active && !arcballHandler.active
            onTriggered: root.renderController.requestToolTip(hovered.point.scenePosition)
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onSingleTapped: (eventPoint, button) => {
                                root.renderController.requestContextMenu(eventPoint.scenePosition)
                            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onDoubleTapped: root.animateScaleToFit()
        }

        WheelHandler {
            target: null
            onWheel: (event) => {
                         let d = 1.0 + (event.angleDelta.y / 1200.0)
                         rootNode.scale = rootNode.scale.times(d)
                     }
        }

        DragHandler {
            id: moveHandler

            property vector3d pressPosition
            property bool animationWasActive: false

            acceptedButtons: Qt.RightButton
            enabled: !arcballHandler.active && !pinchHandler.active

            target: null

            onActiveChanged: {
                if (active) {
                    animationWasActive = root.renderController.tumblingAnimationActive
                    root.renderController.tumblingAnimationActive = false
                    pressPosition = rootNode.position
                } else {
                    root.renderController.tumblingAnimationActive = animationWasActive
                }
            }
            onActiveTranslationChanged: {
                if (!active)
                    return
                //TODO: this is not correct for a perspective camera
                let offset = Qt.vector3d(2 * activeTranslation.x, -2 * activeTranslation.y, 0)
                rootNode.position = pressPosition.plus(offset)
            }
        }
        PinchHandler {
            id: pinchHandler

            property vector3d scaleStart

            target: null
            enabled: !moveHandler.active && !arcballHandler.active

            onActiveChanged: {
                if (active)
                    scaleStart = rootNode.scale
            }

            onActiveScaleChanged: {
                if (active)
                    rootNode.scale = scaleStart.times(activeScale)
            }
        }

        DragHandler {
            id: arcballHandler

            property quaternion pressRotation
            property point pressPos
            property bool animationWasActive: false

            acceptedButtons: Qt.LeftButton
            enabled: !moveHandler.active && !pinchHandler.active

            target: null
            cursorShape: Qt.ClosedHandCursor

            onActiveChanged: {
                if (active) {
                    animationWasActive = root.renderController.tumblingAnimationActive
                    root.renderController.tumblingAnimationActive = false
                    pressRotation = rootNode.rotation
                    pressPos = centroid.pressPosition
                } else {
                    root.renderController.tumblingAnimationActive = animationWasActive
                }
            }
            onActiveTranslationChanged: {
                if (!active)
                    return
                let mousePos = Qt.point(pressPos.x + activeTranslation.x,
                                        pressPos.y + activeTranslation.y)

                rootNode.rotation = root.renderController.rotateArcBall(pressPos, mousePos,
                                                                        pressRotation,
                                                                        Qt.size(view.width, view.height))
            }
        }
    }
}
