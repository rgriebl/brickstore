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
#include <QToolButton>
#include <QAction>

#include "bricklink/core.h"
#include "bricklink/item.h"
#include "common/config.h"
#include "desktopuihelpers.h"
#include "selectitemdialog.h"
#include "selectitem.h"


SelectItemDialog::SelectItemDialog(bool popupMode, QWidget *parent)
    : QDialog(parent)
    , m_popupMode(popupMode)
{
    if (popupMode)
        setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    setupUi(this);
    w_si->setExcludeWithoutInventoryFilter(false);

    auto ba = Config::inst()->value(u"/MainWindow/ModifyItemDialog/SelectItem"_qs)
            .toByteArray();
    if (!w_si->restoreState(ba)) {
        w_si->restoreState(SelectItem::defaultState());
        w_si->setCurrentItemType(BrickLink::core()->itemType('P'));
    }

    connect(w_si, &SelectItem::itemSelected,
            this, &SelectItemDialog::checkItem);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_resetGeometryAction = new QAction(this);
    m_resetGeometryAction->setIcon(QIcon::fromTheme(u"zoom-fit-best"_qs));
    m_resetGeometryAction->setToolTip(tr("Reset the position to automatic mode"));
    m_resetGeometryAction->setVisible(false);

    connect(m_resetGeometryAction, &QAction::triggered, this, [this]() {
        DesktopUIHelpers::setPopupPos(this, m_popupPos);
        m_resetGeometryAction->setVisible(false);
    }, Qt::QueuedConnection);

    if (popupMode) {
        auto reset = new QToolButton();
        reset->setAutoRaise(true);
        reset->setToolButtonStyle(Qt::ToolButtonIconOnly);
        reset->setDefaultAction(m_resetGeometryAction);

        w_buttons->addButton(reset, QDialogButtonBox::ResetRole);
    }

    setFocusProxy(w_si);

    m_geometryConfigKey = popupMode ? u"/MainWindow/ModifyItemPopup/Geometry"_qs
                                    : u"/MainWindow/ModifyItemDialog/Geometry"_qs;

    if (!popupMode)
        restoreGeometry(Config::inst()->value(m_geometryConfigKey).toByteArray());
}

SelectItemDialog::~SelectItemDialog()
{
    if (!m_popupMode)
        Config::inst()->setValue(m_geometryConfigKey, saveGeometry());
    Config::inst()->setValue(u"/MainWindow/ModifyItemDialog/SelectItem"_qs, w_si->saveState());
}

void SelectItemDialog::setItemType(const BrickLink::ItemType *itt)
{
    w_si->setCurrentItemType(itt);
}

void SelectItemDialog::setItem(const BrickLink::Item *item)
{
    w_si->clearFilter();
    w_si->setCurrentItem(item, true);
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

void SelectItemDialog::setPopupGeometryChanged(bool b)
{
    m_resetGeometryAction->setVisible(b);
}

bool SelectItemDialog::isPopupGeometryChanged() const
{
    return m_resetGeometryAction->isVisible();
}

void SelectItemDialog::setPopupPosition(const QRect &pos)
{
    m_popupPos = pos; // we need to delay the positioning, because X11 doesn't know the frame size yet
}

void SelectItemDialog::moveEvent(QMoveEvent *e)
{
    QDialog::moveEvent(e);
    if (m_popupMode)
        setPopupGeometryChanged(true);
}

void SelectItemDialog::resizeEvent(QResizeEvent *e)
{
    QDialog::resizeEvent(e);
    if (m_popupMode)
        setPopupGeometryChanged(true);
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
                    setPopupGeometryChanged(false);
                } else {
                    setPopupGeometryChanged(true);
                }
            }, Qt::QueuedConnection);
        }
    }
}

void SelectItemDialog::hideEvent(QHideEvent *e)
{
    if (m_popupMode) {
        if (isPopupGeometryChanged())
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
