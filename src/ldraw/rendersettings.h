// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
    Q_PROPERTY(QQuaternion defaultRotation MEMBER m_defaultRotation CONSTANT FINAL)
    Q_PROPERTY(float fieldOfView MEMBER m_fieldOfView CONSTANT FINAL)
    Q_PROPERTY(bool showBoundingSpheres MEMBER m_showBoundingSpheres CONSTANT FINAL)
    Q_PROPERTY(float tumblingAnimationAngle MEMBER m_tumblingAnimationAngle CONSTANT FINAL)
    Q_PROPERTY(QVector3D tumblingAnimationAxis MEMBER m_tumblingAnimationAxis CONSTANT FINAL)

    Q_PROPERTY(int antiAliasing MEMBER m_antiAliasing NOTIFY antiAliasingChanged FINAL)
    Q_PROPERTY(bool smoothNormals MEMBER m_smoothNormals NOTIFY smoothNormalsChanged FINAL)
    Q_PROPERTY(bool renderLines MEMBER m_renderLines NOTIFY renderLinesChanged FINAL)
    Q_PROPERTY(float lineThickness MEMBER m_lineThickness NOTIFY lineThicknessChanged FINAL)

    Q_PROPERTY(bool lighting MEMBER m_lighting NOTIFY lightingChanged FINAL)
    Q_PROPERTY(float additionalLight MEMBER m_additionalLight NOTIFY additionalLightChanged FINAL)
    Q_PROPERTY(float aoStrength MEMBER m_aoStrength NOTIFY aoStrengthChanged FINAL)
    Q_PROPERTY(float aoDistance MEMBER m_aoDistance NOTIFY aoDistanceChanged FINAL)
    Q_PROPERTY(float aoSoftness MEMBER m_aoSoftness NOTIFY aoSoftnessChanged FINAL)

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

    enum class AntiAliasing {
        No,
        Medium,
        High,
        VeryHigh,
    };
    Q_ENUM(AntiAliasing)

  public:
    bool smoothNormals() const;
    bool renderLines() const;
    QQuaternion defaultRotation() const;

    void save();
    void load();
    void resetToDefaults();

signals:
    void antiAliasingChanged(int newAntiAliasing);
    void smoothNormalsChanged(bool newSmoothNormals);
    void renderLinesChanged(bool newRenderLines);
    void lineThicknessChanged(float newLineThickness);

    void lightingChanged(bool newLighting);
    void additionalLightChanged(float newAdditionalLight);
    void aoStrengthChanged(float newAoStrength);
    void aoDistanceChanged(float newAoDistance);
    void aoSoftnessChanged(float newAoSoftness);

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
    QVariantMap propertyDefaultValues() const;
    void forEachProperty(const std::function<void (QMetaProperty &)> &callback);

    // fixed values
    QQuaternion m_defaultRotation     = QQuaternion::fromEulerAngles(-24, -138, 160);
    float m_fieldOfView               = 40.f;
    bool m_showBoundingSpheres        = false;
    float m_tumblingAnimationAngle    = 0.1f;
    QVector3D m_tumblingAnimationAxis = { 0.5f, 0.375f, 0.25f };

    // user-configurable values
    int m_antiAliasing { };
    bool m_smoothNormals { };
    bool m_renderLines { };
    float m_lineThickness { };

    bool m_lighting { };
    float m_additionalLight { };
    float m_aoStrength { };
    float m_aoSoftness { };
    float m_aoDistance { };

    float m_plainMetalness { };
    float m_plainRoughness { };
    float m_chromeMetalness { };
    float m_chromeRoughness { };
    float m_metallicMetalness { };
    float m_metallicRoughness { };
    float m_pearlMetalness { };
    float m_pearlRoughness { };
};

} // namespace LDraw
