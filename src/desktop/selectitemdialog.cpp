/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QPushButton>

#include "bricklink/core.h"
#include "bricklink/item.h"
#include "common/config.h"
#include "utility/utility.h"
#include "desktopuihelpers.h"
#include "selectitemdialog.h"
#include "selectitem.h"


SelectItemDialog::SelectItemDialog(bool popupMode, QWidget *parent)
    : QDialog(parent)
    , m_popupMode(popupMode)
{
    setupUi(this);
    w_si->setExcludeWithoutInventoryFilter(false);

    auto ba = Config::inst()->value("/MainWindow/ModifyItemDialog/SelectItem"_l1)
            .toByteArray();
    if (!w_si->restoreState(ba)) {
        w_si->restoreState(SelectItem::defaultState());
        w_si->setCurrentItemType(BrickLink::core()->itemType('P'));
    }

    connect(w_si, &SelectItem::itemSelected,
            this, &SelectItemDialog::checkItem);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_si);

    m_geometryConfigKey = popupMode ? "/MainWindow/ModifyItemPopup/Geometry"_l1
                                    : "/MainWindow/ModifyItemDialog/Geometry"_l1;

    if (!popupMode)
        restoreGeometry(Config::inst()->value(m_geometryConfigKey).toByteArray());
}

SelectItemDialog::~SelectItemDialog()
{
    if (!m_popupMode)
        Config::inst()->setValue(m_geometryConfigKey, saveGeometry());
    Config::inst()->setValue("/MainWindow/ModifyItemDialog/SelectItem"_l1, w_si->saveState());
}

void SelectItemDialog::setItemType(const BrickLink::ItemType *itt)
{
    w_si->setCurrentItemType(itt);
}

void SelectItemDialog::setItem(const BrickLink::Item *item)
{
    w_si->setCurrentItem(item, true);
    w_si->clearFilter();
}

const BrickLink::Item *SelectItemDialog::item() const
{
    return w_si->currentItem();
}

void SelectItemDialog::checkItem(const BrickLink::Item *item, bool ok)
{
    bool b = item && (w_si->hasExcludeWithoutInventoryFilter() ? item->hasInventory() : true);

    QPushButton *p = w_buttons->button(QDialogButtonBox::Ok);
    p->setEnabled(b);

    if (b && ok)
        p->animateClick();
}

int SelectItemDialog::execAtPosition(const QRect &pos)
{
    setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    m_popupPos = pos; // we need to delay the positioning, because X11 doesn't know the frame size yet
    return QDialog::exec();
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

void SelectItemDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowStateChange) {
        if (m_popupMode && m_popupPos.isValid()) {
            if (windowState() & Qt::WindowMaximized) {
                QMetaObject::invokeMethod(this, [this]() {
                    setWindowState(Qt::WindowNoState | Qt::WindowActive);
                    DesktopUIHelpers::setPopupPos(this, m_popupPos);
                    m_geometryChanged = false;
                }, Qt::QueuedConnection);
            }
        }
    }
    return QDialog::changeEvent(e);
}
#elif defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)

bool SelectItemDialog::event(QEvent *e)
{
    if (e->type() == QEvent::NonClientAreaMouseButtonDblClick) {
        if (m_popupMode && m_popupPos.isValid()) {
            QMetaObject::invokeMethod(this, [this]() {
                DesktopUIHelpers::setPopupPos(this, m_popupPos);
                m_geometryChanged = false;
            }, Qt::QueuedConnection);
            return true;
        }
    }
    return QDialog::event(e);
}
#endif

void SelectItemDialog::moveEvent(QMoveEvent *e)
{
    QDialog::moveEvent(e);
    if (m_popupMode)
        m_geometryChanged = true;
}

void SelectItemDialog::resizeEvent(QResizeEvent *e)
{
    QDialog::resizeEvent(e);
    if (m_popupMode)
        m_geometryChanged = true;
}

void SelectItemDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    if (m_popupMode) {
        activateWindow();
        w_si->setFocus();

        if (m_popupPos.isValid()) {
            QMetaObject::invokeMethod(this, [this]() {
                auto ba = Config::inst()->value(m_geometryConfigKey).toByteArray();
                if (ba.isEmpty() || !restoreGeometry(ba)) {
                    DesktopUIHelpers::setPopupPos(this, m_popupPos);
                    m_geometryChanged = false;
                } else {
                    m_geometryChanged = true;
                }
            }, Qt::QueuedConnection);
        }
    }
}

void SelectItemDialog::hideEvent(QHideEvent *e)
{
    if (m_popupMode) {
        if (m_geometryChanged)
            Config::inst()->setValue(m_geometryConfigKey, saveGeometry());
        else
            Config::inst()->remove(m_geometryConfigKey);
    }
    QDialog::hideEvent(e);
}

QSize SelectItemDialog::sizeHint() const
{
    QSize s = QDialog::sizeHint();
    return { s.rwidth() * 3 / 2, s.height() * 3 / 2 };
}

#include "moc_selectitemdialog.cpp"
