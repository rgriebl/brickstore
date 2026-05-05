// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QImage>

#include "bricklink/global.h"
#include "utility/pooledarray.h"


namespace BrickLink {

class Color
{
public:
    static constexpr uint InvalidId = static_cast<uint>(-1);

    uint id() const            { return m_id; }
    QString name() const       { return m_name.asQString(); }
    QColor color() const       { return m_color; }

    ColorType type() const     { return m_type; }

    bool isSolid() const       { return m_type & ColorTypeFlag::Solid; }
    bool isTransparent() const { return m_type & ColorTypeFlag::Transparent; }
    bool isGlitter() const     { return m_type & ColorTypeFlag::Glitter; }
    bool isSpeckle() const     { return m_type & ColorTypeFlag::Speckle; }
    bool isMetallic() const    { return m_type & ColorTypeFlag::Metallic; }
    bool isChrome() const      { return m_type & ColorTypeFlag::Chrome; }
    bool isPearl() const       { return m_type & ColorTypeFlag::Pearl; }
    bool isMilky() const       { return m_type & ColorTypeFlag::Milky; }
    bool isModulex() const     { return m_type & ColorTypeFlag::Modulex; }
    bool isSatin() const       { return m_type & ColorTypeFlag::Satin; }

    float luminance() const    { return m_luminance; }

    int ldrawId() const              { return m_ldraw_id; }
    QColor ldrawColor() const        { return m_ldraw_color; }
    QColor ldrawEdgeColor() const    { return m_ldraw_edge_color; }

    bool hasParticles() const        { return !qFuzzyIsNull(m_particleMinSize) && !qFuzzyIsNull(m_particleMaxSize); }
    double particleMinSize() const   { return double(m_particleMinSize); }
    double particleMaxSize() const   { return double(m_particleMaxSize); }
    double particleFraction() const  { return double(m_particleFraction); }
    double particleVFraction() const { return double(m_particleVFraction); }
    QColor particleColor() const     { return m_particleColor; }
    float popularity() const         { return m_popularity < 0 ? 0 : m_popularity; }

    int yearReleased() const         { return m_year_from; }
    int yearLastProduced() const     { return m_year_to; }

    static const QVector<ColorTypeFlag> &allColorTypes();
    static QString typeName(ColorTypeFlag t);

    uint index() const;   // only for internal use

    Color() = default;
    explicit Color(std::nullptr_t) : Color() { } // for scripting only!

    constexpr std::strong_ordering operator<=>(uint id) const { return m_id <=> id; }
    constexpr std::strong_ordering operator<=>(const Color &other) const { return *this <=> other.m_id; }
    constexpr bool operator==(uint id) const { return (*this <=> id == 0); }

    const QImage sampleImage(int w, int h) const;

private:
    PooledArray<char16_t> m_name;
    uint    m_id = InvalidId;
    int     m_ldraw_id = -1;
    QColor  m_color;
    QColor  m_ldraw_color;
    QColor  m_ldraw_edge_color;
    ColorType m_type = {};
    float   m_popularity = 0;
    quint16 m_year_from = 0;
    quint16 m_year_to = 0;
    float   m_luminance = 0;
    float   m_particleMinSize = 0;
    float   m_particleMaxSize = 0;
    float   m_particleFraction = 0;
    float   m_particleVFraction = 0;
    QColor  m_particleColor;

    static QHash<uint, QImage> s_colorImageCache;

    friend class Core;
    friend class Database;
    friend class TextImport;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(const BrickLink::Color *)
