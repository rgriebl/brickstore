/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

#include <QObject>
#include <QFont>
#include <QColor>
#include <QVariant>
#include <QList>
#include <QSize>
#include <QScriptable>
#include <QScriptValue>
#include <QScriptEngine>

class QPaintDevice;
class QPixmap;
class CReportPage;
class QPainter;
class QScriptEngine;


class ReportUtility : public QObject, public QScriptable
{
    Q_OBJECT

public:
    ReportUtility();

    Q_INVOKABLE QString translate(const QString &context, const QString &text) const;

    Q_INVOKABLE QString localDateString(const QDateTime &dt) const;
    Q_INVOKABLE QString localTimeString(const QDateTime &dt) const;

    Q_INVOKABLE QString localWeightString(double weight) const;

    Q_INVOKABLE QWidget *loadUiFile(const QString &fileName);
};

Q_DECLARE_METATYPE(ReportUtility *)


class ReportMoney : public QObject, public QScriptable
{
    Q_OBJECT
    Q_OVERRIDE(QString objectName SCRIPTABLE false)

public:
    ReportMoney();

    Q_INVOKABLE double fromValue(double d);
    Q_INVOKABLE double fromLocalValue(double d);

    Q_INVOKABLE double value(double d) const;
    Q_INVOKABLE double localValue(double d) const;

    Q_INVOKABLE QString localCurrencySymbol() const;

    Q_INVOKABLE QString toString(double d, bool with_currency_symbol = false, int precision = 3);
    Q_INVOKABLE QString toLocalString(double d, bool with_currency_symbol = false, int precision = 3);
};

Q_DECLARE_METATYPE(ReportMoney *)


class Size : public QSizeF
{
public:
    Size() = default;
    Size(const Size &) = default;
    Size(const QSizeF &sf) : QSizeF(sf) { }

    Size &operator=(const Size &) = default;
    Size &operator=(const QSizeF &sf);

    static QScriptValue toScriptValue(QScriptEngine *engine, const Size &s);
    static void fromScriptValue(const QScriptValue &obj, Size &s);
    static QScriptValue createScriptValue(QScriptContext *, QScriptEngine *engine);
};

Q_DECLARE_METATYPE(Size)


class ReportPage;

class ReportJob : public QObject
{
    Q_OBJECT
    Q_OVERRIDE(QString objectName   SCRIPTABLE false)

    Q_PROPERTY(int    pageCount    READ pageCount NOTIFY pageCountChanged)
// Q_PROPERTY( int     paperFormat  READ paperFormat)
    Q_PROPERTY(Size    paperSize    READ paperSize CONSTANT)
    Q_PROPERTY(double  scaling      READ scaling WRITE setScaling NOTIFY scalingChanged)

public:
    Q_INVOKABLE QObject *addPage();
    Q_INVOKABLE QObject *getPage(int i) const;
    Q_INVOKABLE void abort();

    int pageCount() const;
    Size paperSize() const;
    bool isAborted() const;
    double scaling() const;
    void setScaling(double s);

signals:
    void pageCountChanged(int pages);
    void scalingChanged(double s);

public:
    ReportJob(QPaintDevice *pd);
    ~ReportJob();

    QPaintDevice *paintDevice() const;

    bool print(int from, int to);
    void dump();

private:
    QList<ReportPage *> m_pages;
    QPaintDevice *m_pd;
    bool m_aborted = false;
    double m_scaling = 1.;
};

Q_DECLARE_METATYPE(ReportJob *)


class Font : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString family     READ family     WRITE setFamily     NOTIFY familyChanged)
    Q_PROPERTY(qreal   pointSize  READ pointSize  WRITE setPointSize  NOTIFY pointSizeChanged)
    Q_PROPERTY(int     pixelSize  READ pixelSize  WRITE setPixelSize  NOTIFY pixelSizeChanged)
    Q_PROPERTY(bool    bold       READ bold       WRITE setBold       NOTIFY boldChanged)
    Q_PROPERTY(bool    italic     READ italic     WRITE setItalic     NOTIFY italicChanged)
    Q_PROPERTY(bool    underline  READ underline  WRITE setUnderline  NOTIFY underlineChanged)
    Q_PROPERTY(bool    strikeout  READ strikeout  WRITE setStrikeout  NOTIFY strikeoutChanged)

public:
    Font() = default;
    Font(const QFont &qf);

    QFont toQFont() const;
    void fromQFont(const QFont &qf);

    static QScriptValue toScriptValue(QScriptEngine *engine, Font * const &in);
    static void fromScriptValue(const QScriptValue &object, Font * &out);
    static QScriptValue createScriptValue(QScriptContext *, QScriptEngine *engine);

private:
    QString family() const;
    qreal pointSize() const;
    int pixelSize() const;
    bool bold() const;
    bool italic() const;
    bool underline() const;
    bool strikeout() const;

    void setFamily(const QString &f);
    void setPointSize(qreal ps);
    void setPixelSize(int ps);
    void setBold(bool b);
    void setItalic(bool i);
    void setUnderline(bool u);
    void setStrikeout(bool s);

signals:
    void familyChanged(const QString &family);
    void pointSizeChanged(qreal ps);
    void pixelSizeChanged(int ps);
    void boldChanged(bool b);
    void italicChanged(bool i);
    void underlineChanged(bool u);
    void strikeoutChanged(bool s);

private:
    QFont d;
};

Q_DECLARE_METATYPE(Font *)


class Color : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int     red         READ red         WRITE setRed)
    Q_PROPERTY(int     green       READ green       WRITE setGreen)
    Q_PROPERTY(int     blue        READ blue        WRITE setBlue)
    Q_PROPERTY(QString name        READ name        WRITE setName)
    Q_PROPERTY(uint    rgb         READ rgb         WRITE setRgb)
    Q_PROPERTY(int     hue         READ hue         WRITE setHue)
    Q_PROPERTY(int     saturation  READ saturation  WRITE setSaturation)
    Q_PROPERTY(int     value       READ value       WRITE setValue)

public:
    Color() = default;
    Color(const QColor &qc);

    QColor toQColor() const;
    void fromQColor(const QColor &qc);

    static QScriptValue toScriptValue(QScriptEngine *engine, Color * const &in);

    static void fromScriptValue(const QScriptValue &object, Color * &out);

    static QScriptValue createScriptValue(QScriptContext *, QScriptEngine *engine);

    Q_INVOKABLE void setRgb(uint value);
    Q_INVOKABLE void setRgb(int r, int g, int b);
    Q_INVOKABLE Color *light() const;
    Q_INVOKABLE Color *dark() const;

private:
    int red() const        { return d.red(); }
    int green() const      { return d.green(); }
    int blue() const       { return d.blue(); }
    QString name() const   { return d.name(); }
    uint rgb() const       { return d.rgb(); }
    int hue() const        { return d.hue(); }
    int saturation() const { return d.saturation(); }
    int value() const      { return d.value(); }

    void setRed(int r)        { d.setRed(r); }
    void setGreen(int g)      { d.setGreen(g); }
    void setBlue(int b)       { d.setBlue(b); }
    void setName(const QString &name) { d.setNamedColor(name); }
    void setHue(int h)        { d.setHsv(h, d.saturation(), d.value()); }
    void setSaturation(int s) { d.setHsv(d.hue(), s, d.value()); }
    void setValue(int v)      { d.setHsv(d.hue(), d.saturation(), v); }

private:
    QColor d;
};

Q_DECLARE_METATYPE(Color *)


class ReportPage : public QObject, public QScriptable
{
    Q_OBJECT
    Q_OVERRIDE(QString objectName       SCRIPTABLE false)

    Q_PROPERTY(int     number           READ pageNumber)
    Q_PROPERTY(Font *  font             READ font       WRITE setFont)
    Q_PROPERTY(Color * color            READ color      WRITE setColor)
    Q_PROPERTY(Color * backgroundColor  READ bgColor    WRITE setBgColor)
    Q_PROPERTY(int     lineStyle        READ lineStyle  WRITE setLineStyle)
    Q_PROPERTY(double  lineWidth        READ lineWidth  WRITE setLineWidth)

public:
    enum LineStyle {
        NoLine,
        SolidLine,
        DashLine,
        DotLine,
        DashDotLine,
        DashDotDotLine
    };

    enum AlignmentFlag {
        AlignLeft     = Qt::AlignLeft,
        AlignHCenter  = Qt::AlignHCenter,
        AlignRight    = Qt::AlignRight,
        AlignTop      = Qt::AlignTop,
        AlignVCenter  = Qt::AlignVCenter,
        AlignBottom   = Qt::AlignBottom,
        AlignCenter   = Qt::AlignCenter,
        TextDontClip  = Qt::TextDontClip,
        TextWordWrap  = Qt::TextWordWrap,
        TextWrapAnywhere = Qt::TextWrapAnywhere
    };
    Q_ENUM(LineStyle)
    Q_DECLARE_FLAGS(Alignment, AlignmentFlag)
    Q_FLAG(Alignment)

    Q_INVOKABLE Size textSize(const QString &text);

    Q_INVOKABLE void drawText(double x, double y, const QString &text);
    Q_INVOKABLE void drawText(double left, double top, double width, double height, ReportPage::Alignment align, const QString &text);
    Q_INVOKABLE void drawLine(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void drawRect(double left, double top, double width, double height);
    Q_INVOKABLE void drawEllipse(double left, double top, double width, double height);
    Q_INVOKABLE void drawImage(double left, double top, double width, double height, const QImage &image);

    // 1.1 compatibility
    Q_INVOKABLE void drawPixmap(double left, double top, double width, double height, const QImage &image)
    { drawImage(left, top, width, height, image); }

public:
    int pageNumber() const;
    Font *font() const;
    Color *color() const;
    Color *bgColor() const;
    int lineStyle() const;
    double lineWidth() const;

    void setFont(Font *f);
    void setColor(Color *c);
    void setBgColor(Color *c);
    void setLineStyle(int linestyle);
    void setLineWidth(double linewidth);

public:
    ReportPage(const ReportJob *job);

    void dump();
    void print(QPainter *p, double scale [2]) const;

private:
    struct Cmd {
        enum {
            Attributes,
            Text,
            Line,
            Rect,
            Ellipse,
            Image
        } m_cmd;
    };

    struct AttrCmd : public Cmd {
        QFont   m_font;
        QColor  m_color;
        QColor  m_bgcolor;
        int     m_linestyle;
        double  m_linewidth;
    };

    struct DrawCmd : public Cmd {
        double    m_x, m_y, m_w, m_h;
        QVariant  m_p1;
        QVariant  m_p2;
    };

    void attr_cmd();

private:
    QList<Cmd *> m_cmds;
    const ReportJob *m_job;
    AttrCmd m_attr;
};

Q_DECLARE_METATYPE(ReportPage *)
