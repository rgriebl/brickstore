// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDataStream>
#include <QtGui/QMatrix4x4>

#include "common/config.h"
#include "ldraw/rendersettings.h"


namespace LDraw {

RenderSettings *RenderSettings::s_inst = nullptr;

RenderSettings *RenderSettings::inst()
{
    if (!s_inst)
        s_inst = new RenderSettings();
    return s_inst;
}

RenderSettings *RenderSettings::create(QQmlEngine *qe, QJSEngine *)
{
    Q_ASSERT(!s_inst || !qmlEngine(s_inst) || (qmlEngine(s_inst) == qe));
    auto rs = inst();
    QQmlEngine::setObjectOwnership(rs, QQmlEngine::CppOwnership);
    return rs;
}

void RenderSettings::forEachProperty(const std::function<void(QMetaProperty &)> &callback)
{
    const QMetaObject *mo = metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto mp = mo->property(i);
        callback(mp);
    }
}

void RenderSettings::save()
{
    forEachProperty([this](QMetaProperty &mp) {
        Config::inst()->setValue(u"LDraw/RenderSettings/" + QLatin1String(mp.name()), mp.read(this));
    });
}

void RenderSettings::load()
{
    const auto pdv = propertyDefaultValues();

    forEachProperty([this, pdv](QMetaProperty &mp) {
        QString name = QString::fromLatin1(mp.name());
        auto v = Config::inst()->value(u"LDraw/RenderSettings/" + name);
        if (!v.isValid())
            v = pdv.value(name);
        if (v.isValid())
            mp.write(this, v);
    });
}

RenderSettings::RenderSettings()
{
    load();
}

QVariantMap RenderSettings::propertyDefaultValues() const
{
    static const QVariantMap pdv = {
        { u"defaultRotation"_qs, QVariant::fromValue(QQuaternion::fromEulerAngles(-24, -138, 160)) },

        { u"orthographicCamera"_qs, false },
        { u"lighting"_qs, true },
        { u"renderLines"_qs, true },
        { u"lineThickness"_qs, 2 },
        { u"showBoundingSpheres"_qs, false },
        { u"tumblingAnimationAngle"_qs, 0.1 },
        { u"tumblingAnimationAxis"_qs, QVariant::fromValue(QVector3D { 0.5, 0.375, 0.25 }) },
        { u"fieldOfView"_qs, 40. },
        { u"antiAliasing"_qs, int(AntiAliasing::HighAA) },

        { u"aoStrength"_qs, 0.6 },
        { u"aoSoftness"_qs, 0.7 },
        { u"aoDistance"_qs, 0.9 },
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        { u"additionalLight"_qs, 0.4},
#else
        { u"additionalLight"_qs, 0.0 },
#endif

        { u"plainMetalness"_qs, 0 },
        { u"plainRoughness"_qs, 0.5 },
        { u"chromeMetalness"_qs, 1 },
        { u"chromeRoughness"_qs, 0.15 },
        { u"metallicMetalness"_qs, 1 },
        { u"metallicRoughness"_qs, 0.45 },
        { u"pearlMetalness"_qs, 0.5 },
        { u"pearlRoughness"_qs, 0.25 },
    };
    return pdv;
}

void RenderSettings::resetToDefaults()
{
    const auto pdv = propertyDefaultValues();

    forEachProperty([this, pdv](QMetaProperty &mp) {
        QString name = QString::fromLatin1(mp.name());
        auto v = pdv.value(name);
        if (v.isValid())
            mp.write(this, v);
        Config::inst()->remove(u"LDraw/RenderSettings/" + name);
    });
}

} // namespace LDraw

#include "moc_rendersettings.cpp"
