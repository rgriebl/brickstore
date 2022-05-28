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
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentio.h"
#include "utility/utility.h"
#include "importinventorydialog.h"
#include "importinventorywidget.h"
#include "selectitem.h"
#include "progressdialog.h"


ImportInventoryDialog::ImportInventoryDialog(QWidget *parent)
    : ImportInventoryDialog(nullptr, 1, BrickLink::Condition::New, parent)
{ }

ImportInventoryDialog::ImportInventoryDialog(const BrickLink::Item *item, int quantity,
                                             BrickLink::Condition condition, QWidget *parent)
    : QDialog(parent)
    , m_verifyItem(item)
{
    setWindowTitle(tr("Import BrickLink Inventory"));

    m_import = new ImportInventoryWidget(this);

    if (!m_verifyItem) {
        m_select = new SelectItem(this);
        m_select->setExcludeWithoutInventoryFilter(true);
        connect(m_select, &SelectItem::itemSelected,
                this, &ImportInventoryDialog::checkItem);
        setSizeGripEnabled(true);
        setFocusProxy(m_select);
    } else {
        m_verifyLabel = new QLabel(this);
        m_verifyLabel->setText(tr("Parting out:") % u" <b>" % QString::fromLatin1(m_verifyItem->id())
                               % u" " % m_verifyItem->name() % u"</b");

        setFocusProxy(m_import);
    }

    m_buttons = new QDialogButtonBox(this);
    m_buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Close);
    connect(m_buttons, &QDialogButtonBox::accepted,
            this, &ImportInventoryDialog::importInventory);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &ImportInventoryDialog::reject);

    auto *layout = new QVBoxLayout(this);
    if (m_select)
        layout->addWidget(m_select, 10);
    if (m_verifyLabel)
        layout->addWidget(m_verifyLabel);
    layout->addWidget(m_import);
    layout->addWidget(m_buttons);

    if (m_select) {
        QByteArray ba = Config::inst()->value("/MainWindow/ImportInventoryDialog/Geometry"_l1).toByteArray();
        if (!ba.isEmpty())
            restoreGeometry(ba);

        ba = Config::inst()->value("/MainWindow/ImportInventoryDialog/SelectItem"_l1)
                .toByteArray();
        if (!m_select->restoreState(ba)) {
            m_select->restoreState(SelectItem::defaultState());
            m_select->setCurrentItemType(BrickLink::core()->itemType('S'));
        }

        ba = Config::inst()->value("/MainWindow/ImportInventoryDialog/Details"_l1)
                .toByteArray();
        m_import->restoreState(ba);

        if (auto *a = ActionManager::inst()->action("bricklink_catalog")) {
            new QShortcut(a->shortcuts().constFirst(), this, [this]() {
                if (const auto item = m_select->currentItem())
                    BrickLink::core()->openUrl(BrickLink::Url::CatalogInfo, item);
            });
        }
        if (auto *a = ActionManager::inst()->action("bricklink_priceguide")) {
            new QShortcut(a->shortcuts().constFirst(), this, [this]() {
                const auto item = m_select->currentItem();
                if (item && !item->itemType()->hasColors())
                    BrickLink::core()->openUrl(BrickLink::Url::PriceGuideInfo, item);
            });
        }
        if (auto *a = ActionManager::inst()->action("bricklink_lotsforsale")) {
            new QShortcut(a->shortcuts().constFirst(), this, [this]() {
                const auto item = m_select->currentItem();
                if (item && !item->itemType()->hasColors())
                    BrickLink::core()->openUrl(BrickLink::Url::LotsForSale, item);
            });
        }
        checkItem(m_select->currentItem(), false);
    } else {
        m_import->setQuantity(quantity);
        m_import->setCondition(condition);

        QMetaObject::invokeMethod(this, [this]() { setFixedSize(sizeHint()); }, Qt::QueuedConnection);

        checkItem(m_verifyItem, false);
    }

    languageChange();
}

ImportInventoryDialog::~ImportInventoryDialog()
{
    if (!m_verifyItem) {
        Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Geometry"_l1, saveGeometry());
        Config::inst()->setValue("/MainWindow/ImportInventoryDialog/SelectItem"_l1, m_select->saveState());
        Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Details"_l1, m_import->saveState());
    }
}

bool ImportInventoryDialog::setItem(const BrickLink::Item *item)
{
    return m_select ? m_select->setCurrentItem(item) : false;
}

const BrickLink::Item *ImportInventoryDialog::item() const
{
    return m_verifyItem ? m_verifyItem : m_select->currentItem();
}

int ImportInventoryDialog::quantity() const
{
    return m_import->quantity();
}

BrickLink::Condition ImportInventoryDialog::condition() const
{
    return m_import->condition();
}

BrickLink::Status ImportInventoryDialog::extraParts() const
{
    return m_import->extraParts();
}

bool ImportInventoryDialog::includeInstructions() const
{
    return m_import->includeInstructions();
}

bool ImportInventoryDialog::includeAlternates() const
{
    return m_import->includeAlternates();
}

bool ImportInventoryDialog::includeCounterParts() const
{
    return m_import->includeCounterParts();
}

void ImportInventoryDialog::languageChange()
{
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Import"));
}

void ImportInventoryDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ImportInventoryDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    activateWindow();
}

void ImportInventoryDialog::keyPressEvent(QKeyEvent *e)
{
    // simulate QDialog behavior
    if (e->matches(QKeySequence::Cancel)) {
        reject();
        return;
    } else if ((!e->modifiers() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))
               || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter))) {
        // we need the animateClick here instead of triggering directly: otherwise we
        // get interference from the itemActivated signal on the QTreeView, resulting in
        // double triggering
        auto *button = m_buttons->button(QDialogButtonBox::Ok);
        if (button->isVisible() && button->isEnabled())
            button->animateClick();
        return;
    }

    QWidget::keyPressEvent(e);
}


QSize ImportInventoryDialog::sizeHint() const
{
    if (m_verifyItem) {
        return QDialog::sizeHint();
    } else {
        QFontMetrics fm(font());
        return QSize(fm.horizontalAdvance("m"_l1) * 120, fm.height() * 30);
    }
}


void ImportInventoryDialog::checkItem(const BrickLink::Item *it, bool ok)
{
    m_import->setItem(it);

    auto *button = m_buttons->button(QDialogButtonBox::Ok);
    button->setEnabled((it));
    if (ok)
        button->animateClick();
}

void ImportInventoryDialog::importInventory()
{
    if (m_verifyItem) {
        accept();
    } else {
        Document::fromPartInventory(item(), nullptr, quantity(), condition(),
                                    extraParts(), includeInstructions(),
                                    includeAlternates(), includeCounterParts());
    }
}

#include "moc_importinventorydialog.cpp"
