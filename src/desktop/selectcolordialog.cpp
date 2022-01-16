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
#include <QPushButton>

#include "bricklink/color.h"
#include "bricklink/core.h"
#include "common/config.h"
#include "utility/utility.h"
#include "helpers.h"
#include "selectcolordialog.h"
#include "selectcolor.h"


SelectColorDialog::SelectColorDialog(bool popupMode, QWidget *parent)
    : QDialog(parent)
    , m_popupMode(popupMode)
{
    setupUi(this);

    auto ba = Config::inst()->value("/MainWindow/ModifyColorDialog/SelectColor"_l1)
            .toByteArray();
    if (!w_sc->restoreState(ba))
        w_sc->restoreState(SelectColor::defaultState());

    connect(w_sc, &SelectColor::colorSelected,
            this, &SelectColorDialog::checkColor);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_sc);

    m_geometryConfigKey = popupMode ? "/MainWindow/ModifyColorPopup/Geometry"_l1
                                    : "/MainWindow/ModifyColorDialog/Geometry"_l1;

    if (!popupMode)
        restoreGeometry(Config::inst()->value(m_geometryConfigKey).toByteArray());
}

SelectColorDialog::~SelectColorDialog()
{
    if (!m_popupMode)
        Config::inst()->setValue(m_geometryConfigKey, saveGeometry());
    Config::inst()->setValue("/MainWindow/ModifyColorDialog/SelectColor"_l1, w_sc->saveState());
}


void SelectColorDialog::setColor(const BrickLink::Color *color)
{
    w_sc->setCurrentColor(color);
}

void SelectColorDialog::setColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item)
{
    w_sc->setCurrentColorAndItem(color, item);
}

const BrickLink::Color *SelectColorDialog::color() const
{
    return w_sc->currentColor();
}

void SelectColorDialog::checkColor(const BrickLink::Color *col, bool ok)
{
    QPushButton *p = w_buttons->button(QDialogButtonBox::Ok);
    p->setEnabled((col));

    if (col && ok)
        p->animateClick();
}

int SelectColorDialog::execAtPosition(const QRect &pos)
{
    setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    m_popupPos = pos; // we need to delay the positioning, because X11 doesn't know the frame size yet
    return QDialog::exec();
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

void SelectColorDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowStateChange) {
        if (m_popupMode && m_popupPos.isValid()) {
            if (windowState() & Qt::WindowMaximized) {
                QMetaObject::invokeMethod(this, [this]() {
                    setWindowState(Qt::WindowNoState | Qt::WindowActive);
                    Utility::setPopupPos(this, m_popupPos);
                    m_geometryChanged = false;
                }, Qt::QueuedConnection);
            }
        }
    }
    return QDialog::changeEvent(e);
}
#elif defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)

bool SelectColorDialog::event(QEvent *e)
{
    if (e->type() == QEvent::NonClientAreaMouseButtonDblClick) {
        if (m_popupMode && m_popupPos.isValid()) {
            QMetaObject::invokeMethod(this, [this]() {
                Helpers::setPopupPos(this, m_popupPos);
                m_geometryChanged = false;
            }, Qt::QueuedConnection);
            return true;
        }
    }
    return QDialog::event(e);
}
#endif

void SelectColorDialog::moveEvent(QMoveEvent *e)
{
    QDialog::moveEvent(e);
    if (m_popupMode)
        m_geometryChanged = true;
}

void SelectColorDialog::resizeEvent(QResizeEvent *e)
{
    QDialog::resizeEvent(e);
    if (m_popupMode)
        m_geometryChanged = true;
}

void SelectColorDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    if (m_popupMode) {
        activateWindow();
        w_sc->setFocus();

        if (m_popupPos.isValid()) {
            QMetaObject::invokeMethod(this, [this]() {
                auto ba = Config::inst()->value(m_geometryConfigKey).toByteArray();
                if (ba.isEmpty() || !restoreGeometry(ba)) {
                    Helpers::setPopupPos(this, m_popupPos);
                    m_geometryChanged = false;
                } else {
                    m_geometryChanged = true;
                }
            }, Qt::QueuedConnection);
        }
    }
}

void SelectColorDialog::hideEvent(QHideEvent *e)
{
    if (m_popupMode) {
        if (m_geometryChanged)
            Config::inst()->setValue(m_geometryConfigKey, saveGeometry());
        else
            Config::inst()->remove(m_geometryConfigKey);
    }
    QDialog::hideEvent(e);
}

QSize SelectColorDialog::sizeHint() const
{
    QSize s = QDialog::sizeHint();
    return { s.rwidth() * 3 / 2, s.height() * 3 / 2 };
}

#include "moc_selectcolordialog.cpp"
