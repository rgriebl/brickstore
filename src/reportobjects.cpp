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

#include <QApplication>
#include <QPixmap>
#include <QPrinter>
//#include <QPaintDeviceMetrics>
#include <QFontMetrics>
#include <QPainter>
#include <QDateTime>

#include <QtScript>


#include "currency.h"
#include "utility.h"

#include "reportobjects.h"


ReportUtility::ReportUtility()
    : QObject(0)
{
    setObjectName(QLatin1String("Utility"));
}

QString ReportUtility::translate(const QString &context, const QString &text) const
{
    return qApp->translate(context.toLatin1(), text.toLatin1());
}

QString ReportUtility::localDateString(const QDateTime &dt) const
{
    return dt.date().toString(Qt::LocalDate);
}

QString ReportUtility::localTimeString(const QDateTime &dt) const
{
    return dt.time().toString(Qt::LocalDate);
}


ReportJob::ReportJob(QPaintDevice *pd)
    : QObject(0), m_pd(pd), m_aborted(false), m_scaling(1.0f)
{
    setObjectName(QLatin1String("Job"));
}

ReportJob::~ReportJob()
{
    qDeleteAll(m_pages);
}

QPaintDevice *ReportJob::paintDevice() const
{
    return m_pd;
}

ReportPage *ReportJob::addPage()
{
    ReportPage *page = new ReportPage(this);
    m_pages.append(page);
    return page;
}

ReportPage *ReportJob::getPage(uint i) const
{
    if (int(i) < m_pages.count())
        return m_pages [i];
    else
        return 0;
}

void ReportJob::abort()
{
    m_aborted = true;
}

bool ReportJob::isAborted() const
{
    return m_aborted;
}

double ReportJob::scaling() const
{
    return m_scaling;
}

void ReportJob::setScaling(double s)
{
    m_scaling = s;
}

uint ReportJob::pageCount() const
{
    return m_pages.count();
}

QSize ReportJob::paperSize() const
{
    return QSize(m_pd->widthMM(), m_pd->heightMM());
}

void ReportJob::dump()
{
    qDebug("Print Job Dump");
    qDebug(" # of pages: %d", int(m_pages.count()));
    qDebug(" ");

    for (int i = 0; i < m_pages.count(); ++i) {
        qDebug("Page #%d", i);
        m_pages [i]->dump();
    }
}

bool ReportJob::print(uint from, uint to)
{
    if (!m_pages.count() || (from > to) || (int(to) >= m_pages.count()))
        return false;

    QPainter p;
    if (!p.begin(m_pd))
        return false;

    QPrinter *prt = (m_pd->devType() == QInternal::Printer) ? static_cast<QPrinter *>(m_pd) : 0;

    double scaling [2];
    scaling [0] = m_scaling * double(m_pd->logicalDpiX()) / 25.4f;
    scaling [1] = m_scaling * double(m_pd->logicalDpiY()) / 25.4f;
    bool no_new_page = true;

    for (uint i = from; i <= to; i++) {
        ReportPage *page = m_pages [i];

        if (!no_new_page && prt)
            prt->newPage();

        page->print(&p, scaling);
        no_new_page = false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

ReportPage::ReportPage(const ReportJob *job)
        : QObject(const_cast <ReportJob *>(job)), m_job(job)
{
    setObjectName(QString("Page%1").arg(job->pageCount() + 1));

    m_attr.m_font = QFont("Arial", 10);
    m_attr.m_color = Qt::black;
    m_attr.m_bgcolor = Qt::white;
    m_attr.m_linewidth = 0.1;
    m_attr.m_linestyle = SolidLine;
}

int ReportPage::pageNumber() const
{
    int res = -1;

    for (uint i = 0; i < m_job->pageCount(); i++) {
        if (m_job->getPage(i) == this) {
            res = int(i);
            break;
        }
    }
    return res;
}

void ReportPage::dump()
{
    qDebug(" # of commands: %d", int(m_cmds.count()));
    qDebug(" ");

    for (int i = 0; i < m_cmds.count(); ++i) {
        switch (m_cmds.at(i)->m_cmd) {
        case Cmd::Attributes: {
            AttrCmd *ac = static_cast<AttrCmd *>(m_cmds.at(i));

            qDebug(" [%d] Attributes (Font: %s | Color: %s | BgColor: %s | Line: %f | LineStyle: %d", i, qPrintable(ac->m_font.toString()), qPrintable(ac->m_color.name()), qPrintable(ac->m_bgcolor.name()), ac->m_linewidth, ac->m_linestyle);
            break;
        }
        case Cmd::Text:  {
            DrawCmd *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            if (dc->m_w == -1 && dc->m_h == -1)
                qDebug(" [%d] Text (%f,%f), \"%s\"", i, dc->m_x, dc->m_y, qPrintable(dc->m_p2.toString()));
            else
                qDebug(" [%d] Text (%f,%f - %fx%f), align: %d, \"%s\"", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h, dc->m_p1.toInt(), qPrintable(dc->m_p2.toString()));

            break;
        }
        case Cmd::Line: {
            DrawCmd *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Line (%f,%f - %f,%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        case Cmd::Rect: {
            DrawCmd *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Rectangle (%f,%f - %fx%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        case Cmd::Ellipse: {
            DrawCmd *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Ellipse (%f,%f - %fx%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        case Cmd::Pixmap: {
            DrawCmd *dc = static_cast<DrawCmd *>(m_cmds.at(i));

            qDebug(" [%d] Pixmap (%f,%f - %fx%f)", i, dc->m_x, dc->m_y, dc->m_w, dc->m_h);
            break;
        }
        }
    }
}

void ReportPage::print(QPainter *p, double scale [2])
{
    foreach (const Cmd *c, m_cmds) {
        if (c->m_cmd == Cmd::Attributes) {
            const AttrCmd *ac = static_cast<const AttrCmd *>(c);

            // QFont f = ac->m_font;
            // f.setPointSizeFloat ( f.pointSizeFloat ( ) * scale [1] );
            p->setFont(ac->m_font);

            if (ac->m_color.isValid())
                p->setPen(QPen(ac->m_color, int(ac->m_linewidth * (scale [0] + scale [1]) / 2.f), Qt::PenStyle(ac->m_linestyle)));
            else
                p->setPen(QPen(Qt::NoPen));

            if (ac->m_bgcolor.isValid())
                p->setBrush(QBrush(ac->m_bgcolor));
            else
                p->setBrush(QBrush(Qt::NoBrush));
        }
        else {
            const DrawCmd *dc = static_cast<const DrawCmd *>(c);

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

            case Cmd::Pixmap: {
                QPixmap pix = dc->m_p1.value<QPixmap>();

                if (!pix.isNull()) {
                    QRect dr = QRect(x, y, w, h);

                    QSize oldsize = dr.size();
                    QSize newsize = pix.size();
                    newsize.scale(oldsize, Qt::KeepAspectRatio);

                    dr.setSize(newsize);
                    dr.translate((oldsize.width() - newsize.width()) / 2,
                                 (oldsize.height() - newsize.height()) / 2);

                    p->drawPixmap(dr, pix);
                }
                break;
            }

            case Cmd::Attributes:
                break;
            }
        }
    }
}


QFont ReportPage::font() const
{
    return m_attr.m_font;
}

QColor ReportPage::color() const
{
    return m_attr.m_color;
}

QColor ReportPage::bgColor() const
{
    return m_attr.m_bgcolor;
}

int ReportPage::lineStyle() const
{
    return m_attr.m_linestyle;
}

double ReportPage::lineWidth() const
{
    return m_attr.m_linewidth;
}

void ReportPage::attr_cmd()
{
    AttrCmd *ac = new AttrCmd();
    *ac = m_attr;
    ac->m_cmd = Cmd::Attributes;
    m_cmds.append(ac);
}

void ReportPage::setFont(const QFont &f)
{
    m_attr.m_font = f;
    attr_cmd();
}

void ReportPage::setColor(const QColor &c)
{
    m_attr.m_color = c;
    attr_cmd();
}

void ReportPage::setBgColor(const QColor &c)
{
    m_attr.m_bgcolor = c;
    attr_cmd();
}

void ReportPage::setLineStyle(int linestyle)
{
    m_attr.m_linestyle = linestyle;
    attr_cmd();
}

void ReportPage::setLineWidth(double linewidth)
{
    m_attr.m_linewidth = linewidth;
    attr_cmd();
}


QSize ReportPage::textSize(const QString &text)
{
    QFontMetrics fm(m_attr.m_font);
    QPaintDevice *pd = m_job->paintDevice();
    QSize s = fm.size(0, text);
    return QSize(s.width() * pd->widthMM() / pd->width(),
                 s.height() * pd->heightMM() / pd->height());
}

void ReportPage::drawText(double x, double y, const QString &text)
{
    DrawCmd *dc = new DrawCmd();
    dc->m_cmd = Cmd::Text;
    dc->m_x = x;
    dc->m_y = y;
    dc->m_w = dc->m_h = -1;
    dc->m_p2 = text;
    m_cmds.append(dc);
}

void ReportPage::drawText(double left, double top, double width, double height, Alignment align, const QString &text)
{
    DrawCmd *dc = new DrawCmd();
    dc->m_cmd = Cmd::Text;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    dc->m_p1 = int(align);
    dc->m_p2 = text;
    m_cmds.append(dc);
}

void ReportPage::drawLine(double x1, double y1, double x2, double y2)
{
    DrawCmd *dc = new DrawCmd();
    dc->m_cmd = Cmd::Line;
    dc->m_x = x1;
    dc->m_y = y1;
    dc->m_w = x2;
    dc->m_h = y2;
    m_cmds.append(dc);
}

void ReportPage::drawRect(double left, double top, double width, double height)
{
    DrawCmd *dc = new DrawCmd();
    dc->m_cmd = Cmd::Rect;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    m_cmds.append(dc);
}

void ReportPage::drawEllipse(double left, double top, double width, double height)
{
    DrawCmd *dc = new DrawCmd();
    dc->m_cmd = Cmd::Ellipse;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    m_cmds.append(dc);
}

void ReportPage::drawPixmap(double left, double top, double width, double height, const QPixmap &pixmap)
{
    DrawCmd *dc = new DrawCmd();
    dc->m_cmd = Cmd::Pixmap;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    dc->m_p1 = pixmap;
    m_cmds.append(dc);
}



///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////


ReportMoneyStatic::ReportMoneyStatic(QScriptEngine *eng)
    : m_engine(eng)
{
    setObjectName(QLatin1String("Money"));
}

double ReportMoneyStatic::fromValue(double d)
{
    return d;
}

double ReportMoneyStatic::fromLocalValue(double d)
{
    return d; //TODO: / Currency::rate();
}

double ReportMoneyStatic::value(double d) const
{
    return d;
}
double ReportMoneyStatic::localValue(double d) const
{
    return d; //TODO: * Currency::rate();
}

QString ReportMoneyStatic::localCurrencySymbol() const
{
    return QString(); //TODO: Currency::symbol();
}

QString ReportMoneyStatic::toString(double /*d*/, bool /*with_currency_symbol*/, int precision)
{
    if (precision > 3 || precision < 0) {
        m_engine->currentContext()->throwError("Money.toString(): precision has to be in the range [0 ..3]");
        return QString();
    }
    return QString(); //TODO: Currency(d).toUSD(with_currency_symbol ? Currency::LocalSymbol : Currency::NoSymbol, precision);
}

QString ReportMoneyStatic::toLocalString(double /*d*/, bool /*with_currency_symbol*/, int precision)
{
    if (precision > 3 || precision < 0) {
        m_engine->currentContext()->throwError("Money.toLocalString(): precision has to be in the range [0 ..3]");
        return QString();
    }
    return QString(); //TODO: Currency(d).toLocal(with_currency_symbol ? Currency::LocalSymbol : Currency::NoSymbol, precision);
}
