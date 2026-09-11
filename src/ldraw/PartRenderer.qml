// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick
import QtQuick3D
import LDraw

pragma ComponentBehavior: Bound

Item {
    id: root

    property RenderController renderController: RenderController { }

    // overridable defaults: assigning these replaces the binding, so an offscreen
    // render can pick its own values without touching the persisted RenderSettings
    // (see LDraw::renderPartImage). "rotation" is taken by QQuickItem.
    property alias modelRotation: rootNode.rotation
    property bool renderLines: RenderSettings.renderLines
    property int antiAliasing: RenderSettings.antiAliasing

    // gated target: workaround for QTBUG-148459 (crash in connectSignalsToMethods
    // during async incubation); bind the target only after incubation finished
    Connections {
        property bool ready: false
        target: ready ? root.renderController : null
        function onQmlResetCamera() {
            root.animateScaleToFit()
        }
        function onItemOrColorChanged() {
            root.scaleToFit()
        }
        Component.onCompleted: ready = true
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
        let r_max = pcamera.z * Math.sin(pcamera.fieldOfView * Math.PI / 180 / 2)
        z = (r > 0) ? (r_max / r) : 1
        if (pcamera.fieldOfViewOrientation == PerspectiveCamera.Vertical && (width < height))
            z = z * width / height
        else if (pcamera.fieldOfViewOrientation == PerspectiveCamera.Horizontal && (height < width))
            z = z * height / width

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
            antialiasingMode: (root.antiAliasing === RenderSettings.No) ?
                                  SceneEnvironment.NoAA : SceneEnvironment.SSAA
            antialiasingQuality: (root.antiAliasing === RenderSettings.Medium)
                                 ? SceneEnvironment.Medium
                                 : ((root.antiAliasing === RenderSettings.VeryHigh)
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

        camera: PerspectiveCamera {
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
                        property bool isTwoSided   : model.modelData && model.modelData.isTwoSided
                        property bool isTransparent: (color.a < 1)
                        property var textureData   : model.modelData ? model.modelData.textureData : null

                        property var texture: Texture { textureData: material.textureData }

                        lighting: RenderSettings.lighting ? PrincipledMaterial.FragmentLighting : PrincipledMaterial.NoLighting

                        baseColorMap: textureData ? texture : null
                        baseColor   : textureData ? "white" : color

                        cullMode     : (isTransparent || isTwoSided) ? Material.NoCulling : Material.BackFaceCulling
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
                instancing: root.renderLines ? root.renderController.lines : null
                visible: root.renderLines
                depthBias: -10

                materials: CustomMaterial {
                    property real customLineWidth: RenderSettings.lineThickness * rootNode.scale.x / 50
                    property size resolution: Qt.size(view.width, view.height)
                    // vector4d instead of color: color uniforms get an sRGB->linear conversion,
                    // but this has to match the raw sRGB instance colors
                    property vector4d customEdgeColor: {
                        let c = root.renderController.edgeColor
                        return Qt.vector4d(c.r, c.g, c.b, c.a)
                    }

                    cullMode: Material.BackFaceCulling
                    shadingMode: CustomMaterial.Unshaded
                    vertexShader: "./shaders/custom-line.vert"
                    fragmentShader: "./shaders/custom-line.frag"
                }
            }
        }

        FrameAnimation {
            id: animation
            running: root.renderController.tumblingAnimationActive
            onTriggered: {
                // tumblingAnimationAngle is per 60 Hz tick
                rootNode.rotate(RenderSettings.tumblingAnimationAngle * frameTime * 60,
                                RenderSettings.tumblingAnimationAxis, Node.LocalSpace)
            }
        }

        HoverHandler {
            id: hovered
            cursorShape: Qt.SizeAllCursor
        }
        Timer {
            interval: 2000
            running: hovered.hovered && !pinchHandler.active && !arcballHandler.active
            onTriggered: root.renderController.requestToolTip(hovered.point.scenePosition)
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

        PinchHandler {
            id: pinchHandler

            property vector3d scaleStart

            target: null
            enabled: !arcballHandler.active

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
            enabled: !pinchHandler.active

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
