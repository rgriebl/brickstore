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
#include <QLayout>
#include <QtDebug>

#include "utility.h"
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
    QWidget *createWidget(QWidget *parent) override;

private:
    UndoAction(Type t, QUndoStack *stack, QObject *parent);
    UndoAction(Type t, QUndoGroup *group, QObject *parent);

    QStringList descriptionList(QUndoStack *stack) const;
    QString listLabel(int count) const;

private:
    Type         m_type;
    QUndoStack * m_undoStack;
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
{ }

UndoAction::UndoAction(Type t, QUndoGroup *group, QObject *parent)
    : UndoAction(t, group->activeStack(), parent)
{
    connect(group, &QUndoGroup::activeStackChanged,
            this, [this](QUndoStack *stack) { m_undoStack = stack; });
}

QWidget *UndoAction::createWidget(QWidget *parent)
{
    if (auto *tb = qobject_cast<QToolBar *>(parent)) {
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

        auto menu = new QMenu(button);
        button->setMenu(menu);
        button->setPopupMode(QToolButton::MenuButtonPopup);

        // we need a margin - otherwise the list will paint over the menu's border
        QWidget *w = new QWidget();
        QVBoxLayout *l = new QVBoxLayout(w);
        int fw = menu->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        l->setContentsMargins(fw, fw, fw, fw);

        auto list = new QListWidget();
        list->setFrameShape(QFrame::NoFrame);
        list->setSelectionMode(QAbstractItemView::MultiSelection);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setMouseTracking(true);
        list->setResizeMode(QListView::Adjust);
        auto pal = QApplication::palette(menu);
        list->setPalette(pal);

        auto label = new QLabel();
        label->setAlignment(Qt::AlignCenter);
        label->setPalette(pal);

        l->addWidget(list);
        l->addWidget(label);
        auto *widgetaction = new QWidgetAction(this);
        widgetaction->setDefaultWidget(w);
        menu->addAction(widgetaction);

        menu->setFocusProxy(list);

        connect(list, &QListWidget::itemEntered, this, [=](QListWidgetItem *item) {
            if (list->currentItem() != item)
                list->setCurrentItem(item);
        });

        connect(list, &QListWidget::currentItemChanged, this, [=](QListWidgetItem *item) {
            if (item) {
                int currentIndex = list->row(item);
                for (int i = 0; i < list->count(); i++)
                    list->item(i)->setSelected(i <= currentIndex);
                label->setText(listLabel(currentIndex + 1));
            }
        });

        connect(list, &QListWidget::itemClicked, list, &QListWidget::itemActivated);
        connect(list, &QListWidget::itemActivated, this, [=](QListWidgetItem *item) {
            if (item) {
                menu->close();
                emit triggered(list->row(item) + 1);
            }
        });

        connect(menu, &QMenu::aboutToShow, this, [=]() {
            list->clear();

            if (m_undoStack) {
                int maxw = 0;
                QFontMetrics fm = list->fontMetrics();
                const auto style = list->style();

                const QStringList dl = descriptionList(m_undoStack);
                for (const auto &desc : dl) {
                    (void) new QListWidgetItem(desc, list);
                    maxw = std::max(maxw, fm.horizontalAdvance(desc));
                }

                list->setMinimumWidth(maxw
                                        + style->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, list)
                                        + 2 * style->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, list)
                                        + 2 * (style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, list) + 1));

                if (list->count()) {
                    list->setCurrentItem(list->item(0));
                    list->setFocus();
                }
            }

            // we cannot resize the menu dynamically, so we need to set a minimum width now
            const auto fm = label->fontMetrics();
            label->setMinimumWidth(std::max(fm.horizontalAdvance(listLabel(1)),
                                              fm.horizontalAdvance(listLabel(100000)))
                                     + 2 * label->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
        });

        return button;
    }
    return nullptr;
}

void UndoAction::setDescription(const QString &desc)
{
    QString str = (m_type == Undo) ? tr("Undo") : tr("Redo");
    if (!desc.isEmpty())
        str = str % u" (" % desc % u')';
    setText(str);
}

QString UndoAction::listLabel(int count) const
{
    return (m_type == Undo) ? tr("Undo %n action(s)", nullptr, count)
                            : tr("Redo %n action(s)", nullptr, count);
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
