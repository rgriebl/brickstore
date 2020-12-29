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

#include <QApplication>
#include <QImage>
#include <QPrinter>
#include <QFontMetrics>
#include <QPainter>
#include <QDateTime>
#include <QFileInfo>
#include <QtUiTools/QUiLoader>

#include <QtScript>

#include "framework.h"
#include "currency.h"
#include "utility.h"
#include "report.h"
#include "reportobjects.h"
#include "config.h"


ReportUtility::ReportUtility()
    : QObject(nullptr)
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

QString ReportUtility::localWeightString(double weight) const
{
    return Utility::weightToString(weight, Config::inst()->measurementSystem(), true, true);
}

QWidget *ReportUtility::loadUiFile(const QString &fileName)
{
    QFileInfo uiInfo(fileName);
    QString pathName = fileName;
    if (uiInfo.isRelative()) {
        QString scriptPath(engine()->property("bsScriptPath").toString());

        if (scriptPath.isEmpty())
            return nullptr;

        QFileInfo scriptInfo(scriptPath);
        pathName = scriptInfo.dir().absoluteFilePath(fileName);
    }

    QFile f(pathName);
    if (f.open(QFile::ReadOnly)) {
        QUiLoader loader;
        return loader.load(&f, FrameWork::inst());
    } else {
        return nullptr;
    }
}


ReportJob::ReportJob(QPaintDevice *pd)
    : QObject(nullptr)
    , m_pd(pd)
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

QObject *ReportJob::addPage()
{
    auto *page = new ReportPage(this);
    m_pages.append(page);
    emit pageCountChanged(m_pages.size());
    return page;
}

QObject *ReportJob::getPage(int i) const
{
    if (int(i) < m_pages.count())
        return m_pages.at(i);
    else
        return nullptr;
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
    if (!qFuzzyCompare(m_scaling, s)) {
        m_scaling = s;
        emit scalingChanged(s);
    }
}

int ReportJob::pageCount() const
{
    return m_pages.count();
}

Size ReportJob::paperSize() const
{
    return QSizeF(m_pd->widthMM(), m_pd->heightMM());
}

void ReportJob::dump()
{
    qDebug("Print Job Dump");
    qDebug(" # of pages: %d", int(m_pages.count()));
    qDebug(" ");

    for (int i = 0; i < m_pages.count(); ++i) {
        qDebug("Page #%d", i);
        m_pages.at(i)->dump();
    }
}

bool ReportJob::print(int from, int to)
{
    if (m_pages.isEmpty() || (from < 0) || (from > to) || (int(to) >= m_pages.count()))
        return false;

    QPainter p;
    if (!p.begin(m_pd))
        return false;

    QPrinter *prt = (m_pd->devType() == QInternal::Printer) ? static_cast<QPrinter *>(m_pd) : nullptr;

    double scaling [2];
    scaling [0] = m_scaling * double(m_pd->logicalDpiX()) / 25.4;
    scaling [1] = m_scaling * double(m_pd->logicalDpiY()) / 25.4;
    bool no_new_page = true;

    for (int i = from; i <= to; i++) {
        ReportPage *page = m_pages.at(i);

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

    m_attr.m_font = new Font(QFont("Arial", 10));
    m_attr.m_color = new Color(QColor(Qt::black));
    m_attr.m_bgcolor = new Color(QColor(Qt::white));
    m_attr.m_linewidth = 0.1;
    m_attr.m_linestyle = SolidLine;
}

int ReportPage::pageNumber() const
{
    for (int i = 0; i < m_job->pageCount(); i++) {
        if (m_job->getPage(i) == this)
            return i;
    }
    return -1;
}

void ReportPage::dump()
{
    qDebug(" # of commands: %d", int(m_cmds.count()));
    qDebug(" ");

    for (int i = 0; i < m_cmds.count(); ++i) {
        switch (m_cmds.at(i)->m_cmd) {
        case Cmd::Attributes: {
            auto *ac = static_cast<AttrCmd *>(m_cmds.at(i));

            qDebug(" [%d] Attributes (Font: %s | Color: %s | BgColor: %s | Line: %f | LineStyle: %d", i, qPrintable(ac->m_font->toQFont().toString()), qPrintable(ac->m_color->toQColor().name()), qPrintable(ac->m_bgcolor->toQColor().name()), ac->m_linewidth, ac->m_linestyle);
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

void ReportPage::print(QPainter *p, double scale [2]) const
{
    for (const Cmd *c : m_cmds) {
        if (c->m_cmd == Cmd::Attributes) {
            const auto *ac = static_cast<const AttrCmd *>(c);

            // QFont f = ac->m_font;
            // f.setPointSizeFloat ( f.pointSizeFloat ( ) * scale [1] );
            p->setFont(ac->m_font->toQFont());

            if (ac->m_color->toQColor().isValid())
                p->setPen(QPen(ac->m_color->toQColor(), int(ac->m_linewidth * (scale [0] + scale [1]) / 2.), Qt::PenStyle(ac->m_linestyle)));
            else
                p->setPen(QPen(Qt::NoPen));

            if (ac->m_bgcolor->toQColor().isValid())
                p->setBrush(QBrush(ac->m_bgcolor->toQColor()));
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


Font *ReportPage::font() const
{
    return m_attr.m_font;
}

Color *ReportPage::color() const
{
    return m_attr.m_color;
}

Color *ReportPage::bgColor() const
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
    auto *ac = new AttrCmd();
    *ac = m_attr;
    ac->m_cmd = Cmd::Attributes;
    m_cmds.append(ac);
}

void ReportPage::setFont(Font *font)
{
    m_attr.m_font->fromQFont(font->toQFont());
    attr_cmd();
}

void ReportPage::setColor(Color *color)
{
    m_attr.m_color->fromQColor(color->toQColor());
    attr_cmd();
}

void ReportPage::setBgColor(Color *color)
{
    m_attr.m_bgcolor->fromQColor(color->toQColor());
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


Size ReportPage::textSize(const QString &text)
{
    QFontMetrics fm(m_attr.m_font->toQFont());
    QPaintDevice *pd = m_job->paintDevice();
    QSize s = fm.size(0, text);
    return QSizeF(s.width() * pd->widthMM() / pd->width(),
                  s.height() * pd->heightMM() / pd->height());
}

void ReportPage::drawText(double x, double y, const QString &text)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Text;
    dc->m_x = x;
    dc->m_y = y;
    dc->m_w = dc->m_h = -1;
    dc->m_p2 = text;
    m_cmds.append(dc);
}

void ReportPage::drawText(double left, double top, double width, double height, Alignment align, const QString &text)
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

void ReportPage::drawLine(double x1, double y1, double x2, double y2)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Line;
    dc->m_x = x1;
    dc->m_y = y1;
    dc->m_w = x2;
    dc->m_h = y2;
    m_cmds.append(dc);
}

void ReportPage::drawRect(double left, double top, double width, double height)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Rect;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    m_cmds.append(dc);
}

void ReportPage::drawEllipse(double left, double top, double width, double height)
{
    auto *dc = new DrawCmd();
    dc->m_cmd = Cmd::Ellipse;
    dc->m_x = left;
    dc->m_y = top;
    dc->m_w = width;
    dc->m_h = height;
    m_cmds.append(dc);
}

void ReportPage::drawImage(double left, double top, double width, double height, const QImage &image)
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


ReportMoney::ReportMoney()
{
    setObjectName(QLatin1String("Money"));
}

double ReportMoney::fromValue(double d)
{
    return d;
}

double ReportMoney::fromLocalValue(double d)
{
    return d; //TODO: / Currency::rate();
}

double ReportMoney::value(double d) const
{
    return d;
}
double ReportMoney::localValue(double d) const
{
    return d; //TODO: * Currency::rate();
}

QString ReportMoney::localCurrencySymbol() const
{
    return QString(); //TODO: Currency::symbol();
}

QString ReportMoney::toString(double /*d*/, bool /*with_currency_symbol*/, int precision)
{
    if (precision > 3 || precision < 0) {
        engine()->currentContext()->throwError("Money.toString(): precision has to be in the range [0 ..3]");
        return QString();
    }
    return QString(); //TODO: Currency(d).toUSD(with_currency_symbol ? Currency::LocalSymbol : Currency::NoSymbol, precision);
}

QString ReportMoney::toLocalString(double /*d*/, bool /*with_currency_symbol*/, int precision)
{
    if (precision > 3 || precision < 0) {
        engine()->currentContext()->throwError("Money.toLocalString(): precision has to be in the range [0 ..3]");
        return QString();
    }
    return QString(); //TODO: Currency(d).toLocal(with_currency_symbol ? Currency::LocalSymbol : Currency::NoSymbol, precision);
}


Font::Font(const QFont &qf)
    : d(qf)
{ }

QFont Font::toQFont() const
{
    return d;
}

void Font::fromQFont(const QFont &qf)
{
    d = qf;
}

QScriptValue Font::toScriptValue(QScriptEngine *engine, Font * const &in)
{
    return engine->newQObject(in);
}

void Font::fromScriptValue(const QScriptValue &object, Font *&out)
{
    out = qobject_cast<Font *>(object.toQObject());
}

QScriptValue Font::createScriptValue(QScriptContext *, QScriptEngine *engine)
{
    Font *f = new Font();
    return engine->newQObject(f);
}

QString Font::family() const
{
    return d.family();
}

qreal Font::pointSize() const
{
    return d.pointSizeF();
}

int Font::pixelSize() const
{
    return d.pixelSize();
}

bool Font::bold() const
{
    return d.bold();
}

bool Font::italic() const
{
    return d.italic();
}

bool Font::underline() const
{
    return d.underline();
}

bool Font::strikeout() const
{
    return d.strikeOut();
}

void Font::setFamily(const QString &f)
{
    if (f != family()) {
        d.setFamily(f);
        emit familyChanged(f);
    }
}

void Font::setPointSize(qreal ps)
{
    if (!qFuzzyCompare(ps, pointSize())) {
        int oldPixelSize = pixelSize();
        d.setPointSizeF(ps);
        emit pointSizeChanged(ps);
        if (pixelSize() != oldPixelSize)
            emit pixelSizeChanged(pixelSize());
    }
}

void Font::setPixelSize(int ps)
{
    if (ps != pixelSize()) {
        qreal oldPointSize = pointSize();
        d.setPixelSize(ps);
        emit pixelSizeChanged(ps);
        if (!qFuzzyCompare(pointSize(), oldPointSize))
            emit pointSizeChanged(pointSize());
    }
}

void Font::setBold(bool b)
{
    if (b != bold()) {
        d.setBold(b);
        emit boldChanged(b);
    }
}

void Font::setItalic(bool i)
{
    if (i != italic()) {
        d.setItalic(i);
        emit italicChanged(i);
    }
}

void Font::setUnderline(bool u)
{
    if (u != underline()) {
        d.setUnderline(u);
        emit underlineChanged(u);
    }
}

void Font::setStrikeout(bool s)
{
    if (s != strikeout()) {
        d.setStrikeOut(s);
        emit strikeoutChanged(s);
    }
}


Size &Size::operator=(const QSizeF &sf)
{
    QSizeF::operator=(sf); return *this;
}

QScriptValue Size::toScriptValue(QScriptEngine *engine, const Size &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("width", s.width());
    obj.setProperty("height", s.height());
    return obj;
}

void Size::fromScriptValue(const QScriptValue &obj, Size &s)
{
    s.setWidth(obj.property("width").toNumber());
    s.setHeight(obj.property("height").toNumber());
}

QScriptValue Size::createScriptValue(QScriptContext *, QScriptEngine *engine)
{
    return engine->toScriptValue(Size());
}


Color::Color(const QColor &qc)
    : d(qc)
{ }

QColor Color::toQColor() const
{
    return d;
}

void Color::fromQColor(const QColor &qc)
{
    d = qc;
}

QScriptValue Color::toScriptValue(QScriptEngine *engine, Color * const &in)
{
    return engine->newQObject(in);
}

void Color::fromScriptValue(const QScriptValue &object, Color *&out)
{
    out = qobject_cast<Color *>(object.toQObject());
}

QScriptValue Color::createScriptValue(QScriptContext *, QScriptEngine *engine)
{
    auto *c = new Color();
    return engine->newQObject(c); // ###ScriptOwnership ???
}

void Color::setRgb(uint value)
{
    d.setRgb(value);
}

void Color::setRgb(int r, int g, int b)
{
    d.setRgb(r, g, b);
}

Color *Color::light() const
{
    return new Color(d.lighter());
}

Color *Color::dark() const
{
    return new Color(d.darker());
}

#include "moc_reportobjects.cpp"
