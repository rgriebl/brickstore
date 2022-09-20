/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once
#include <functional>

#include <QtCore/QObject>
#include <QtGui/QQuaternion>
#include <QtQml/QQmlEngine>


namespace LDraw {

class RenderSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderSettings)
    QML_SINGLETON
    Q_PROPERTY(QQuaternion defaultRotation MEMBER m_defaultRotation NOTIFY defaultRotationChanged FINAL)
    Q_PROPERTY(bool orthographicCamera MEMBER m_orthographicCamera NOTIFY orthographicCameraChanged FINAL)
    Q_PROPERTY(bool lighting MEMBER m_lighting NOTIFY lightingChanged FINAL)
    Q_PROPERTY(bool renderLines MEMBER m_renderLines NOTIFY renderLinesChanged FINAL)
    Q_PROPERTY(float lineThickness MEMBER m_lineThickness NOTIFY lineThicknessChanged FINAL)
    Q_PROPERTY(bool showBoundingSpheres MEMBER m_showBoundingSpheres NOTIFY showBoundingSpheresChanged FINAL)
    Q_PROPERTY(float tumblingAnimationAngle MEMBER m_tumblingAnimationAngle NOTIFY tumblingAnimationAngleChanged FINAL)
    Q_PROPERTY(QVector3D tumblingAnimationAxis MEMBER m_tumblingAnimationAxis NOTIFY tumblingAnimationAxisChanged FINAL)
    Q_PROPERTY(float fieldOfView MEMBER m_fieldOfView NOTIFY fieldOfViewChanged FINAL)
    Q_PROPERTY(int antiAliasing MEMBER m_antiAliasing NOTIFY antiAliasingChanged FINAL)

    Q_PROPERTY(float aoStrength MEMBER m_aoStrength NOTIFY aoStrengthChanged FINAL)
    Q_PROPERTY(float aoDistance MEMBER m_aoDistance NOTIFY aoDistanceChanged FINAL)
    Q_PROPERTY(float aoSoftness MEMBER m_aoSoftness NOTIFY aoSoftnessChanged FINAL)
    Q_PROPERTY(float additionalLight MEMBER m_additionalLight NOTIFY additionalLightChanged FINAL)

    Q_PROPERTY(float plainMetalness MEMBER m_plainMetalness NOTIFY plainMetalnessChanged FINAL)
    Q_PROPERTY(float plainRoughness MEMBER m_plainRoughness NOTIFY plainRoughnessChanged FINAL)
    Q_PROPERTY(float chromeMetalness MEMBER m_chromeMetalness NOTIFY chromeMetalnessChanged FINAL)
    Q_PROPERTY(float chromeRoughness MEMBER m_chromeRoughness NOTIFY chromeRoughnessChanged FINAL)
    Q_PROPERTY(float metallicMetalness MEMBER m_metallicMetalness NOTIFY metallicMetalnessChanged FINAL)
    Q_PROPERTY(float metallicRoughness MEMBER m_metallicRoughness NOTIFY metallicRoughnessChanged FINAL)
    Q_PROPERTY(float pearlMetalness MEMBER m_pearlMetalness NOTIFY pearlMetalnessChanged FINAL)
    Q_PROPERTY(float pearlRoughness MEMBER m_pearlRoughness NOTIFY pearlRoughnessChanged FINAL)

public:
    static RenderSettings *inst();
    static RenderSettings *create(QQmlEngine *qe, QJSEngine *je); // QML_SINGLETON

    enum AntiAliasing {
        NoAA,
        MediumAA,
        HighAA,
        VeryHighAA,
    };
    Q_ENUM(AntiAliasing)

public:
    void save();
    void load();
    void resetToDefaults();

    QVariantMap propertyDefaultValues() const;

signals:
    void defaultRotationChanged(const QQuaternion &newDefaultRotation);
    void orthographicCameraChanged(bool newOrthographicCamera);
    void lightingChanged(bool newLighting);
    void renderLinesChanged(bool newRenderLines);
    void lineThicknessChanged(float newLineThickness);
    void showBoundingSpheresChanged(bool newShowBoundingSpheres);
    void tumblingAnimationAngleChanged(float newTumblingAnimationAngle);
    void tumblingAnimationAxisChanged(const QVector3D &newTumblingAnimationAxis);
    void fieldOfViewChanged(float newFieldOfView);
    void antiAliasingChanged(int newAntiAliasing);

    void aoStrengthChanged(float newAoStrength);
    void aoDistanceChanged(float newAoDistance);
    void aoSoftnessChanged(float newAoSoftness);
    void additionalLightChanged(float newAdditionalLight);

    void plainMetalnessChanged(float newPlainMetalness);
    void plainRoughnessChanged(float newPlainRoughness);
    void chromeMetalnessChanged(float newChromeMetalness);
    void chromeRoughnessChanged(float newChromeRoughness);
    void metallicMetalnessChanged(float newMetallicMetalness);
    void metallicRoughnessChanged(float newMetallicRoughness);
    void pearlMetalnessChanged(float newPearlMetalness);
    void pearlRoughnessChanged(float newPearlRoughness);

private:
    RenderSettings();
    static RenderSettings *s_inst;
    void setToDefault();
    void forEachProperty(std::function<void (QMetaProperty &)> callback);

    QQuaternion m_defaultRotation;
    bool m_orthographicCamera;
    bool m_lighting;
    bool m_renderLines;
    float m_lineThickness;
    bool m_showBoundingSpheres;
    float m_tumblingAnimationAngle;
    QVector3D m_tumblingAnimationAxis;
    float m_fieldOfView;
    int m_antiAliasing;

    float m_aoStrength;
    float m_aoSoftness;
    float m_aoDistance;
    float m_additionalLight;

    float m_plainMetalness;
    float m_plainRoughness;
    float m_chromeMetalness;
    float m_chromeRoughness;
    float m_metallicMetalness;
    float m_metallicRoughness;
    float m_pearlMetalness;
    float m_pearlRoughness;
};

} // namespace LDraw
