// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPaintDevice>
#include <QPainter>
#if defined(BS_DESKTOP)
#  include <QPrinter>
#else
#  include <QPdfWriter>
#endif
#include <QPaintEngine>
#include <QDebug>
#include <QQmlEngine>

#include "printjob.h"


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

/*! \qmltype PrintPage
    \inqmlmodule BrickStore
    \ingroup qml-printing
    \brief An instance of this type represents the canvas of a printer page.

    \note All coordinates are floating point values and the unit is \c mm. Check
          PrintJob::paperSize to make sure you adapt to the paper format of the selected printer.
*/
/*! \qmlproperty int PrintPage::number
    \readonly
    The page index within the current print job.
    \sa PrintJob
*/
/*! \qmlproperty font PrintPage::font
    The currently selected font.
    You can create and assign font objects using the \c{Qt.font()} factory function.

    See the \l {https://doc.qt.io/qt-5/qml-qtqml-qt.html#font-method} {Qt documentation for Qt.font}
    and the \l {https://doc.qt.io/qt-5/qml-font.html} {QML font documentation} for the available
    font properties.

    \badcode
    ...
    page.font = Qt.font({ family: "Times", pointSize: 12 })
    ...
    \endcode
    The default is \c{{ family: "Arial", pointSize: 10 }}.
*/
/*! \qmlproperty color PrintPage::color
    The currently selected color, used for drawing text, lines and the outlines of rectangles and
    ellipses.

    See the \l {https://doc.qt.io/qt-5/qml-color.html} {QML color documentation}.

    \badcode
    ...
    page.color = active ? "black" : "#808080"
    ...
    \endcode

    The default is \c{"black"}.
*/
/*! \qmlproperty color PrintPage::backgroundColor
    The currently selected background color, used for filling rectangles and ellipses.
    Set to \c{"transparent"} to prevent filling.

    See the \l {https://doc.qt.io/qt-5/qml-color.html} {QML color documentation}.

    \badcode
    ...
    page.backgroundColor = Qt.rgba(.13, .4, .66, 1)
    ...
    \endcode
    The default is \c{"white"}.
*/
/*! \qmlproperty enumeration PrintPage::lineStyle
    The style of lines, used for drawing lines and the outlines of rectangles and
    ellipses.

    The available values for this line style enumeration are:

    \value PrintPage.SolidLine    A plain line.
    \value PrintPage.NoLine       No line at all. For example, drawRect fills but does not draw any outline.
    \value PrintPage.DashLine     Dashes separated by a few pixels.
    \value PrintPage.DotLine      Dots separated by a few pixels.
    \value PrintPage.DashDotLine  Alternate dots and dashes.
    \value PrintPage.DashDotDotLine  One dash, two dots, one dash, two dots.

    The default is \c{PrintPage.SolidLine}.
*/
/*! \qmlproperty real PrintPage::lineWidth
    The width of a line, used for drawing lines and the outlines of rectangles and
    ellipses.

    A line width of zero indicates a cosmetic pen. This means that the pen width is always drawn
    one pixel wide, independent of the actual pixel density and resolution of the current output
    device.

    The default is \c{0.1}.
*/
/*! \qmlmethod size PrintPage::textSize(string text)
    Calculates the size of string \a text, if it was rendered in the currently selected font.
*/
/*! \qmlmethod PrintPage::drawText(real left, real top, real width, real height, int flags, string text)
    Draws the given \a text within the rectangle specified by the top-left corner at (\a left, \a top)
    and a size of (\a width x \a height) using the currently selected font and color.

    If the text is larger than the specified rectangle, the text is clipped. You can use textSize()
    to calculate the size needed to fit a specific text element. You can suppress this feature
    using the \c TextDontClip flag.

    The text can be aligned within this rectangle through the \a flags parameter. These possible
    are the possible values and can you can or them (\c{|}) together where it makes sense:

    \value PrintPage.AlignLeft     Aligns with the left edge.
    \value PrintPage.AlignHCenter  Centers horizontally in the available space.
    \value PrintPage.AlignRight    Aligns with the right edge.
    \value PrintPage.AlignTop      Aligns with the top.
    \value PrintPage.AlignVCenter  Centers vertically in the available space.
    \value PrintPage.AlignBottom   Aligns with the bottom.
    \value PrintPage.AlignCenter   Centers in both dimensions.
    \value PrintPage.TextDontClip  If it's impossible to stay within the given bounds, it prints outside.
    \value PrintPage.TextWordWrap  Breaks lines at appropriate points, e.g. at word boundaries.
    \value PrintPage.TextWrapAnywhere  Breaks lines anywhere, even within words.
*/
/*! \qmlmethod PrintPage::drawLine(real x1, real y1, real x2, real y2)
    Draws a line from (\a x1, \a y1) to (\a x2, \a y2) using color.
*/
/*! \qmlmethod PrintPage::drawRect(real left, real top, real width, real height)
    Draws a rectangle with the top-left corner at (\a left, \a top) and a size of
    (\a width x \a height).

    The rectangle is filled with backgroundColor and the outline is drawn using a lineWidth thick
    pencil using the selected color and lineStyle.
*/
/*! \qmlmethod PrintPage::drawEllipse(real left, real top, real width, real height)
    Draws an ellipse within the rectangle specified by the top-left corner at (\a left, \a top)
    and a size of (\a width x \a height).

    The ellipse is filled with backgroundColor and the outline is drawn using a lineWidth thick
    pencil using the selected color and lineStyle.
*/
/*! \qmlmethod PrintPage::drawImage(real left, real top, real width, real height, image image)
    Draws the given \a image with its top-left corner at (\a left, \a top), scaled to the size
    (\a width x \a height).
*/

QmlPrintPage::QmlPrintPage(const QmlPrintJob *job)
    : QObject(const_cast <QmlPrintJob *>(job)), m_job(job)
{
    setObjectName(u"Page" + QString::number(job->pageCount() + 1));

    m_attr.m_font = QFont(u"Arial"_qs, 10);
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

void QmlPrintPage::print(QPainter *p, double scaleX, double scaleY) const
{
    for (const Cmd *c : m_cmds) {
        if (c->m_cmd == Cmd::Attributes) {
            const auto *ac = static_cast<const AttrCmd *>(c);

            // QFont f = ac->m_font;
            // f.setPointSizeFloat ( f.pointSizeFloat ( ) * scale [1] );
            p->setFont(ac->m_font);

            if (ac->m_color.isValid())
                p->setPen(QPen(ac->m_color, int(ac->m_linewidth * (scaleX + scaleY) / 2.), Qt::PenStyle(ac->m_linestyle)));
            else
                p->setPen(QPen(Qt::NoPen));

            if (ac->m_bgcolor.isValid())
                p->setBrush(QBrush(ac->m_bgcolor));
            else
                p->setBrush(QBrush(Qt::NoBrush));
        }
        else {
            const auto *dc = static_cast<const DrawCmd *>(c);

            int x = int(dc->m_x * scaleX);
            int y = int(dc->m_y * scaleY);
            int w = int(dc->m_w * scaleX);
            int h = int(dc->m_h * scaleY);

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
                auto img = dc->m_p1.value<QImage>();

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

void QmlPrintPage::pushAttrCmd()
{
    auto *ac = new AttrCmd(m_attr);
    ac->m_cmd = Cmd::Attributes;
    m_cmds.append(ac);
}

void QmlPrintPage::setFont(const QFont &font)
{
    m_attr.m_font = font;
    pushAttrCmd();
}

void QmlPrintPage::setColor(const QColor &color)
{
    m_attr.m_color = color;
    pushAttrCmd();
}

void QmlPrintPage::setBgColor(const QColor &color)
{
    m_attr.m_bgcolor = color;
    pushAttrCmd();
}

void QmlPrintPage::setLineStyle(int linestyle)
{
    m_attr.m_linestyle = linestyle;
    pushAttrCmd();
}

void QmlPrintPage::setLineWidth(double linewidth)
{
    m_attr.m_linewidth = linewidth;
    pushAttrCmd();
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

/*! \qmltype PrintJob
    \inqmlmodule BrickStore
    \ingroup qml-printing
    \brief An instance of this type represents an active print job.
*/
/*! \qmlproperty int PrintJob::pageCount
    \readonly
    The number of pages added to this print job.
*/
/*! \qmlproperty size PrintJob::paperSize
    \readonly
    The paper size of a page in \c mm.
*/
/*! \qmlmethod PrintPage PrintJob::addPage()
    Calling this function adds a new PrintPage to the print job and returns a reference to it for
    painting.
*/
/*! \qmlmethod PrintPage PrintJob::getPage(int index)
    Returns a reference to an already added PrintPage at \a index or \c null if the index is
    invalid.
*/
/*! \qmlmethod PrintJob::abort()
    Request this print job to be aborted.
    \note Control will return back to the next JavaScript statement and you still have to return
          normally from your extension's PrintingScriptAction::printFunction() function.
*/

QmlPrintJob::QmlPrintJob(QPaintDevice *pd)
    : QObject(nullptr)
    , m_pd(pd)
{
    setObjectName(u"Job"_qs);
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
    int pageNo = int(m_pages.size());
    page->setObjectName(u"Print page #"_qs + QString::number(pageNo));
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
    return int(m_pages.count());
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

bool QmlPrintJob::print(const QList<uint> &pages)
{
    if (m_pages.isEmpty())
        return false;

    QPainter p;
    if (!p.begin(m_pd))
        return false;

#if defined(BS_DESKTOP)
    QPrinter *prt = (m_pd->devType() == QInternal::Printer) ? static_cast<QPrinter *>(m_pd) : nullptr;

#  if defined(Q_OS_WIN) && (QT_VERSION < QT_VERSION_CHECK(6, 8, 0)) // workaround for QTBUG-5363
    if (prt && prt->fullPage() && !prt->printerName().isEmpty()
            && (p.paintEngine()->type() != QPaintEngine::Picture)) { // printing to a real printer
        auto margins = prt->pageLayout().marginsPixels(prt->resolution());
        p.translate(-margins.left(), -margins.top());
    }
#  endif
#else
    QPdfWriter *prt = static_cast<QPdfWriter *>(m_pd);
#endif

    double scaleX = double(m_pd->logicalDpiX()) / 25.4;
    double scaleY = double(m_pd->logicalDpiY()) / 25.4;
    bool no_new_page = true;

    for (int i = 0; i < pageCount(); ++i) {
        if (!pages.isEmpty() && !pages.contains(uint(i + 1)))
            continue;

        QmlPrintPage *page = m_pages.at(i);

        if (!no_new_page && prt)
            prt->newPage();

        page->print(&p, scaleX, scaleY);
        no_new_page = false;
    }
    return true;
}

#include "moc_printjob.cpp"
