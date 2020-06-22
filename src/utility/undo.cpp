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
#include <QToolBar>
#include <QToolButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QMenu>
#include <QWidgetAction>
#include <QMouseEvent>
#include <QStyle>
#include <QtDebug>

#include "undo.h"


class UndoAction : public QWidgetAction
{
    Q_OBJECT
public:
    enum Type { Undo, Redo };

    template<typename T>
    static QAction *create(Type type, T *stack, QObject *parent);

public slots:
    void setDescription(const QString &str);

signals:
    void triggered(int);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    bool event(QEvent *) override;

    QWidget *createWidget(QWidget *parent) override;

protected slots:
    void itemSelected(QListWidgetItem *);
    void selectRange(QListWidgetItem *);
    void setCurrentItemSlot(QListWidgetItem *item);
    void languageChange();

private:
    UndoAction(Type t, QUndoStack *stack, QObject *parent);
    UndoAction(Type t, QUndoGroup *group, QObject *parent);

    QStringList descriptionList(QUndoStack *stack) const;

private:
    Type         m_type;
    QMenu      * m_menu = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *     m_label = nullptr;
    QString      m_desc;
    QUndoStack * m_undoStack;

    static const char *s_strings [];
};




UndoStack::UndoStack(QObject *parent)
    : QUndoStack(parent)
{ }

void UndoStack::redoMultiple(int count)
{
    setIndex(index() + count);
}

void UndoStack::undoMultiple(int count)
{
    setIndex(index() - count);
}

QAction *UndoStack::createRedoAction(QObject *parent)
{
    return UndoAction::create(UndoAction::Redo, this, parent);
}

QAction *UndoStack::createUndoAction(QObject *parent)
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

void UndoGroup::redoMultiple(int count)
{
    if (QUndoStack *st = activeStack())
        st->setIndex(st->index() + count);
}

void UndoGroup::undoMultiple(int count)
{
    if (QUndoStack *st = activeStack())
        st->setIndex(st->index() - count);
}

QAction *UndoGroup::createRedoAction(QObject *parent)
{
    return UndoAction::create(UndoAction::Redo, this, parent);
}

QAction *UndoGroup::createUndoAction(QObject *parent)
{
    return UndoAction::create(UndoAction::Undo, this, parent);
}




// --------------------------------------------------------------------------

class UndoActionListWidget : public QListWidget
{
    Q_OBJECT
public:
    UndoActionListWidget(QWidget *parent)
        : QListWidget(parent)
    { }

    QSize sizeHint() const override
    {
        int fw = 2* style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

        QSize s1 = QListWidget::sizeHint();
        QSize s2 = s1.boundedTo(QSize(300, 300));

        if ((s2.height() < s1.height()) && (s1.width() <= s2.width()))
            s2.setWidth(s1.width() + style()->pixelMetric(QStyle::PM_ScrollBarExtent));

        return s2.expandedTo(QSize(60, 60)) + QSize(fw, fw);
    }
private:
    Q_DISABLE_COPY(UndoActionListWidget)
};

class UndoActionLabel : public QLabel
{
    Q_OBJECT
public:
    UndoActionLabel(QWidget *parent, const char **strings)
        : QLabel(parent)
        , m_strings(strings)
    { }

    QSize sizeHint() const override
    {
        QSize s = QLabel::sizeHint();

        int fw = 2* style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        int w = s.width() - 2*fw;

        const QFontMetrics &fm = fontMetrics();

        for (int i = 0; i < 4; i++) {
            QString s = UndoAction::tr(m_strings[i]);
            if (!(i & 1))
                s = s.arg(1000);
            int ws = fm.horizontalAdvance(s);
            w = qMax(w, ws);
        }
        s.setWidth(w + 2*fw + 8);
        return s;
    }

private:
    Q_DISABLE_COPY(UndoActionLabel)

    const char **m_strings;
};



const char *UndoAction::s_strings [] = {
    QT_TRANSLATE_NOOP( "UndoManager", "Undo %1 Actions" ),
    QT_TRANSLATE_NOOP( "UndoManager", "Undo Action" ),
    QT_TRANSLATE_NOOP( "UndoManager", "Redo %1 Actions" ),
    QT_TRANSLATE_NOOP( "UndoManager", "Redo Action" )
};

template <typename T>
QAction *UndoAction::create(UndoAction::Type type, T *stackOrGroup, QObject *parent)
{
    bool un = (type == Undo);

    auto *a = new UndoAction(type, stackOrGroup, parent);
    a->setEnabled(un ? stackOrGroup->canUndo() : stackOrGroup->canRedo());

    connect(stackOrGroup, un ? &T::undoTextChanged : &T::redoTextChanged,
            a, &UndoAction::setDescription);
    connect(stackOrGroup, un ? &T::canUndoChanged  : &T::canRedoChanged,
            a, &QAction::setEnabled);

    connect(a, QOverload<int>::of(&UndoAction::triggered),
            stackOrGroup, un ? &T::undoMultiple : &T::redoMultiple);
    connect(a, &QAction::triggered,
            stackOrGroup, un ? &T::undo : &T::redo);

    return a;
}

UndoAction::UndoAction(Type t, QUndoStack *stack, QObject *parent)
    : QWidgetAction(parent)
    , m_type(t)
    , m_undoStack(stack)
{
    languageChange();
}

UndoAction::UndoAction(Type t, QUndoGroup *group, QObject *parent)
    : UndoAction(t, group->activeStack(), parent)
{
    connect(group, &QUndoGroup::activeStackChanged,
            this, [this](QUndoStack *stack) { m_undoStack = stack; });
}

QWidget *UndoAction::createWidget(QWidget *parent)
{
    if (auto *tb = qobject_cast<QToolBar *>(parent)) {
        if (m_menu) {
            qWarning("Each UndoAction should only be added once to a single QToolBar");
            return nullptr;
        }

        // straight from QToolBar
        auto *button = new QToolButton(tb);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(tb->iconSize());
        button->setToolButtonStyle(tb->toolButtonStyle());
        connect(tb, &QToolBar::iconSizeChanged,
                button, &QToolButton::setIconSize);
        connect(tb, &QToolBar::toolButtonStyleChanged,
                button, &QToolButton::setToolButtonStyle);
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

        auto *listaction = new QWidgetAction(this);
        listaction->setDefaultWidget(m_list);
        m_menu->addAction(listaction);

        m_label = new UndoActionLabel(m_menu, s_strings);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->installEventFilter(this);
        m_label->setPalette(QApplication::palette(m_menu));
        m_label->setBackgroundRole(m_menu->backgroundRole());

        auto *labelaction = new QWidgetAction(this);
        labelaction->setDefaultWidget(m_label);
        m_menu->addAction(labelaction);

        m_menu->setFocusProxy(m_list);

        connect(m_list, &QListWidget::itemEntered,
                this, &UndoAction::setCurrentItemSlot);
        connect(m_list, &QListWidget::currentItemChanged,
                this, &UndoAction::selectRange);
        connect(m_list, &QListWidget::itemClicked,
                this, &UndoAction::itemSelected);
        connect(m_list, &QListWidget::itemActivated,
                this, &UndoAction::itemSelected);

        connect(m_menu, &QMenu::aboutToShow,
                this, [this]() {
            m_list->clear();

            if (m_undoStack) {
                const QStringList dl = descriptionList(m_undoStack);
                for (const auto &desc : dl)
                    (void) new QListWidgetItem(desc, m_list);
            }

            if (m_list->count()) {
                m_list->setCurrentItem(m_list->item(0));
                selectRange(m_list->item(0));
            }

#if defined(Q_OS_MACOS)
            m_list->setFocus();
#endif
        });

        return button;
    }
    return nullptr;
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

    return QWidgetAction::eventFilter(o, e);
}

void UndoAction::setCurrentItemSlot(QListWidgetItem *item)
{
    if (m_list->currentItem() != item)
        m_list->setCurrentItem(item);
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

#include "undo.moc"

#include "moc_undo.cpp"
