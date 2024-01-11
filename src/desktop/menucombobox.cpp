// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QMenu>
#include <QPainter>

#include "menucombobox.h"


MenuComboBox::MenuComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setFrame(false);
    setProperty("transparentCombo", true);
}

void MenuComboBox::showPopup()
{
#if defined(Q_OS_MACOS)
    QComboBox::showPopup();
    return;
#endif

    if (!model()->rowCount())
        return;

    if (!m_menu) {
        m_menu = new QMenu(this);
        connect(m_menu, &QMenu::aboutToHide, this, &QComboBox::hidePopup, Qt::QueuedConnection);
    } else {
        m_menu->clear();
    }
    QAction *current = nullptr;

    for (int i = 0; i < model()->rowCount(); ++i) {
        auto index = model()->index(i, 0);
        QAction *a = m_menu->addAction(index.data().toString());
        if (i == currentIndex()) {
            current = a;
            m_menu->setActiveAction(a);
            a->setCheckable(true);
            a->setChecked(true);
            a->setIconVisibleInMenu(false);
        } else {
            a->setIcon(index.data(Qt::DecorationRole).value<QIcon>());
        }
        a->setData(i);
        connect(a, &QAction::triggered, this, [this]() {
            setCurrentIndex(qobject_cast<QAction *>(sender())->data().toInt());
        });
    }

    int dy = 0;
    if (current)
        dy += (height() - m_menu->actionGeometry(current).height()) / 2;
    if (model()->rowCount())
        dy -= m_menu->actionGeometry(m_menu->actions().constFirst()).y();
    m_menu->setMinimumSize(m_menu->minimumSizeHint().expandedTo({ width(), 0 }));

    m_menu->popup(mapToGlobal(QPoint(0, dy)), current);
}

void MenuComboBox::hidePopup()
{
    if (m_menu)
        m_menu->hide();
    QComboBox::hidePopup();
}

#include "moc_menucombobox.cpp"
