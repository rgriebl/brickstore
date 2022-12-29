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
#include <QStringListModel>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/document.h"
#include "importinventorydialog.h"
#include "importinventorywidget.h"
#include "selectitem.h"


ImportInventoryDialog::ImportInventoryDialog(QWidget *parent)
    : ImportInventoryDialog(nullptr, 1, BrickLink::Condition::Count, parent)
{ }

ImportInventoryDialog::ImportInventoryDialog(const BrickLink::Item *item, int quantity,
                                             BrickLink::Condition condition, QWidget *parent)
    : QDialog(parent)
    , m_verifyItem(item)
    , m_favoriteFilters(new QStringListModel(this))
{
    setWindowTitle(tr("Import BrickLink Inventory"));

    m_import = new ImportInventoryWidget(this);

    if (!m_verifyItem) {
        m_select = new SelectItem(this);
        m_select->setExcludeWithoutInventoryFilter(true);
        m_select->setFilterFavoritesModel(m_favoriteFilters);
        connect(m_select, &SelectItem::itemSelected,
                this, &ImportInventoryDialog::checkItem);
        setSizeGripEnabled(true);
        setFocusProxy(m_select);
    } else {
        m_verifyLabel = new QLabel(this);
        m_verifyLabel->setText(tr("Parting out:") + u" <b>" + QString::fromLatin1(m_verifyItem->id())
                               + u" " + m_verifyItem->name() + u"</b");

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

    QByteArray ba = Config::inst()->value(u"MainWindow/ImportInventoryDialog/Details"_qs)
            .toByteArray();
    m_import->restoreState(ba);

    if (m_select) {
        ba = Config::inst()->value(u"MainWindow/ImportInventoryDialog/Geometry"_qs).toByteArray();
        if (!ba.isEmpty())
            restoreGeometry(ba);

        ba = Config::inst()->value(u"MainWindow/ImportInventoryDialog/SelectItem"_qs)
                .toByteArray();
        if (!m_select->restoreState(ba)) {
            m_select->restoreState(SelectItem::defaultState());
            m_select->setCurrentItemType(BrickLink::core()->itemType('S'));
        }

        m_favoriteFilters->setStringList(Config::inst()->value(u"MainWindow/ImportInventoryDialog/Filter"_qs).toStringList());

        if (auto *a = ActionManager::inst()->action("bricklink_catalog")) {
            new QShortcut(a->shortcuts().constFirst(), this, [this]() {
                if (const auto currentItem = m_select->currentItem())
                    BrickLink::core()->openUrl(BrickLink::Url::CatalogInfo, currentItem);
            });
        }
        if (auto *a = ActionManager::inst()->action("bricklink_priceguide")) {
            new QShortcut(a->shortcuts().constFirst(), this, [this]() {
                const auto currentItem = m_select->currentItem();
                if (currentItem && !currentItem->itemType()->hasColors())
                    BrickLink::core()->openUrl(BrickLink::Url::PriceGuideInfo, currentItem);
            });
        }
        if (auto *a = ActionManager::inst()->action("bricklink_lotsforsale")) {
            new QShortcut(a->shortcuts().constFirst(), this, [this]() {
                const auto currentItem = m_select->currentItem();
                if (currentItem && !currentItem->itemType()->hasColors())
                    BrickLink::core()->openUrl(BrickLink::Url::LotsForSale, currentItem);
            });
        }
        checkItem(m_select->currentItem(), false);
    } else {
        m_import->setQuantity(quantity);
        if (condition != BrickLink::Condition::Count)
            m_import->setCondition(condition);

        QMetaObject::invokeMethod(this, [this]() { setFixedSize(sizeHint()); }, Qt::QueuedConnection);

        checkItem(m_verifyItem, false);
    }

    languageChange();
}

ImportInventoryDialog::~ImportInventoryDialog()
{
    if (!m_verifyItem) {
        Config::inst()->setValue(u"MainWindow/ImportInventoryDialog/Filter"_qs, m_favoriteFilters->stringList());
        Config::inst()->setValue(u"MainWindow/ImportInventoryDialog/Geometry"_qs, saveGeometry());
        Config::inst()->setValue(u"MainWindow/ImportInventoryDialog/SelectItem"_qs, m_select->saveState());
    }
    Config::inst()->setValue(u"MainWindow/ImportInventoryDialog/Details"_qs, m_import->saveState());
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

BrickLink::PartOutTraits ImportInventoryDialog::partOutTraits() const
{
    return m_import->partOutTraits();
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
        return QSize(fm.horizontalAdvance(u"m"_qs) * 120, fm.height() * 30);
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
                                    extraParts(), partOutTraits());
    }
}

#include "moc_importinventorydialog.cpp"
