#include <QToolButton>
#include <QLabel>
#include <QLayout>
#if defined(Q_OS_MAC)
#  include <QWindowsStyle>
#endif

#include "infobar.h"

struct Section {
    int          id;
    QWidget *    widget;
    QBoxLayout * layout;
    QLabel *     label;
    QWidget *    content;
    QToolButton *buttons[InfoBar::Count];
};

struct InfoBarPrivate
{
    QList<Section *> sections;
    static int       sectionIds;

    Section *findSection(int id) const;
};

int InfoBarPrivate::sectionIds = 0;

InfoBar::InfoBar(QWidget *parent)
    : QWidget(parent), d(new InfoBarPrivate)
{
    new QHBoxLayout(this);
}

InfoBar::~InfoBar()
{
}

int InfoBar::insertSection(int pos, const QString &title)
{
    Section *s = new Section;
    s->id = ++d->sectionIds;
    s->widget = new QWidget(this);
    s->layout = new QHBoxLayout(s->widget);
    s->label = new QLabel(title);
    s->layout->addWidget(s->label);
    s->content = new QWidget();
    s->layout->addWidget(s->content);
    memset(s->buttons, 0, sizeof(s->buttons));

    d->sections.insert(pos, s);
    static_cast<QBoxLayout *>(layout())->insertWidget(pos, s->widget);

    return s->id;
}

void InfoBar::removeSection(int id)
{
    if (Section *s = d->findSection(id)) {
        d->sections.removeOne(s);
        delete s->widget;
        delete s;
    }
}

Section *InfoBarPrivate::findSection(int id) const
{
    for (int i = 0; i < sections.count(); ++i) {
        Section *s = sections.at(i);

        if (s->id == id)
            return s;
    }
    return 0;
}

QLabel *InfoBar::sectionTitel(int id)
{
    if (Section *s = d->findSection(id))
        return s->label;
    return 0;
}

QAbstractButton *InfoBar::sectionButton(int id, ButtonType button)
{
    if (button < 0 || button >= Count)
        return 0;

    if (Section *s = d->findSection(id)) {
        QToolButton *b = s->buttons[button];
        if (!b) {
            s->buttons[button] = b = new QToolButton(s->widget);
            b->setAutoRaise(true);
#if defined(Q_OS_MAC)
            b->setStyle(new QWindowsStyle());
#endif
            const char *icon;
            QString tooltip;
            switch (button) {
            case Cancel:
                icon = ":/images/infobar/cancel.png";
                tooltip = tr("Cancel");
                break;
            case Previous:
                icon = ":/images/infobar/prev.png";
                tooltip = tr("Previous");
                break;
            case Next:
                icon = ":/images/infobar/next.png";
                tooltip = tr("Next");
                break;
            case More:
                icon = ":/images/infobar/more.png";
                tooltip = tr("More");
                break;
            default:
                icon = 0;
                break;
            }
            b->setIcon(QIcon(icon));
            b->setToolTip(tooltip);
            b->setText(tooltip); // TODO: REMOVE

            int buttonindex = 2;
            for (int i = 0; i < button; ++i) {
                if (s->buttons[i])
                    buttonindex++;
            }
            s->layout->insertWidget(buttonindex, b);
            b->show();
        }
        return b;
    }
    return 0;
}

QWidget *InfoBar::sectionContent(int id) const
{
    if (Section *s = d->findSection(id))
        return s->content;
    return 0;
}

void InfoBar::setSectionContent(int id, QWidget *w)
{
    if (!w)
        return;

    if (Section *s = d->findSection(id)) {
        delete s->content;
        s->content = w;
        s->layout->insertWidget(1, w);
    }
}
