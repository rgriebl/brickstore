// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPushButton>
#include <QHeaderView>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QPalette>
#include <QVariant>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QShortcut>
#include <QKeyEvent>
#include <QTimer>
#include <QMessageBox>

#include "bricklink/wantedlist.h"
#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/config.h"
#include "common/humanreadabletimedelta.h"
#include "betteritemdelegate.h"
#include "historylineedit.h"
#include "importwantedlistdialog.h"

using namespace std::chrono_literals;


ImportWantedListDialog::ImportWantedListDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_update->setProperty("iconScaling", true);

    w_wantedLists->header()->setStretchLastSection(false);
    auto proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSortLocaleAware(true);
    proxyModel->setSortRole(BrickLink::WantedLists::WantedListSortRole);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(-1);
    proxyModel->setSourceModel(BrickLink::core()->wantedLists());
    w_wantedLists->setModel(proxyModel);
    w_wantedLists->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    w_wantedLists->setItemDelegate(new BetterItemDelegate(BetterItemDelegate::AlwaysShowSelection
                                                          | BetterItemDelegate::MoreSpacing, w_wantedLists));

    connect(w_filter, &HistoryLineEdit::textChanged,
            proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated,
            this, [this]() { w_filter->setFocus(); });

    w_import = new QPushButton();
    w_import->setDefault(true);
    w_buttons->addButton(w_import, QDialogButtonBox::ActionRole);
    connect(w_import, &QAbstractButton::clicked,
            this, [this]() { importWantedLists(w_wantedLists->selectionModel()->selectedRows()); });
    w_showOnBrickLink = new QPushButton();
    w_showOnBrickLink->setIcon(QIcon::fromTheme(u"bricklink"_qs));
    w_buttons->addButton(w_showOnBrickLink, QDialogButtonBox::ActionRole);
    connect(w_showOnBrickLink, &QAbstractButton::clicked,
            this, &ImportWantedListDialog::showWantedListsOnBrickLink);

    connect(w_update, &QToolButton::clicked,
            this, &ImportWantedListDialog::updateWantedLists);
    connect(w_wantedLists->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportWantedListDialog::checkSelected);
    connect(w_wantedLists, &QTreeView::activated,
            this, [this]() { w_import->animateClick(); });
    connect(BrickLink::core(), &BrickLink::Core::authenticatedTransferOverallProgress,
            this, [this](int done, int total) {
        w_progress->setVisible(done != total);
        w_progress->setMaximum(total);
        w_progress->setValue(done);
    });
    connect(BrickLink::core()->wantedLists(), &BrickLink::WantedLists::updateFinished,
            this, [this](bool success, const QString &message) {
        Q_UNUSED(success)

        w_update->setEnabled(true);
        w_wantedLists->setEnabled(true);

        m_updateMessage = message;
        updateStatusLabel();

        if (BrickLink::core()->wantedLists()->rowCount()) {
            auto tl = w_wantedLists->model()->index(0, 0);
            w_wantedLists->selectionModel()->select(tl, QItemSelectionModel::SelectCurrent
                                                    | QItemSelectionModel::Rows);
            w_wantedLists->scrollTo(tl);
        }
        w_wantedLists->header()->resizeSections(QHeaderView::ResizeToContents);
        w_wantedLists->setFocus();

        checkSelected();
    });

    languageChange();

    auto t = new QTimer(this);
    t->setInterval(30s);
    connect(t, &QTimer::timeout, this,
            &ImportWantedListDialog::updateStatusLabel);
    t->start();

    QMetaObject::invokeMethod(this, &ImportWantedListDialog::updateWantedLists, Qt::QueuedConnection);

    auto ba = Config::inst()->value(u"MainWindow/ImportWantedListDialog/Filter"_qs).toByteArray();
    if (!ba.isEmpty())
        w_filter->restoreState(ba);
    ba = Config::inst()->value(u"MainWindow/ImportWantedListDialog/ListState"_qs).toByteArray();
    if (!ba.isEmpty())
        w_wantedLists->header()->restoreState(ba);

    setFocusProxy(w_filter);
}

ImportWantedListDialog::~ImportWantedListDialog()
{
    Config::inst()->setValue(u"MainWindow/ImportWantedListDialog/Filter"_qs, w_filter->saveState());
    Config::inst()->setValue(u"MainWindow/ImportWantedListDialog/ListState"_qs, w_wantedLists->header()->saveState());
}

void ImportWantedListDialog::keyPressEvent(QKeyEvent *e)
{
    // simulate QDialog behavior
    if (e->matches(QKeySequence::Cancel)) {
        close();
        return;
    } else if ((!e->modifiers() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))
               || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter))) {
        // we need the animateClick here instead of triggering directly: otherwise we
        // get interference from the QTreeView::activated signal, resulting in double triggers
        if (w_import->isVisible() && w_import->isEnabled())
            w_import->animateClick();
        return;
    }

    QWidget::keyPressEvent(e);
}

void ImportWantedListDialog::showEvent(QShowEvent *e)
{
    auto ba = Config::inst()->value(u"MainWindow/ImportWantedListDialog/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    QDialog::showEvent(e);
    activateWindow();
}

void ImportWantedListDialog::closeEvent(QCloseEvent *e)
{
    Config::inst()->setValue(u"MainWindow/ImportWantedListDialog/Geometry"_qs, saveGeometry());
    QDialog::closeEvent(e);
}

void ImportWantedListDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QDialog::changeEvent(e);
}

void ImportWantedListDialog::languageChange()
{
    retranslateUi(this);

    w_import->setText(tr("Import"));
    w_filter->setToolTip(ActionManager::toolTipLabel(tr("Filter the list for lines containing these words"),
                                                     QKeySequence::Find, w_filter->instructionToolTip()));
    w_showOnBrickLink->setText(tr("Show"));
    w_showOnBrickLink->setToolTip(tr("Show on BrickLink"));
    updateStatusLabel();
}

void ImportWantedListDialog::updateWantedLists()
{
    if (BrickLink::core()->wantedLists()->updateStatus() == BrickLink::UpdateStatus::Updating)
        return;

    m_updateMessage.clear();
    w_update->setEnabled(false);
    w_import->setEnabled(false);
    w_wantedLists->setEnabled(false);

    BrickLink::core()->wantedLists()->startUpdate();
    updateStatusLabel();
}


void ImportWantedListDialog::importWantedLists(const QModelIndexList &rows)
{
    for (auto idx : rows) {
        auto wantedList = idx.data(BrickLink::WantedLists::WantedListPointerRole).value<BrickLink::WantedList *>();

        BrickLink::core()->wantedLists()->startFetchLots(wantedList);
    }
}

void ImportWantedListDialog::showWantedListsOnBrickLink()
{
    const auto selection = w_wantedLists->selectionModel()->selectedRows();
    for (auto idx : selection) {
        auto wantedList = idx.data(BrickLink::WantedLists::WantedListPointerRole).value<BrickLink::WantedList *>();
        Application::openUrl(BrickLink::Core::urlForWantedList(wantedList->id()));
    }
}

void ImportWantedListDialog::checkSelected()
{
    bool b = w_wantedLists->selectionModel()->hasSelection();
    w_import->setEnabled(b);
    w_showOnBrickLink->setEnabled(b);
}

void ImportWantedListDialog::updateStatusLabel()
{
    QString s;

    switch (BrickLink::core()->wantedLists()->updateStatus()) {
    case BrickLink::UpdateStatus::Ok:
        s = tr("Last updated %1").arg(
                    HumanReadableTimeDelta::toString(QDateTime::currentDateTime(),
                                                     BrickLink::core()->wantedLists()->lastUpdated()));
        break;

    case BrickLink::UpdateStatus::Updating:
        s = tr("Currently updating wanted lists");
        break;

    case BrickLink::UpdateStatus::UpdateFailed:
        s = tr("Last update failed") + u": " + m_updateMessage;
        break;

    default:
        break;
    }
    w_lastUpdated->setText(s);
}

#include "moc_importwantedlistdialog.cpp"
