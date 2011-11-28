/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __REPORTOBJECTS_H__
#define __REPORTOBJECTS_H__

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


class ReportUtility : public QObject, public QScriptable {
    Q_OBJECT

public slots:
    QString translate(const QString &context, const QString &text) const;

    QString localDateString(const QDateTime &dt) const;
    QString localTimeString(const QDateTime &dt) const;

    QWidget *loadUiFile(const QString &fileName);

public:
    ReportUtility();
};


class ReportMoneyStatic : public QObject {
    Q_OBJECT
    Q_OVERRIDE(QString objectName SCRIPTABLE false)

public:
    ReportMoneyStatic(QScriptEngine *eng);

public slots:
    double fromValue(double d);
    double fromLocalValue(double d);

    double value(double d) const;
    double localValue(double d) const;

    QString localCurrencySymbol() const;

    QString toString(double d, bool with_currency_symbol = false, int precision = 3);
    QString toLocalString(double d, bool with_currency_symbol = false, int precision = 3);

private:
    QScriptEngine *m_engine;
};


class ReportPage;

class ReportJob : public QObject {
    Q_OBJECT
    Q_OVERRIDE(QString objectName   SCRIPTABLE false)

    Q_PROPERTY(uint    pageCount    READ pageCount)
// Q_PROPERTY( int     paperFormat  READ paperFormat)
    Q_PROPERTY(Size    paperSize    READ paperSize)
    Q_PROPERTY(double  scaling      READ scaling WRITE setScaling)

public slots:
    QObject *addPage();
    QObject *getPage(uint i) const;
    void abort();

public:
    uint pageCount() const;
    Size paperSize() const;
    bool isAborted() const;
    double scaling() const;
    void setScaling(double s);

public:
    ReportJob(QPaintDevice *pd);
    ~ReportJob();

    QPaintDevice *paintDevice() const;

    bool print(uint from, uint to);
    void dump();

private:
    QList<ReportPage *> m_pages;
    QPaintDevice *m_pd;
    bool m_aborted;
    double m_scaling;
};

class Font : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString family     READ family     WRITE setFamily)
    Q_PROPERTY(qreal   pointSize  READ pointSize  WRITE setPointSize)
    Q_PROPERTY(int     pixelSize  READ pixelSize  WRITE setPixelSize)
    Q_PROPERTY(bool    bold       READ bold       WRITE setBold)
    Q_PROPERTY(bool    italic     READ italic     WRITE setItalic)
    Q_PROPERTY(bool    underline  READ underline  WRITE setUnderline)
    Q_PROPERTY(bool    strikeout  READ strikeout  WRITE setStrikeout)

public:
    Font() { }
    Font(const QFont &qf) : d(qf) { }

    QFont toQFont() const { return d; }
    void fromQFont(const QFont &qf) { d = qf; }

    static QScriptValue toScriptValue(QScriptEngine *engine, Font * const &in)
    {
        return engine->newQObject(in);
    }

    static void fromScriptValue(const QScriptValue &object, Font * &out)
    {
        out = qobject_cast<Font *>(object.toQObject());
    }

    static QScriptValue createScriptValue(QScriptContext *, QScriptEngine *engine)
    {
        Font *f = new Font();
        return engine->newQObject(f);
    }

private:
    QString family() const  { return d.family(); }
    qreal pointSize() const { return d.pointSizeF(); }
    int pixelSize() const   { return d.pixelSize(); }
    bool bold() const       { return d.bold(); }
    bool italic() const     { return d.italic(); }
    bool underline() const  { return d.underline(); }
    bool strikeout() const  { return d.strikeOut(); }

    void setFamily(const QString &f) { d.setFamily(f); }
    void setPointSize(qreal ps) { d.setPointSizeF(ps); }
    void setPixelSize(int ps) { d.setPixelSize(ps); }
    void setBold(bool b) { d.setBold(b); }
    void setItalic(bool i) { d.setItalic(i); }
    void setUnderline(bool u) { d.setUnderline(u); }
    void setStrikeout(bool s) { d.setStrikeOut(s); }

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
    Q_PROPERTY(int     rgb         READ rgb         WRITE setRgb)
    Q_PROPERTY(int     hue         READ hue         WRITE setHue)
    Q_PROPERTY(int     saturation  READ saturation  WRITE setSaturation)
    Q_PROPERTY(int     value       READ value       WRITE setValue)

public:
    Color() { }
    Color(const QColor &qc) : d(qc) { }

    QColor toQColor() const { return d; }
    void fromQColor(const QColor &qc) { d = qc; }

    static QScriptValue toScriptValue(QScriptEngine *engine, Color * const &in)
    { return engine->newQObject(in); }

    static void fromScriptValue(const QScriptValue &object, Color * &out)
    { out = qobject_cast<Color *>(object.toQObject()); }

    static QScriptValue createScriptValue(QScriptContext *, QScriptEngine *engine)
    {
        Color *c = new Color();
        return engine->newQObject(c); // ###ScriptOwnership ???
    }

public slots:
    void setRgb(int value) { d.setRgb(value); }
    void setRgb(int r, int g, int b) { d.setRgb(r, g, b); }
    Color *light() const { return new Color(d.light()); }
    Color *dark() const { return new Color(d.dark()); }

private:
    int red() const        { return d.red(); }
    int green() const      { return d.green(); }
    int blue() const       { return d.blue(); }
    QString name() const   { return d.name(); }
    int rgb() const        { return d.rgb(); }
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

class Size : public QSizeF
{
public:
    static QScriptValue toScriptValue(QScriptEngine *engine, const Size &s)
    {
        QScriptValue obj = engine->newObject();
        obj.setProperty("width", s.width());
        obj.setProperty("height", s.height());
        return obj;
    }

    static void fromScriptValue(const QScriptValue &obj, Size &s)
    {
        s.setWidth(obj.property("width").toNumber());
        s.setHeight(obj.property("height").toNumber());
    }

    static QScriptValue createScriptValue(QScriptContext *, QScriptEngine *engine)
    {
        return engine->toScriptValue(Size());
    }
};

Q_DECLARE_METATYPE(Size)

class ReportPage : public QObject {
    Q_OBJECT
    Q_OVERRIDE(QString objectName       SCRIPTABLE false)

    Q_ENUMS(LineStyle)
    Q_FLAGS(Alignment)

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
        AlignCenter   = Qt::AlignCenter
    };

    Q_DECLARE_FLAGS(Alignment, AlignmentFlag)

public slots:
    Size textSize(const QString &text);

    void drawText(double x, double y, const QString &text);
    void drawText(double left, double top, double width, double height, Alignment align, const QString &text);
    void drawLine(double x1, double y1, double x2, double y2);
    void drawRect(double left, double top, double width, double height);
    void drawEllipse(double left, double top, double width, double height);
    void drawPixmap(double left, double top, double width, double height, const QPixmap &pixmap);

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
    void print(QPainter *p, double scale [2]);

private:
    struct Cmd {
        enum {
            Attributes,
            Text,
            Line,
            Rect,
            Ellipse,
            Pixmap
        } m_cmd;
    };

struct AttrCmd : public Cmd {
        Font  * m_font;
        Color * m_color;
        Color * m_bgcolor;
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

#endif

