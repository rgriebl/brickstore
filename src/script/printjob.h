/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include <QSizeF>

QT_FORWARD_DECLARE_CLASS(QPaintDevice)
QT_FORWARD_DECLARE_CLASS(QPainter)


namespace QmlWrapper {

class PrintJob;

class PrintPage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int number READ pageNumber)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(QColor backgroundColor READ bgColor WRITE setBgColor)
    Q_PROPERTY(int lineStyle READ lineStyle  WRITE setLineStyle)
    Q_PROPERTY(double lineWidth READ lineWidth  WRITE setLineWidth)

public:
    enum LineStyle {
        NoLine,
        SolidLine,
        DashLine,
        DotLine,
        DashDotLine,
        DashDotDotLine
    };
    Q_ENUM(LineStyle)

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
    Q_DECLARE_FLAGS(Alignment, AlignmentFlag)
    Q_FLAG(Alignment)

    Q_INVOKABLE QSizeF textSize(const QString &text);

    Q_INVOKABLE void drawText(double left, double top, double width, double height,
                              QmlWrapper::PrintPage::Alignment align, const QString &text);
    Q_INVOKABLE void drawLine(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void drawRect(double left, double top, double width, double height);
    Q_INVOKABLE void drawEllipse(double left, double top, double width, double height);
    Q_INVOKABLE void drawImage(double left, double top, double width, double height,
                               const QImage &image);

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
    PrintPage(const PrintJob *job);

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
    QVector<Cmd *> m_cmds;
    const PrintJob *m_job;
    AttrCmd m_attr;
};


class PrintJob : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
//  Q_PROPERTY(int paperFormat READ paperFormat)
    Q_PROPERTY(QSizeF paperSize READ paperSize CONSTANT)
    Q_PROPERTY(double scaling READ scaling WRITE setScaling NOTIFY scalingChanged)

public:
    Q_INVOKABLE QmlWrapper::PrintPage *addPage();
    Q_INVOKABLE QmlWrapper::PrintPage *getPage(int i) const;
    Q_INVOKABLE void abort();

    int pageCount() const;
    QSizeF paperSize() const;
    bool isAborted() const;
    double scaling() const;
    void setScaling(double s);

signals:
    void pageCountChanged(int pages);
    void scalingChanged(double s);

public:
    PrintJob(QPaintDevice *pd);
    ~PrintJob() override;

    QPaintDevice *paintDevice() const;

    bool print(int from, int to);
    void dump();

private:
    QVector<PrintPage *> m_pages;
    QPaintDevice *m_pd;
    bool m_aborted = false;
    double m_scaling = 1.;
};

} // namespace QmlWrapper

Q_DECLARE_METATYPE(QmlWrapper::PrintPage *)
Q_DECLARE_METATYPE(QmlWrapper::PrintPage::Alignment)
Q_DECLARE_METATYPE(QmlWrapper::PrintPage::LineStyle)
Q_DECLARE_METATYPE(QmlWrapper::PrintJob *)
