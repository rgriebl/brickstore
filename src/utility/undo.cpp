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
    void languageChange();

private:
    UndoAction(Type t, QUndoStack *stack, QObject *parent);
    UndoAction(Type t, QUndoGroup *group, QObject *parent);

    QStringList descriptionList(QUndoStack *stack) const;
    QString listLabel(int count) const;

private:
    Type         m_type;
    QMenu      * m_menu = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *     m_label = nullptr;
    QString      m_desc;
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

        // we need a margin - otherwise the list will paint over the menu's border
        QWidget *w = new QWidget();
        QVBoxLayout *l = new QVBoxLayout(w);
        int fw = m_menu->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
        l->setContentsMargins(fw, fw, fw, fw);

        m_list = new QListWidget();
        m_list->setFrameShape(QFrame::NoFrame);
        m_list->setSelectionMode(QAbstractItemView::MultiSelection);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setMouseTracking(true);
        m_list->setResizeMode(QListView::Adjust);
        auto pal = QApplication::palette(m_menu);
//        pal.setColor(QPalette::Base, pal.color(QPalette::Window));
//        pal.setColor(QPalette::Text, pal.color(QPalette::WindowText));
        m_list->setPalette(pal);

        m_label = new QLabel();
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setPalette(pal);

        l->addWidget(m_list);
        l->addWidget(m_label);
        auto *widgetaction = new QWidgetAction(this);
        widgetaction->setDefaultWidget(w);
        m_menu->addAction(widgetaction);

        m_menu->setFocusProxy(m_list);

        connect(m_list, &QListWidget::itemEntered, this, [this](QListWidgetItem *item) {
            if (m_list->currentItem() != item)
                m_list->setCurrentItem(item);
        });

        connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
            if (item) {
                int currentIndex = m_list->row(item);
                for (int i = 0; i < m_list->count(); i++)
                    m_list->item(i)->setSelected(i <= currentIndex);
                m_label->setText(listLabel(currentIndex + 1));
            }
        });

        connect(m_list, &QListWidget::itemClicked, m_list, &QListWidget::itemActivated);
        connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            if (item) {
                m_menu->close();
                emit triggered(m_list->row(item) + 1);
            }
        });

        connect(m_menu, &QMenu::aboutToShow, this, [this]() {
            m_list->clear();

            if (m_undoStack) {
                int maxw = 0;
                QFontMetrics fm = m_list->fontMetrics();
                const auto style = m_list->style();

                const QStringList dl = descriptionList(m_undoStack);
                for (const auto &desc : dl) {
                    (void) new QListWidgetItem(desc, m_list);
                    maxw = std::max(maxw, fm.horizontalAdvance(desc));
                }

                m_list->setMinimumWidth(maxw
                                        + style->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_list)
                                        + 2 * style->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, m_list)
                                        + 2 * (style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, m_list) + 1));

                if (m_list->count()) {
                    m_list->setCurrentItem(m_list->item(0));
                    m_list->setFocus();
                }
            }

            // we cannot resize the menu dynamically, so we need to set a minimum width now
            const auto fm = m_label->fontMetrics();
            m_label->setMinimumWidth(std::max(fm.horizontalAdvance(listLabel(1)),
                                              fm.horizontalAdvance(listLabel(100000)))
                                     + 2 * m_label->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
        });

        return button;
    }
    return nullptr;
}

void UndoAction::setDescription(const QString &desc)
{
    m_desc = desc;
    QString str = (m_type == Undo) ? tr("Undo") : tr("Redo");
    if (!desc.isEmpty())
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
}


bool UndoAction::eventFilter(QObject *o, QEvent *e)
{
    if (qobject_cast<QToolButton *>(o) &&
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
