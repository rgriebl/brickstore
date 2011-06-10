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
#include <QToolBar>
#include <QToolButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QMenu>
#include <QWidgetAction>
#include <QMouseEvent>
#include <QStyle>

#include "undo.h"


class UndoAction : public QWidgetAction {
    Q_OBJECT
public:
    enum Type { Undo, Redo };

    template<typename T>
    static QAction *create(Type type, const T *stack, QObject *parent);

public slots:
    virtual void setDescription(const QString &str);

signals:
    void triggered(int);

protected:
    virtual bool eventFilter(QObject *o, QEvent *e);
    virtual bool event(QEvent *);

    virtual QWidget *createWidget(QWidget *parent);

protected slots:
    void updateDescriptions();
    void fixMenu();
    void itemSelected(QListWidgetItem *);
    void selectRange(QListWidgetItem *);
    void setCurrentItemSlot(QListWidgetItem *item);
    void languageChange();

private:
    UndoAction(Type t, QObject *parent);

    QStringList descriptionList(QUndoStack *stack) const;

private:
    Type         m_type;
    QMenu      * m_menu;
    QListWidget *m_list;
    QLabel *     m_label;
    QString      m_desc;

    static const char *s_strings [];
};




UndoStack::UndoStack(QObject *parent)
    : QUndoStack(parent)
{ }

void UndoStack::redo(int count)
{
    setIndex(index() + count);
}

void UndoStack::undo(int count)
{
    setIndex(index() - count);
}

QAction *UndoStack::createRedoAction(QObject *parent) const
{
    return UndoAction::create(UndoAction::Redo, this, parent);
}

QAction *UndoStack::createUndoAction(QObject *parent) const
{
    return UndoAction::create(UndoAction::Undo, this, parent);
}

void UndoStack::endMacro(const QString &str)
{
    int idx = index();
    QUndoStack::endMacro();
    if (index() == (idx+1)) {
        const_cast<QUndoCommand *>(command(idx))->setText(str);
        emit undoTextChanged(str);
    }
}



UndoGroup::UndoGroup(QObject *parent)
    : QUndoGroup(parent)
{ }

void UndoGroup::redo(int count)
{
    if (QUndoStack *st = activeStack())
        st->setIndex(st->index() + count);
}

void UndoGroup::undo(int count)
{
    if (QUndoStack *st = activeStack())
        st->setIndex(st->index() - count);
}

QAction *UndoGroup::createRedoAction(QObject *parent) const
{
    return UndoAction::create(UndoAction::Redo, this, parent);
}

QAction *UndoGroup::createUndoAction(QObject *parent) const
{
    return UndoAction::create(UndoAction::Undo, this, parent);
}




// --------------------------------------------------------------------------

class UndoActionListWidget : public QListWidget {
public:
    UndoActionListWidget(QWidget *parent)
        : QListWidget(parent)
    { }

    virtual QSize sizeHint() const
    {
        int fw = 2* style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

        QSize s1 = QListWidget::sizeHint();
        QSize s2 = s1.boundedTo(QSize(300, 300));

        if ((s2.height() < s1.height()) && (s1.width() <= s2.width()))
            s2.setWidth(s1.width() + style()->pixelMetric(QStyle::PM_ScrollBarExtent));

        return s2.expandedTo(QSize(60, 60)) + QSize(fw, fw);
    }
};

class UndoActionLabel : public QLabel {
public:
    UndoActionLabel(QWidget *parent, const char **strings)
        : QLabel(parent), m_strings(strings)
    { }

    virtual QSize sizeHint() const
    {
        QSize s = QLabel::sizeHint();

        int fw = 2* style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        int w = s.width() - 2*fw;

        const QFontMetrics &fm = fontMetrics();

        for (int i = 0; i < 4; i++) {
            QString s = UndoAction::tr(m_strings[i]);
            if (!(i & 1))
                s = s.arg(1000);
            int ws = fm.width(s);
            w = qMax(w, ws);
        }
        s.setWidth(w + 2*fw + 8);
        return s;
    }

private:
    const char **m_strings;
};



const char *UndoAction::s_strings [] = {
    QT_TRANSLATE_NOOP( "UndoManager", "Undo %1 Actions" ),
    QT_TRANSLATE_NOOP( "UndoManager", "Undo Action" ),
    QT_TRANSLATE_NOOP( "UndoManager", "Redo %1 Actions" ),
    QT_TRANSLATE_NOOP( "UndoManager", "Redo Action" )
};

template <typename T>
QAction *UndoAction::create(UndoAction::Type type, const T *stack, QObject *parent)
{
    bool un = (type == Undo);

    QAction *a = new UndoAction(type, parent);
    a->setEnabled(un ? stack->canUndo() : stack->canRedo());

    connect(stack, un ? SIGNAL(undoTextChanged(const QString &)) : SIGNAL(redoTextChanged(const QString &)), a, SLOT(updateDescriptions()));
    connect(stack, un ? SIGNAL(canUndoChanged(bool))             : SIGNAL(canRedoChanged(bool)),             a, SLOT(setEnabled(bool)));
    connect(a, SIGNAL(triggered(int)), stack, un ? SLOT(undo(int)) : SLOT(redo(int)));
    connect(a, SIGNAL(triggered()),    stack, un ? SLOT(undo())    : SLOT(redo()));

    return a;
}

UndoAction::UndoAction(Type t, QObject *parent)
    : QWidgetAction(parent)
{
    m_type = t;
    m_menu = 0;
    m_list = 0;
    m_label = 0;

    languageChange();
}

QWidget *UndoAction::createWidget(QWidget *parent)
{
    if (QToolBar *tb = qobject_cast<QToolBar *>(parent)) {
        if (m_menu) {
            qWarning("Each UndoAction should only be added once to a single QToolBar");
            return 0;
        }

        // straight from QToolBar
        QToolButton *button = new QToolButton(tb);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(tb->iconSize());
        button->setToolButtonStyle(tb->toolButtonStyle());
        QObject::connect(tb, SIGNAL(iconSizeChanged(QSize)),
                         button, SLOT(setIconSize(QSize)));
        QObject::connect(tb, SIGNAL(toolButtonStyleChanged(Qt::ToolButtonStyle)),
                         button, SLOT(setToolButtonStyle(Qt::ToolButtonStyle)));
        button->setDefaultAction(this);
        button->installEventFilter(this);


        m_menu = new QMenu(button);
        button->setMenu(m_menu);
        button->setPopupMode(QToolButton::MenuButtonPopup);

        m_list = new UndoActionListWidget(m_menu);
        m_list->setFrameShape(QFrame::NoFrame);
        m_list->setSelectionMode(QAbstractItemView::MultiSelection);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setMouseTracking(true);

        QWidgetAction *listaction = new QWidgetAction(this);
        listaction->setDefaultWidget(m_list);
        m_menu->addAction(listaction);

        m_label = new UndoActionLabel(m_menu, s_strings);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->installEventFilter(this);
        m_label->setPalette(QApplication::palette(m_menu));
        m_label->setBackgroundRole(m_menu->backgroundRole());

        QWidgetAction *labelaction = new QWidgetAction(this);
        labelaction->setDefaultWidget(m_label);
        m_menu->addAction(labelaction);

        m_menu->setFocusProxy(m_list);

        connect(m_list, SIGNAL(itemEntered(QListWidgetItem *)), this, SLOT(setCurrentItemSlot(QListWidgetItem *)));
        connect(m_list, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(selectRange(QListWidgetItem *)));
        connect(m_list, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(itemSelected(QListWidgetItem *)));
        connect(m_list, SIGNAL(itemActivated(QListWidgetItem *)), this, SLOT(itemSelected(QListWidgetItem *)));

        connect(m_menu, SIGNAL(aboutToShow()), this, SLOT(fixMenu()));

        return button;
    }
    return 0;
}

void UndoAction::setDescription(const QString &desc)
{
    m_desc = desc;
    QString str = (m_type == Undo) ? tr("Undo") : tr("Redo");
    if (!desc. isEmpty())
        str.append(QString(" (%1)").arg(desc));
    setText(str);
}

bool UndoAction::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();

    return QWidgetAction::event(e);
}

void UndoAction::languageChange ( )
{
    setDescription(m_desc);

    if (m_menu)
        m_menu->resize(m_menu->sizeHint());
}


bool UndoAction::eventFilter(QObject *o, QEvent *e)
{
    if ((o == m_label) &&
        (e->type() == QEvent::MouseButtonPress) &&
        (static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)) {
        m_menu->close();
    }
    else if (qobject_cast<QToolButton *>(o) &&
             (e->type() == QEvent::EnabledChange)) {
        bool b = static_cast<QWidget *>(o)->isEnabled();
        // don't disable the widgets - this will lead to an empty menu on the Mac!
        if (b) {
            m_menu->setEnabled(b);
            m_list->setEnabled(b);
            m_label->setEnabled(b);
        }
    }

    return QAction::eventFilter(o, e);
}

void UndoAction::setCurrentItemSlot(QListWidgetItem *item)
{
    if (m_list->currentItem() != item)
        m_list->setCurrentItem(item);
}

void UndoAction::fixMenu()
{
    m_list->setCurrentItem(m_list->item(0));
    selectRange(m_list->item(0));

#if defined(Q_OS_MAC)
    m_list->setFocus();
#endif
}

void UndoAction::selectRange(QListWidgetItem *item)
{
    if (item) {
        int hl = m_list->row(item);

        for (int i = 0; i < m_list->count(); i++)
            m_list->item(i)->setSelected(i <= hl);

        QString s = tr(s_strings[((m_type == Undo) ? 0 : 2) + ((hl > 0) ? 0 : 1)]);
        if (hl > 0)
            s = s.arg(hl+1);
        m_label->setText(s);
    }
}


void UndoAction::itemSelected(QListWidgetItem *item)
{
    if (item) {
        m_menu->close();
        emit triggered(m_list->row(item) + 1);
    }
}

QStringList UndoAction::descriptionList(QUndoStack *stack) const
{
    QStringList sl;

    if (m_type == Undo) {
        for (int i = stack->index()-1; i >= 0; --i)
            sl.append(stack->text(i));
    } else {
        for (int i = stack->index(); i < stack->count(); ++i)
            sl.append(stack->text(i));
    }
    return sl;
}


void UndoAction::updateDescriptions()
{
    QObject *s = sender();

    QStringList sl;

    if (s) {
        QUndoGroup *group = qobject_cast<QUndoGroup *>(s);
        QUndoStack *stack = qobject_cast<QUndoStack *>(s);

        if (!stack && group)
            stack = group->activeStack();
        if (stack)
            sl = descriptionList(stack);
    }

    if (m_list) {
        m_list->clear();
        foreach (const QString &str, sl)
            (void) new QListWidgetItem(str, m_list);
    }

    setDescription(sl.isEmpty() ? QString() : sl.front());
}

#include "undo.moc"
