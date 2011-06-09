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

class QPaintDevice;
class QPixmap;
class CReportPage;
class QPainter;
class QScriptEngine;


class ReportUtility : public QObject {
    Q_OBJECT
    Q_OVERRIDE(QString objectName         SCRIPTABLE false)

public slots:
    QString translate(const QString &context, const QString &text) const;

    QString localDateString(const QDateTime &dt) const;
    QString localTimeString(const QDateTime &dt) const;

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
    Q_PROPERTY(QSize   paperSize    READ paperSize)
    Q_PROPERTY(double  scaling      READ scaling WRITE setScaling)

public slots:
    ReportPage *addPage();
    ReportPage *getPage(uint i) const;
    void abort();

public:
    uint pageCount() const;
    QSize paperSize() const;
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

class ReportPage : public QObject {
    Q_OBJECT
    Q_OVERRIDE(QString objectName       SCRIPTABLE false)

    Q_ENUMS(LineStyle)
    Q_FLAGS(Alignment)

    Q_PROPERTY(int     number           READ pageNumber)
    Q_PROPERTY(QFont   font             READ font       WRITE setFont)
    Q_PROPERTY(QColor  color            READ color      WRITE setColor)
    Q_PROPERTY(QColor  backgroundColor  READ bgColor    WRITE setBgColor)
    Q_PROPERTY(int     lineStyle        READ lineStyle  WRITE setLineStyle)
    Q_PROPERTY(double  lineWidth        READ lineWidth  WRITE setLineWidth)

public:
    enum LineStyle {
        NoLine,
        SolidLine,
        DashLine,
        DotLine,
        DashDotLine,
        DashDotDotLine,
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
    QSize textSize(const QString &text);

    void drawText(double x, double y, const QString &text);
    void drawText(double left, double top, double width, double height, Alignment align, const QString &text);
    void drawLine(double x1, double y1, double x2, double y2);
    void drawRect(double left, double top, double width, double height);
    void drawEllipse(double left, double top, double width, double height);
    void drawPixmap(double left, double top, double width, double height, const QPixmap &pixmap);

public:
    int pageNumber() const;
    QFont font() const;
    QColor color() const;
    QColor bgColor() const;
    int lineStyle() const;
    double lineWidth() const;

    void setFont(const QFont &f);
    void setColor(const QColor &c);
    void setBgColor(const QColor &c);
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
        QFont  m_font;
        QColor m_color;
        QColor m_bgcolor;
        int    m_linestyle;
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

