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
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>

#include "bricklink.h"
#include "config.h"
#include "documentio.h"
#include "progressdialog.h"
#include "framework.h"
#include "importinventorydialog.h"


ImportInventoryDialog::ImportInventoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_select->setExcludeWithoutInventoryFilter(true);
    connect(w_select, &SelectItem::itemSelected,
            this, &ImportInventoryDialog::checkItem);
    connect(w_select, &SelectItem::currentItemTypeChanged,
            this, [this](const BrickLink::ItemType *itt) {
        w_instructions->setVisible(itt && (itt->id() == 'S'));
    });
    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, &ImportInventoryDialog::importInventory);

    QByteArray ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/Geometry")).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/SelectItem"))
            .toByteArray();
    if (!w_select->restoreState(ba)) {
        w_select->restoreState(SelectItem::defaultState());
        w_select->setCurrentItemType(BrickLink::core()->itemType('S'));
    }

    ba = Config::inst()->value(QLatin1String("/MainWindow/ImportInventoryDialog/Details"))
            .toByteArray();
    restoreState(ba);

    if (QAction *a = FrameWork::inst()->findAction("bricklink_catalog")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select->currentItem();
            if (item)
                BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, item);
        });
    }
    if (QAction *a = FrameWork::inst()->findAction("bricklink_priceguide")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select->currentItem();
            if (item && !item->itemType()->hasColors())
                BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, item);
        });
    }
    if (QAction *a = FrameWork::inst()->findAction("bricklink_lotsforsale")) {
        connect(new QShortcut(a->shortcut(), this), &QShortcut::activated, this, [this]() {
            const auto item = w_select->currentItem();
            if (item && !item->itemType()->hasColors())
                BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, item);
        });
    }
    checkItem(w_select->currentItem(), false);
    languageChange();
}

ImportInventoryDialog::~ImportInventoryDialog()
{
    Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Geometry", saveGeometry());
    Config::inst()->setValue("/MainWindow/ImportInventoryDialog/SelectItem", w_select->saveState());
    Config::inst()->setValue("/MainWindow/ImportInventoryDialog/Details", saveState());
}

bool ImportInventoryDialog::setItem(const BrickLink::Item *item)
{
    return w_select->setCurrentItem(item);
}

const BrickLink::Item *ImportInventoryDialog::item() const
{
    return w_select->currentItem();
}

int ImportInventoryDialog::quantity() const
{
    return qMax(1, w_qty->value());
}

BrickLink::Condition ImportInventoryDialog::condition() const
{
    return w_condition_used->isChecked() ? BrickLink::Condition::Used : BrickLink::Condition::New;
}

BrickLink::Status ImportInventoryDialog::extraParts() const
{
    return static_cast<BrickLink::Status>(w_extra->currentIndex());
}

bool ImportInventoryDialog::includeInstructions() const
{
    return !w_instructions->isHidden()
            && w_instructions->isEnabled()
            && w_instructions->isChecked();
}

void ImportInventoryDialog::languageChange()
{
    retranslateUi(this);
    w_import->setText(tr("Import"));
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
    w_select->setFocus();
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
        if (w_import->isVisible() && w_import->isEnabled())
            w_import->animateClick();
        return;
    }

    QWidget::keyPressEvent(e);
}


QSize ImportInventoryDialog::sizeHint() const
{
    QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance("m") * 120, fm.height() * 30);
}

QByteArray ImportInventoryDialog::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("II") << qint32(2)
       << w_condition_new->isChecked()
       << qint32(w_qty->value())
       << qint32(w_extra->currentIndex())
       << w_instructions->isChecked();
    return ba;
}

bool ImportInventoryDialog::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "II") || (version < 1) || (version > 2))
        return false;

    bool isNew;
    qint32 qty;

    ds >> isNew >> qty;

    if (ds.status() != QDataStream::Ok)
        return false;

    w_condition_new->setChecked(isNew);
    w_condition_used->setChecked(!isNew);
    w_qty->setValue(qty);

    if (version >= 2) {
        qint32 extras;
        bool instructions;

        ds >> extras >> instructions;

        if (ds.status() != QDataStream::Ok)
            return false;

        w_instructions->setChecked(instructions);
        w_extra->setCurrentIndex(extras);
    }

    return true;
}

void ImportInventoryDialog::checkItem(const BrickLink::Item *it, bool ok)
{
    w_import->setEnabled((it));

    if (it && it->itemType() && (it->itemType()->id() == 'S')) {
        bool hasInstructions = (BrickLink::core()->item('I', it->id()));
        w_instructions->setEnabled(hasInstructions);
    }
    if (ok)
        w_import->animateClick();
}

void ImportInventoryDialog::importInventory()
{
    if (auto doc = DocumentIO::importBrickLinkInventory(item(), quantity(), condition(),
                                                        extraParts(), includeInstructions())) {
        FrameWork::inst()->createWindow(doc);
    }
}

#include "moc_importinventorydialog.cpp"
