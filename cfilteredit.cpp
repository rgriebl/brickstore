#include <qlineedit.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qvaluelist.h>

#include "cresource.h"
#include "cfilteredit.h"

class CFilterEditPrivate {
public:
    QLineEdit *w_lineedit;
    QPixmap m_icon_filter;
    QPixmap m_icon_menu;
    QPixmap m_icon_clear;

    class FilterType {
        int id;
        QString name;
    };

    QValueList <FilterType> m_typelist;
};

CFilterEdit::CFilterEdit(QWidget *parent, const char *name, WFlags fl)
        : QFrame(parent, name, fl)
{
    d = new CFilterEditPrivate();

    d->m_icon_filter = CResource::inst()->pixmap("filteredit/filter");
    d->m_icon_menu   = CResource::inst()->pixmap("filteredit/menu");
    d->m_icon_clear  = CResource::inst()->pixmap("filteredit/clear");

    d->w_lineedit = new QLineEdit(this);

    setFrameStyle(d->w_lineedit->frameStyle());
    setLineWidth(d->w_lineedit->lineWidth());
    setMidLineWidth(d->w_lineedit->midLineWidth());
    setMargin(d->w_lineedit->margin());

    d->w_lineedit->setFrameStyle(QFrame::NoFrame);


    connect(d->w_lineedit, SIGNAL(textChanged(const QString &)), this, SIGNAL(filterTextChanged(const QString &)));
    connect(d->w_lineedit, SIGNAL(returnPressed()), this, SIGNAL(returnPressed()));
    setFocusProxy(d->w_lineedit);
}

CFilterEdit::~CFilterEdit()
{
    delete d;
}

void CFilterEdit::slotTextChanged(const QString &str)
{
    calcLayout();
    emit filterTextChanged(str);
}

void CFilterEdit::resizeEvent(QResizeEvent *)
{
    calcLayout();
}

void CFilterEdit::calcLayout()
{
    int left = d->m_icon_filter.width() + 4;
    int right = 0;
    int lw = frameWidth();

    if (!d->w_lineedit->text().isEmpty())
        right = d->m_icon_clear.width() + 4;

    d->w_lineedit->setGeometry(lw + left, lw, width() - 2 * lw - left - right, height() - 2 * lw);
}

void CFilterEdit::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    int lw = frameWidth();

    p.drawPixmap(lw, lw, d->m_icon_filter);

    if (!d->m_typelist.isEmpty()) {
        p.drawPixmap(lw, lw, d->m_icon_menu);
    }

    if (!d->w_lineedit->text().isEmpty()) {
        p.drawPixmap(width() - lw - d->m_icon_clear.width(), lw, d->m_icon_clear);
    }
}

