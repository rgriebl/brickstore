// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QFont>
#include <QColor>
#include <QVariant>
#include <QSizeF>
#include <QQmlEngine>

QT_FORWARD_DECLARE_CLASS(QPaintDevice)
QT_FORWARD_DECLARE_CLASS(QPainter)


class QmlPrintJob;

class QmlPrintPage : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PrintPage)
    QML_UNCREATABLE("")
    Q_PROPERTY(int number READ pageNumber CONSTANT)
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
                              QmlPrintPage::Alignment align, const QString &text);
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
    QmlPrintPage(const QmlPrintJob *job);

    void dump();
    void print(QPainter *p, double scaleX, double scaleY) const;

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

    void pushAttrCmd();

private:
    QVector<Cmd *> m_cmds;
    const QmlPrintJob *m_job;
    AttrCmd m_attr;
};


struct QmlPage
{
    Q_GADGET
    QML_FOREIGN(QmlPrintPage)
    QML_NAMED_ELEMENT(Page)
    QML_UNCREATABLE("")
};


class QmlPrintJob : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PrintJob)
    QML_UNCREATABLE("")
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
    Q_PROPERTY(QSizeF paperSize READ paperSize CONSTANT)

public:
    Q_INVOKABLE QmlPrintPage *addPage();
    Q_INVOKABLE QmlPrintPage *getPage(int i) const;
    Q_INVOKABLE void abort();

    int pageCount() const;
    QSizeF paperSize() const;
    bool isAborted() const;

signals:
    void pageCountChanged(int pages);

public:
    QmlPrintJob(QPaintDevice *pd);
    ~QmlPrintJob() override;

    QPaintDevice *paintDevice() const;

    bool print(const QList<uint> &pages);
    void dump();

private:
    QVector<QmlPrintPage *> m_pages;
    QPaintDevice *m_pd;
    bool m_aborted = false;
};
