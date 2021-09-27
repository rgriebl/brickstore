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
#include <QPaintDevice>
#include <QPainter>
#include <QPrinter>
#include <QPaintEngine>
#include <QDebug>
#include <QStringBuilder>

#include "utility/utility.h"
#include "printjob.h"


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

QmlPrintPage::QmlPrintPage(const QmlPrintJob *job)
    : QObject(const_cast <QmlPrintJob *>(job)), m_job(job)
{
    setObjectName(u"Page" % QString::number(job->pageCount() + 1));

    m_attr.m_font = QFont("Arial"_l1, 10);
    m_attr.m_color = QColor(Qt::black);
    m_attr.m_bgcolor = QColor(Qt::white);
    m_attr.m_linewidth = 0.1;
    m_attr.m_linestyle = SolidLine;
}

int QmlPrintPage::pageNumber() const
{
    for (int i = 0; i < m_job->pageCount(); i++) {
        if (m_job->getPage(i) == this)
            return i;
    }
    return -1;
}

void QmlPrintPage::dump()
{
    qDebug(" # of commands: %d", int(m_cmds.count()));
    qDebug(" ");

    for (int i = 0; i < m_cmds.count(); ++i) {
        switch (m_cmds.at(i)->m_cmd) {
        case Cmd::Attributes: {
            auto *ac = static_cast<AttrCmd *>(m_cmds.at(i));

            qDebug(" [%d] Attributes (Font: %s | Color: %s | BgColor: %s | Line: %f | LineStyle: %d", i, qPrintable(ac->m_font.toString()), qPrintable(ac->m_color.name()), qPrintable(ac->m_bgcolor.name()), ac->m_linewidth, ac->m_linestyle);
            break;
        }
        case Cmd::Text:  {
            auto *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            if (qFuzzyCompare(dc->m_w, -1) && qFuzzyCompare(dc->m_h, -1))
                qDebug(" [%d] Text (%f,%f), \"%s\"", i, dc->m_x, dc->m_y, qPrintable(dc->m_p2.toString()));
            else
                qDebug(" [%d] Text (%f,%f - %fx%f), align: %d, \"%s\"", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h, dc->m_p1.toInt(), qPrintable(dc->m_p2.toString()));

            break;
        }
        case Cmd::Line: {
            auto *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Line (%f,%f - %f,%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        case Cmd::Rect: {
            auto *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Rectangle (%f,%f - %fx%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        case Cmd::Ellipse: {
            auto *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Ellipse (%f,%f - %fx%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        case Cmd::Image: {
            auto *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Image (%f,%f - %fx%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        }
    }
}

void QmlPrintPage::print(QPainter *p, double scale [2]) const
{
    for (const Cmd *c : m_cmds) {
        if (c->m_cmd == Cmd::Attributes) {
            const auto *ac = static_cast<const AttrCmd *>(c);

            // QFont f = ac->m_font;
            // f.setPointSizeFloat ( f.pointSizeFloat ( ) * scale [1] );
            p->setFont(ac->m_font);

            if (ac->m_color.isValid())
                p->setPen(QPen(ac->m_color, int(ac->m_linewidth * (scale [0] + scale [1]) / 2.), Qt::PenStyle(ac->m_linestyle)));
            else
                p->setPen(QPen(Qt::NoPen));

            if (ac->m_bgcolor.isValid())
                p->setBrush(QBrush(ac->m_bgcolor));
            else
                p->setBrush(QBrush(Qt::NoBrush));
        }
        else {
            const auto *dc = static_cast<const DrawCmd *>(c);

            int x = int(dc->m_x * scale [0]);
            int y = int(dc->m_y * scale [1]);
            int w = int(dc->m_w * scale [0]);
            int h = int(dc->m_h * scale [1]);

            switch (c->m_cmd) {
            case Cmd::Text:
                if (w < 0 && h < 0)
                    p->drawText(x, y, dc->m_p2.toString());
                else
                    p->drawText(x, y, w, h, dc->m_p1.toInt(), dc->m_p2.toString());
                break;

            case Cmd::Line:
                p->drawLine(x, y, w, h);
                break;

            case Cmd::Rect:
                p->drawRect(x, y, w, h);
                break;

            case Cmd::Ellipse:
                p->drawEllipse(x, y, w, h);
                break;

            case Cmd::Image: {
                QImage img = dc->m_p1.value<QImage>();

                if (!img.isNull()) {
                    QRect dr = QRect(x, y, w, h);

                    QSize oldsize = dr.size();
                    QSize newsize = img.size();
                    newsize.scale(oldsize, Qt::KeepAspectRatio);

                    dr.setSize(newsize);
                    dr.translate((oldsize.width() - newsize.width()) / 2,
                                 (oldsize.height() - newsize.height()) / 2);

                    p->drawImage(dr, img);
                }
                break;
            }

            case Cmd::Attributes:
                break;
            }
        }
    }
}


QFont QmlPrintPage::font() const
{
    return m_attr.m_font;
}

QColor QmlPrintPage::color() const
{
    return m_attr.m_color;
}

QColor QmlPrintPage::bgColor() const
{
    return m_attr.m_bgcolor;
}

int QmlPrintPage::lineStyle() const
{
    return m_attr.m_linestyle;
}

double QmlPrintPage::lineWidth() const
{
    return m_attr.m_linewidth;
}

void QmlPrintPage::attr_cmd()
{
    auto *ac = new AttrCmd();
    *ac = m_attr;
    ac->m_cmd = Cmd::Attributes;
    m_cmds.append(ac);
}

void QmlPrintPage::setFont(const QFont &font)
{
    m_attr.m_font = font;
    attr_cmd();
}

void QmlPrintPage::setColor(const QColor &color)
{
    m_attr.m_color = color;
    attr_cmd();
}

void QmlPrintPage::setBgColor(const QColor &color)
{
    m_attr.m_bgcolor = color;
    attr_cmd();
}

void QmlPrintPage::setLineStyle(int linestyle)
{
    m_attr.m_linestyle = linestyle;
    attr_cmd();
}

void QmlPrintPage::setLineWidth(double linewidth)
{
    m_attr.m_linewidth = linewidth;
    attr_cmd();
}


QSizeF QmlPrintPage::textSize(const QString &text)
{
    QFontMetrics fm(m_attr.m_font);
    QPaintDevice *pd = m_job->paintDevice();
    QSize s = fm.size(0, text);
    return QSizeF(s.width() * pd->widthMM() / pd->width(),
                  s.height() * pd->heightMM() / pd->height());
}

void QmlPrintPage::drawText(double left, double top, double width, double height, Alignment align,
                         const QString &text)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Text;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    dc->m_p1 = int(align);
    dc->m_p2 = text;
    m_cmds.append(dc);
}

void QmlPrintPage::drawLine(double x1, double y1, double x2, double y2)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Line;
    dc->m_x = x1;
    dc->m_y = y1;
    dc->m_w = x2;
    dc->m_h = y2;
    m_cmds.append(dc);
}

void QmlPrintPage::drawRect(double left, double top, double width, double height)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Rect;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    m_cmds.append(dc);
}

void QmlPrintPage::drawEllipse(double left, double top, double width, double height)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Ellipse;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    m_cmds.append(dc);
}

void QmlPrintPage::drawImage(double left, double top, double width, double height, const QImage &image)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Image;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    dc->m_p1 = image;
    m_cmds.append(dc);
}



///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////



QmlPrintJob::QmlPrintJob(QPaintDevice *pd)
    : QObject(nullptr)
    , m_pd(pd)
{
    setObjectName("Job"_l1);
}

QmlPrintJob::~QmlPrintJob()
{
    qDeleteAll(m_pages);
}

QPaintDevice *QmlPrintJob::paintDevice() const
{
    return m_pd;
}

QmlPrintPage *QmlPrintJob::addPage()
{
    auto *page = new QmlPrintPage(this);
    m_pages.append(page);
    int pageNo = m_pages.size();
    page->setObjectName("Print page #"_l1 + QString::number(pageNo));
    emit pageCountChanged(pageNo);
    return page;
}

QmlPrintPage *QmlPrintJob::getPage(int i) const
{
    return m_pages.value(i);
}

void QmlPrintJob::abort()
{
    m_aborted = true;
}

bool QmlPrintJob::isAborted() const
{
    return m_aborted;
}

int QmlPrintJob::pageCount() const
{
    return m_pages.count();
}

QSizeF QmlPrintJob::paperSize() const
{
    return QSizeF(m_pd->widthMM(), m_pd->heightMM());
}

void QmlPrintJob::dump()
{
    qDebug("Print Job Dump");
    qDebug(" # of pages: %d", int(m_pages.count()));
    qDebug(" ");

    for (int i = 0; i < m_pages.count(); ++i) {
        qDebug("Page #%d", i);
        m_pages.at(i)->dump();
    }
}

bool QmlPrintJob::print(int from, int to)
{
    if (m_pages.isEmpty() || (from < 0) || (from > to) || (int(to) >= m_pages.count()))
        return false;

    QPainter p;
    if (!p.begin(m_pd))
        return false;

    QPrinter *prt = (m_pd->devType() == QInternal::Printer) ? static_cast<QPrinter *>(m_pd) : nullptr;

#if defined(Q_OS_WIN) // workaround for QTBUG-5363
    if (prt->fullPage() && !prt->printerName().isEmpty()
            && (p.paintEngine()->type() != QPaintEngine::Picture)) { // printing to a real printer
        auto margins = prt->pageLayout().marginsPixels(prt->resolution());
        p.translate(-margins.left(), -margins.top());
    }
#endif

    double scaling [2];
    scaling [0] = double(m_pd->logicalDpiX()) / 25.4;
    scaling [1] = double(m_pd->logicalDpiY()) / 25.4;
    bool no_new_page = true;

    for (int i = from; i <= to; i++) {
        QmlPrintPage *page = m_pages.at(i);

        if (!no_new_page && prt)
            prt->newPage();

        page->print(&p, scaling);
        no_new_page = false;
    }
    return true;
}

#include "moc_printjob.cpp"

