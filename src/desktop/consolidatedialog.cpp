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
#include <QWindow>
#include <QScreen>
#include <QScrollBar>
#include <QStringListModel>
#include <QStringBuilder>
#include <QMessageBox>

#include "common/config.h"
#include "consolidatedialog.h"
#include "documentdelegate.h"
#include "headerview.h"
#include "view.h"


enum PageIds {
    StartPage,
    DefaultsPage,
    IndividualPage
};

QString ConsolidateDialog::s_baseConfigPath = u"/MainWindow/ConsolidateDialog/"_qs;


ConsolidateDialog::ConsolidateDialog(View *view, QVector<DocumentModel::Consolidate> &list, bool addItems)
    : QWizard(view)
    , m_addingItems(addItems)
    , m_list(list)
{
    m_documentLots = view->model()->lots();
    m_documentSortedLots = view->model()->sortedLots();

    setupUi(this);

    startPage->setFocusProxy(w_forAll);

    w_forAll->setIcon(QIcon::fromTheme(u"vcs-normal"_qs));
    w_individual->setIcon(QIcon::fromTheme(u"vcs-conflicting"_qs));
    w_justAdd->setIcon(QIcon::fromTheme(u"vcs-added"_qs));

    QString title = tr("There are are %n possible consolidation(s)", nullptr, list.size());
    startPage->setTitle(title);
    defaultsPage->setTitle(title);

    auto *headerView = new HeaderView(Qt::Horizontal, w_individualDestination);
    w_individualDestination->setHorizontalHeader(headerView);

    w_individualDestination->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    w_individualDestination->setContextMenuPolicy(Qt::NoContextMenu);

    individualPage->setFocusProxy(w_individualDestination);

    auto *dd = new DocumentDelegate(w_individualDestination);
    dd->setReadOnly(true);
    w_individualDestination->setItemDelegate(dd);
    w_individualDestination->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    int rowHeight = DocumentDelegate::defaultItemHeight(w_individualDestination);
    w_individualDestination->verticalHeader()->setDefaultSectionSize(rowHeight);

    int headerHeight; // QHeader has a 0 sizeHint without a model, so we need to fake one
    {
        QStringListModel dummy;
        w_individualDestination->setModel(&dummy);
        headerHeight = w_individualDestination->horizontalHeader()->sizeHint().height();
        w_individualDestination->setModel(nullptr);
    }

    float rowsToShow = 2.15f;
    int listHeight = int(rowsToShow * rowHeight)
            + headerHeight
            + w_individualDestination->horizontalScrollBar()->sizeHint().height()
            + w_individualDestination->frameWidth() * 2;
    w_individualDestination->setMinimumHeight(listHeight);

    headerView->restoreState(view->headerView()->saveState());
    headerView->showSection(DocumentModel::Index);
    if (headerView->visualIndex(DocumentModel::Index) != 0)
        headerView->moveSection(headerView->visualIndex(DocumentModel::Index), 0);
    for (auto field : { DocumentModel::Category, DocumentModel::ItemType,
         DocumentModel::TotalWeight, DocumentModel::YearReleased, DocumentModel::Weight }) {
        headerView->hideSection(field);
    }

    w_individualMoreOptions->setExpandingWidget(w_individualFieldMergeModes);
    w_individualMoreOptions->setResizeTopLevelOnExpand(true);

    w_defaultFieldMergeModes->setQuantityEnabled(false);
    w_individualFieldMergeModes->setQuantityEnabled(false);

    if (addItems) {
        w_defaultDestination->addItem(tr("Existing Item"), QVariant::fromValue(Destination::IntoExisting));
        w_defaultDestination->addItem(tr("New Item"), QVariant::fromValue(Destination::IntoNew));

        w_defaultDoNotDeleteEmpty->hide();
        w_individualDoNotDeleteEmpty->hide();
    } else {
        w_defaultDestination->addItem(tr("Topmost in Sort Order"), QVariant::fromValue(Destination::IntoTopSorted));
        w_defaultDestination->addItem(tr("Bottommost in Sort Order"), QVariant::fromValue(Destination::IntoBottomSorted));
        w_defaultDestination->addItem(tr("Lowest Index"), QVariant::fromValue(Destination::IntoLowestIndex));
        w_defaultDestination->addItem(tr("Highest Index"), QVariant::fromValue(Destination::IntoHighestIndex));

        w_justAddSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        w_justAdd->hide();
    }

    connect(w_forAll, &BetterCommandButton::clicked, this, [=]() {
        m_forAll = true;
        next();
    });
    connect(w_individual, &BetterCommandButton::clicked, this, [=]() {
        m_forAll = false;
        next();
    });
    connect(w_justAdd, &BetterCommandButton::clicked, this, [=]() {
        accept();
    });
    connect(this, &QWizard::currentIdChanged,
            this, &ConsolidateDialog::initializeCurrentPage);

    // QWizard wasn't really made to do what we are doing here, so we have to work around
    // a few concepts in this class.

    setButtonText(CustomButton1, buttonText(BackButton));   // custom back
    setButtonText(CustomButton2, buttonText(NextButton));   // custom next
    setButtonText(CustomButton3, buttonText(FinishButton)); // custom finish

    connect(w_individualDestination, &QTableView::activated,
            this, [this](const QModelIndex &) {
        auto b = button((m_individualIdx < (m_list.size() - 1)) ? CustomButton2 : CustomButton3);
        b->animateClick();
    });

    connect(this, &QWizard::customButtonClicked,
            this, [=](int which) {
        validateCurrentPage();

        switch (which) {
        case CustomButton3: { // finish
            int unmerged = std::count_if(m_list.cbegin(), m_list.cend(), [](const auto &c) {
                return c.destinationIndex < 0;
            });

            if (!unmerged) {
                accept();
            } else {
                QString question = tr("%1 of %2 consolidations will not be done, because no destination lot has been set.")
                        .arg(unmerged).arg(m_list.size())
                        % u"<br><br>"
                        % tr("Do you still want to consolidate the rest?");
                if (QMessageBox::question(this, tr("Consolidate"), question) == QMessageBox::Yes)
                    accept();
            }
            return;
        }
        case CustomButton1: // back
            m_individualIdx--;
            break;
        case CustomButton2: // next;
            m_individualIdx++;
            break;
        }
        showIndividualMerge(m_individualIdx);
    });

    startPage->adjustSize();
    defaultsPage->adjustSize();

    QSize s = startPage->size().expandedTo(defaultsPage->size());

    defaultsPage->resize(s);
    startPage->setMinimumSize(s);

    // The geometry is the same for both the "add" and "consolidate" modes.
    // All the other settings are saved individually.

    QByteArray ba = Config::inst()->value(s_baseConfigPath % u"Geometry").toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
}

ConsolidateDialog::~ConsolidateDialog()
{
    Config::inst()->setValue(s_baseConfigPath % u"Geometry", saveGeometry());
}

int ConsolidateDialog::nextId() const
{
    switch (currentId()) {
    case StartPage:
        return DefaultsPage;

    case DefaultsPage:
        return m_forAll ? -1 : IndividualPage;

    case IndividualPage:
    default:
        return -1;
    }
}

void ConsolidateDialog::initializeCurrentPage()
{
    switch (currentId()) {
    case StartPage:
        setButtonLayout({ Stretch, CancelButton });
        break;

    case DefaultsPage: {
        if (m_forAll) {
            setButtonLayout({ Stretch, BackButton, FinishButton, CancelButton });
            defaultsPage->setSubTitle(tr("These options are used to consolidate all lots"));

            if (w_defaultDestination->itemData(0).value<Destination>() == Destination::Not)
                w_defaultDestination->removeItem(0);
            w_defaultDestination->setCurrentIndex(0);
            m_individualIdx = -1;
        } else {
            setButtonLayout({ Stretch, BackButton, NextButton, FinishButton, CancelButton });
            defaultsPage->setSubTitle(tr("These options are used as defaults for each lot consolidation"));

            if (w_defaultDestination->itemData(0).value<Destination>() != Destination::Not)
                w_defaultDestination->insertItem(0, tr("No preselection"), QVariant::fromValue(Destination::Not));
            w_defaultDestination->setCurrentIndex(0);
            m_individualIdx = 0;
        }
        button(FinishButton)->setEnabled(m_forAll);

        // load default settings

        const QString configPath = s_baseConfigPath
                % (m_addingItems ? u"Add/" : u"Consolidate/");

        const auto destinationDefault = m_addingItems ? Destination::IntoExisting
                                                      : Destination::IntoTopSorted;

        const auto destination = static_cast<Destination>(
                    Config::inst()->value(configPath % u"Destination/" % (m_forAll ? u"ForAll/" : u"Individual/"),
                                          int(destinationDefault)).toInt());

        int destinationIndex = w_defaultDestination->findData(QVariant::fromValue(destination));
        if (destinationIndex < 0)
            destinationIndex = w_defaultDestination->findData(QVariant::fromValue(destinationDefault));
        w_defaultDestination->setCurrentIndex(destinationIndex < 0 ? 0 : destinationIndex);

        QByteArray ba = Config::inst()->value(configPath % u"FieldMergeModes").toByteArray();
        if (ba.isEmpty() || !w_defaultFieldMergeModes->restoreState(ba)) {
            w_defaultFieldMergeModes->setFieldMergeModes(DocumentModel::createFieldMergeModes(
                                                             DocumentModel::MergeMode::MergeAverage));
        }

        if (!m_addingItems) {
            w_defaultDoNotDeleteEmpty->setChecked(
                        Config::inst()->value(configPath % u"DoNotDeleteEmpty", false).toBool());
        }
        break;
    }
    case IndividualPage:
        setButtonLayout({ Stretch, CustomButton1, CustomButton2, CustomButton3, CancelButton });

        m_individualIdx = 0;
        showIndividualMerge(m_individualIdx);
        break;
    }
}

bool ConsolidateDialog::validateCurrentPage()
{
    switch (currentId()) {
    case StartPage:
        return true;

    case DefaultsPage: {
        m_fieldMergeModes = w_defaultFieldMergeModes->fieldMergeModes();
        m_doNotDeleteEmpty = w_defaultDoNotDeleteEmpty->isVisible()
                && w_defaultDoNotDeleteEmpty->isChecked();
        m_destination = w_defaultDestination->currentData().value<Destination>();

        for (int i = 0; i < m_list.size(); ++i) {
            DocumentModel::Consolidate &c = m_list[i];

            c.fieldMergeModes = m_fieldMergeModes;
            c.doNotDeleteEmpty = m_doNotDeleteEmpty;
            c.destinationIndex = calculateIndex(c, m_destination);
        }

        // save default settings

        QString configPath = s_baseConfigPath
                % (m_addingItems ? u"Add/" : u"Consolidate/");

        Config::inst()->setValue(configPath % u"Destination/" % (m_forAll ? u"ForAll/" : u"Individual/"),
                                 int(w_defaultDestination->currentData().value<Destination>()));
        Config::inst()->setValue(configPath % u"FieldMergeModes", w_defaultFieldMergeModes->saveState());
        if (!m_addingItems)
            Config::inst()->setValue(configPath % u"DoNotDeleteEmpty", w_defaultDoNotDeleteEmpty->isChecked());

        return true;
    }

    case IndividualPage: {
        DocumentModel::Consolidate &c = m_list[m_individualIdx];

        c.fieldMergeModes = w_individualFieldMergeModes->fieldMergeModes();
        c.doNotDeleteEmpty = w_individualDoNotDeleteEmpty->isVisible()
                && w_individualDoNotDeleteEmpty->isChecked();
        c.destinationIndex = w_individualDestination->selectionModel()->hasSelection() ?
                    w_individualDestination->selectionModel()->selectedRows().constFirst().row() : -1;
        return true;
    }

    default:
        return false;
    }
}

void ConsolidateDialog::showIndividualMerge(int idx)
{
    // can't go back to the default options and can't go past the last merge
    button(CustomButton1)->setEnabled(m_individualIdx > 0);
    button(CustomButton2)->setEnabled(m_individualIdx < (m_list.size() - 1));

    const DocumentModel::Consolidate &c = m_list.at(idx);

    auto destination = w_defaultDestination->currentData().value<Destination>();

    QVector<int> fakeIndexes;
    for (const auto lot : c.lots)
        fakeIndexes << m_documentLots.indexOf(lot);

    DocumentModel *docModel = DocumentModel::createTemporary(c.lots, fakeIndexes);
    docModel->setParent(this);
    w_individualDestination->setModel(docModel);

    int selectedIndex = (c.destinationIndex >= 0) ? c.destinationIndex : calculateIndex(c, destination);

    if (selectedIndex < 0) {
        w_individualDestination->clearSelection();
    } else {
        w_individualDestination->selectionModel()->setCurrentIndex(docModel->index(selectedIndex, 0),
                                                                   QItemSelectionModel::SelectCurrent
                                                                   | QItemSelectionModel::Rows);
    }
    w_individualDoNotDeleteEmpty->setChecked(c.doNotDeleteEmpty);
    w_individualFieldMergeModes->setFieldMergeModes(c.fieldMergeModes);

    individualPage->setTitle(tr("Consolidation %1 of %2").arg(m_individualIdx + 1).arg(m_list.size()));
    individualPage->setSubTitle(tr("Select the destination lot (from %1 source lots) and adjust the options if needed").arg(docModel->rowCount()));

    w_individualDestination->setFocus();
}

int ConsolidateDialog::calculateIndex(const DocumentModel::Consolidate &consolidate,
                                      Destination destination)
{
    switch (destination) {
    case Destination::IntoExisting:
        return 0;

    case Destination::IntoNew:
        return consolidate.lots.size() - 1;

    case Destination::IntoTopSorted:
    case Destination::IntoBottomSorted:
    case Destination::IntoHighestIndex:
    case Destination::IntoLowestIndex: {
        std::vector<int> docIndexes;
        docIndexes.reserve(consolidate.lots.size());
        std::vector<int> sortIndexes;
        sortIndexes.reserve(consolidate.lots.size());

        for (const auto lot : std::as_const(consolidate.lots)) {
            docIndexes.push_back(m_documentLots.indexOf(lot));
            sortIndexes.push_back(m_documentSortedLots.indexOf(lot));
        }
        auto [minDocIt, maxDocIt] = std::minmax_element(docIndexes.cbegin(), docIndexes.cend());
        auto [minSortIt, maxSortIt] = std::minmax_element(sortIndexes.cbegin(), sortIndexes.cend());

        switch (destination) {
        case Destination::IntoTopSorted:
            return std::distance(sortIndexes.cbegin(), minSortIt);
        case Destination::IntoBottomSorted:
            return std::distance(sortIndexes.cbegin(), maxSortIt);
        case Destination::IntoHighestIndex:
            return std::distance(docIndexes.cbegin(), maxDocIt);
        case Destination::IntoLowestIndex:
            return std::distance(docIndexes.cbegin(), minDocIt);
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
    return -1;
}

#include "moc_consolidatedialog.cpp"
