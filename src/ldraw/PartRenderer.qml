import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import BrickStore as BS

Item {
    id: root

    required property var renderController
    property alias rc: root.renderController
    property var rs: BS.LDrawRenderSettings

    Connections {
        target: rc
        function onQmlResetCamera() {
            root.animateScaleToFit()
        }
        function onPartOrColorChanged() {
            root.scaleToFit()
        }
    }

    implicitWidth: 200
    implicitHeight: 200

    onWidthChanged: scaleToFit()
    onHeightChanged: scaleToFit()

    function scaleToFit() {
        if (!rc)
            return

        let r = rc.radius
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
        rootNode.rotation = rs.defaultRotation
        scaleToFit()
        modelAnimation.enabled = false
    }

    View3D {
        id: view
        anchors.fill: parent

        renderMode: View3D.Offscreen

        environment: SceneEnvironment {
            id: env
            clearColor: rc.clearColor
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.SSAA
            antialiasingQuality: SceneEnvironment.VeryHigh
            lightProbe: Texture { source: "qrc:/ldraw/lightbox.hdr" }
            probeExposure: rs.lightProbeExposure
            tonemapMode: SceneEnvironment.TonemapModeLinear
            aoStrength: rs.aoStrength * 100
            aoDistance: rs.aoDistance * 100
            aoSoftness: rs.aoSoftness * 100
        }
        camera: rs.orthographicCamera ? ocamera : pcamera

        OrthographicCamera {
            id: ocamera
            z: 3000
        }

        PerspectiveCamera {
            id: pcamera
            z: 3000
            fieldOfView: rs.fieldOfView
        }

        Node {
            id: rootNode
            rotation: rs.defaultRotation
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
                visible: rs.showBoundingSpheres
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
                    visible: rs.showBoundingSpheres
                    position: ldrawGeometry ? ldrawGeometry.center : Qt.vector3d(0, 0, 0)
                    property real radius: ldrawGeometry ? (ldrawGeometry.radius * 2 / 100) : 1
                    scale: Qt.vector3d(radius, radius, radius)
                    materials: PrincipledMaterial {
                        baseColor: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
                        opacity: 0.3
                    }
                }
            }

            // Workaround for QtQuick3D bug:
            //  custom textures are unloaded from the GPU when no model is referencing them anymore,
            //  but they are not re-uploaded when a new model references them again in the future.
            property var textureKeepAlive: ({})

            Repeater3D {
                model: rc.surfaces.length

                Model {
                    id: model
                    property var ldrawGeometry: rc.surfaces[index]

                    geometry: ldrawGeometry
                    materials: PrincipledMaterial {
                        id: material
                        property color color       : model.ldrawGeometry ? model.ldrawGeometry.color : "pink"
                        property real luminance    : model.ldrawGeometry ? model.ldrawGeometry.luminance : 0
                        property bool isChrome     : model.ldrawGeometry && model.ldrawGeometry.isChrome
                        property bool isMetallic   : model.ldrawGeometry && model.ldrawGeometry.isMetallic
                        property bool isPearl      : model.ldrawGeometry && model.ldrawGeometry.isPearl
                        property bool isTransparent: (color.a < 1)
                        property var textureData   : model.ldrawGeometry ? model.ldrawGeometry.textureData : null

                        onTextureDataChanged: {
                            if (textureData) {
                                if (rootNode.textureKeepAlive[textureData] === undefined) {
                                    let obj = Qt.createQmlObject('import QtQuick3D; Texture { }', rootNode)
                                    obj.textureData = textureData
                                    rootNode.textureKeepAlive[textureData] = true
                                    console.log("Texture keep-alive for color " + color)
                                }
                            }
                        }

                        property var texture: Texture { textureData: material.textureData }

                        lighting: rs.lighting ? PrincipledMaterial.FragmentLighting : PrincipledMaterial.NoLighting

                        baseColorMap: textureData ? texture : null
                        baseColor   : textureData ? "white" : color

                        cullMode     : isTransparent ? Material.NoCulling          : Material.BackFaceCulling
                        depthDrawMode: isTransparent ? Material.NeverDepthDraw     : Material.AlwaysDepthDraw
                        alphaMode    : isTransparent ? PrincipledMaterial.Blend    : PrincipledMaterial.Opaque
                        blendMode    : isTransparent ? PrincipledMaterial.Multiply : PrincipledMaterial.SourceOver

                        emissiveFactor: Qt.vector3d(luminance, luminance, luminance)

                        metalness: isChrome ? rs.chromeMetalness
                                            : isMetallic ? rs.metallicMetalness
                                                         : isPearl ? rs.pearlMetalness
                                                                   : rs.plainMetalness
                        roughness: isChrome ? rs.chromeRoughness
                                            : isMetallic ? rs.metallicRoughness
                                                         : isPearl ? rs.pearlRoughness
                                                                   : rs.plainRoughness
                    }
                }
            }
            Model {
                id: lines
                geometry: rc.lineGeometry
                instancing: rc.lines
                visible: rs.renderLines
                depthBias: -10

                materials: CustomMaterial {
                    property real customLineWidth: rs.lineThickness * rootNode.scale.x / 50
                    property size resolution: Qt.size(view.width, view.height)

                    cullMode: Material.BackFaceCulling
                    shadingMode: CustomMaterial.Unshaded
                    vertexShader: "shaders/custom-line.vert"
                    fragmentShader: "shaders/custom-line.frag"
                }
            }
        }

        Timer {
            id: animation
            running: rc.tumblingAnimationActive
            repeat: true
            interval: 16
            onTriggered: {
                rootNode.rotate(rs.tumblingAnimationAngle,
                                rs.tumblingAnimationAxis, Node.LocalSpace)
            }
        }

        HoverHandler {
            id: hovered
            cursorShape: Qt.SizeAllCursor
        }
        Timer {
            interval: 2000
            running: hovered.hovered
            onTriggered: rc.requestToolTip(hovered.point.scenePosition)
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onSingleTapped: (eventPoint, button) => { rc.requestContextMenu(eventPoint.scenePosition) }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
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
            dragThreshold: 1

            onActiveChanged: {
                if (active) {
                    animationWasActive = rc.tumblingAnimationActive
                    rc.tumblingAnimationActive = false
                    pressPosition = rootNode.position
                } else {
                    rc.tumblingAnimationActive = animationWasActive
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
            dragThreshold: 0
            cursorShape: Qt.ClosedHandCursor

            onActiveChanged: {
                if (active) {
                    animationWasActive = rc.tumblingAnimationActive
                    rc.tumblingAnimationActive = false
                    pressRotation = rootNode.rotation
                    pressPos = centroid.pressPosition
                } else {
                    rc.tumblingAnimationActive = animationWasActive
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
}
