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
#include <QClipboard>
#include <QListWidget>
#include <QRadioButton>
#include <QToolButton>
#include <QGridLayout>
#include <QWizard>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QBoxLayout>

#include <QGroupBox>
#include <QCheckBox>
#include <QButtonGroup>

#include "document.h"
#include "selectdocumentdialog.h"


SelectDocument::SelectDocument(const Document *self, QWidget *parent)
    : QWidget(parent)
{
    m_clipboard = new QRadioButton(tr("Items from Clipboard"));
    m_document = new QRadioButton(tr("Items from an already open document:"));
    m_documentList = new QListWidget();

    auto layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setColumnMinimumWidth(0, 20);
    layout->setColumnStretch(1, 100);
    layout->setRowStretch(2, 100);
    layout->addWidget(m_clipboard, 0, 0, 1, 2);
    layout->addWidget(m_document, 1, 0, 1, 2);
    layout->addWidget(m_documentList, 2, 1, 1, 1);

    m_itemsFromClipboard = BrickLink::InvItemMimeData::items(QApplication::clipboard()->mimeData());

    foreach (const Document *doc, Document::allDocuments()) {
        if (doc != self) {
            QListWidgetItem *item = new QListWidgetItem(doc->fileNameOrTitle(), m_documentList);
            item->setData(Qt::UserRole, QVariant::fromValue(doc));
        }
    }

    bool hasClip = !m_itemsFromClipboard.isEmpty();
    bool hasDocs = m_documentList->count() > 0;

    m_clipboard->setEnabled(hasClip);
    m_document->setEnabled(hasDocs);
    m_documentList->setEnabled(hasDocs);

    if (hasDocs) {
        m_document->setChecked(true);
        m_documentList->setCurrentRow(0);
    } else if (hasClip) {
        m_clipboard->setChecked(true);
    }

    auto emitSelected = [this]() { emit documentSelected(isDocumentSelected()); };

    connect(m_clipboard, &QAbstractButton::toggled,
            this, [emitSelected]() { emitSelected(); });
    connect(m_documentList, &QListWidget::itemSelectionChanged,
            this, [emitSelected]() { emitSelected(); });

    QMetaObject::invokeMethod(this, emitSelected, Qt::QueuedConnection);
}

BrickLink::InvItemList SelectDocument::items() const
{
    BrickLink::InvItemList list;

    if (m_clipboard->isChecked()) {
        for (const BrickLink::InvItem *item : m_itemsFromClipboard)
            list.append(new BrickLink::InvItem(*item));
    } else {
        if (!m_documentList->selectedItems().isEmpty()) {
            const auto *doc = m_documentList->selectedItems().constFirst()
                    ->data(Qt::UserRole).value<const Document *>();
            if (doc) {
                const auto items = doc->items();
                for (const Document::Item *item : items)
                    list.append(new BrickLink::InvItem(*item));
            }
        }
    }
    return list;
}

SelectDocument::~SelectDocument()
{
    qDeleteAll(m_itemsFromClipboard);
}

bool SelectDocument::isDocumentSelected() const
{
    return m_clipboard->isChecked() || !m_documentList->selectedItems().isEmpty();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SelectDocumentDialog::SelectDocumentDialog(const Document *self, const QString &headertext,
                                           QWidget *parent)
    : QDialog(parent)
{
    m_sd = new SelectDocument(self);

    auto label = new QLabel(headertext);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_sd);
    layout->addWidget(buttons);

    connect(m_sd, &SelectDocument::documentSelected,
            m_ok, &QAbstractButton::setEnabled);
}

BrickLink::InvItemList SelectDocumentDialog::items() const
{
    return m_sd->items();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SelectMergeMode::SelectMergeMode(Document::MergeMode defaultMode, QWidget *parent)
    : QWidget(parent)
{
    createFields(this);
    m_allGroup->button(int(defaultMode))->animateClick();
}

Document::MergeMode SelectMergeMode::defaultMergeMode() const
{
    return Document::MergeMode::Ignore;
}

QHash<Document::Field, Document::MergeMode> SelectMergeMode::fieldMergeModes() const
{
    QHash<Document::Field, Document::MergeMode> mergeModes;
    auto dmm = defaultMergeMode();

    for (QButtonGroup *bg : m_groups) {
        auto mm = Document::MergeMode(bg->checkedId());
        if (mm != dmm) {
            const auto fields = bg->property("documentField").value<QVector<int>>();
            for (const auto &field : fields)
                mergeModes.insert(Document::Field(field), mm);
        }
    }
    return mergeModes;
}


void SelectMergeMode::createFields(QWidget *parent)
{
    static const struct {
        Document::MergeMode mergeMode;
        const char *name;
        const char *toolTip;
        const char *icon;
    } modes[] = {
    { Document::MergeMode::Ignore,
                QT_TR_NOOP("Ignore"),
                QT_TR_NOOP("Leave the destination value as is"),
                "process-stop", },
    { Document::MergeMode::Copy,
                QT_TR_NOOP("Copy"),
                QT_TR_NOOP("Set to source value"),
                "edit-copy", },
    { Document::MergeMode::Merge,
                QT_TR_NOOP("Merge"),
                QT_TR_NOOP("Set to source value, but only if destination is at default value"),
                "join", },
    { Document::MergeMode::MergeText,
                QT_TR_NOOP("Merge text"),
                QT_TR_NOOP("Merge the text from the source and the destination"),
                "edit-select-text", },
    { Document::MergeMode::MergeAverage,
                QT_TR_NOOP("Merge average"),
                QT_TR_NOOP("Merge field by calculating a quantity average"),
                "taxes-finances", },
    };

    static const struct {
        const char *name;
        QVector<int> fields;
        Document::MergeModes mergeModes;

    } fields[] = {
    { QT_TR_NOOP("Price"),           { Document::Price },
        Document::MergeMode::Copy | Document::MergeMode::Merge | Document::MergeMode::MergeAverage },
    { QT_TR_NOOP("Cost"),            { Document::Cost },
        Document::MergeMode::Copy | Document::MergeMode::Merge | Document::MergeMode::MergeAverage },
    { QT_TR_NOOP("Tier prices"),     { Document::TierP1, Document::TierP2, Document::TierP3,
                                       Document::TierQ1, Document::TierQ2, Document::TierQ3 },
        Document::MergeMode::Copy | Document::MergeMode::Merge | Document::MergeMode::MergeAverage },
    { QT_TR_NOOP("Quantity"),        { Document::Quantity },
        Document::MergeMode::Copy | Document::MergeMode::Merge },
    { QT_TR_NOOP("Bulk quantity"),   { Document::Bulk },
        Document::MergeMode::Copy | Document::MergeMode::Merge },
    { QT_TR_NOOP("Sale percentage"), { Document::Sale },
        Document::MergeMode::Copy | Document::MergeMode::Merge },
    { QT_TR_NOOP("Comment"),         { Document::Comments },
        Document::MergeMode::Copy | Document::MergeMode::Merge | Document::MergeMode::MergeText },
    { QT_TR_NOOP("Remark"),          { Document::Remarks },
        Document::MergeMode::Copy | Document::MergeMode::Merge | Document::MergeMode::MergeText },
    { QT_TR_NOOP("Reserved"),        { Document::Reserved },
        Document::MergeMode::Copy | Document::MergeMode::Merge },
    { QT_TR_NOOP("Retain flag"),     { Document::Retain },
        Document::MergeMode::Copy },
    { QT_TR_NOOP("Stockroom"),       { Document::Stockroom },
        Document::MergeMode::Copy },
    };

    auto grid = new QGridLayout(parent);
    grid->setColumnStretch(6, 100);
    int row = 0;
    int col = 0;

    auto label = new QLabel(tr("All fields"));
    grid->addWidget(label, row, 0);
    m_allGroup = new QButtonGroup();
    for (const auto &mode : modes) {
        auto tb = new QToolButton();
        tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tb->setIcon(QIcon::fromTheme(mode.icon));
        tb->setText(tr(mode.name));
        tb->setToolTip(tr(mode.toolTip));
        tb->setAutoRaise(true);
        grid->addWidget(tb, row, col + 1);
        m_allGroup->addButton(tb, int(mode.mergeMode));
        ++col;
    }
    connect(m_allGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, [this](int mergeMode) {
        for (QButtonGroup *group : qAsConst(m_groups)) {
            int m = mergeMode;
            while (true) {
                if (auto *b = group->button(m)) {
                    b->setChecked(true);
                    break;
                } else {
                    m >>= 1;
                }
            }
        }
        emit mergeModesChanged(mergeMode != int(Document::MergeMode::Ignore));
    });

    ++row;
    auto hbar = new QFrame();
    hbar->setFrameStyle(QFrame::HLine);
    grid->addWidget(hbar, row, 0, 1, 6);

    ++row;
    col = 0;

    for (const auto &field : fields) {
        label = new QLabel(tr(field.name));
        grid->addWidget(label, row, 0);
        auto group = new QButtonGroup();
        group->setProperty("documentField", QVariant::fromValue(field.fields));
        for (const auto &mode : modes) {
            if ((mode.mergeMode == Document::MergeMode::Ignore)
                    || (field.mergeModes & mode.mergeMode)) {
                auto tb = new QToolButton();
                tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                tb->setIcon(QIcon::fromTheme(mode.icon));
                tb->setText(tr(mode.name));
                tb->setToolTip(tr(mode.toolTip));
                tb->setCheckable(true);
                tb->setAutoRaise(true);
                grid->addWidget(tb, row, col + 1);
                group->addButton(tb, int(mode.mergeMode));
            }
            ++col;
        }
        m_groups.append(group);

        connect(group, QOverload<int>::of(&QButtonGroup::buttonClicked),
                this, [this]() {
            bool ignoreAll = true;
            for (QButtonGroup *bg : qAsConst(m_groups))
                ignoreAll = ignoreAll && (bg->checkedId() == int(Document::MergeMode::Ignore));
            emit mergeModesChanged(!ignoreAll);
        });

        ++row;
        col = 0;
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

class WizardPage : public QWizardPage
{
    Q_OBJECT
public:
    WizardPage() = default;

    void setComplete(bool b)
    {
        if (m_completed != b) {
            m_completed = b;
            emit completeChanged();
        }
    }

    bool isComplete() const override;

private:
    bool m_completed = false;
};

bool WizardPage::isComplete() const
{
    return m_completed;
}


SelectCopyMergeDialog::SelectCopyMergeDialog(const Document *self, const QString &chooseDocText,
                                             const QString &chooseFieldsText, QWidget *parent)
    : QWizard(parent)
{
    setOptions(QWizard::IndependentPages);
#ifdef Q_OS_WIN
    setWizardStyle(QWizard::ModernStyle);
#endif
    QString title = tr("Copy or merge values");

    m_sd = new SelectDocument(self);
    m_mm = new SelectMergeMode(Document::MergeMode::Merge);

    auto *dpage = new WizardPage();
    dpage->setTitle(title);
    dpage->setSubTitle(chooseDocText);
    auto *dlayout = new QVBoxLayout(dpage);
    dlayout->addWidget(m_sd);
    addPage(dpage);

    auto *mpage = new WizardPage();
    mpage->setTitle(title);
    mpage->setSubTitle(chooseFieldsText);
    mpage->setFinalPage(true);
    auto *mlayout = new QVBoxLayout(mpage);
    mlayout->addWidget(m_mm);
    addPage(mpage);

    dpage->adjustSize();
    mpage->adjustSize();
    QSize s = mpage->size().expandedTo(dpage->size());
    dpage->setFixedSize(s);
    mpage->setFixedSize(s);

    connect(m_sd, &SelectDocument::documentSelected,
            dpage, &WizardPage::setComplete);
    connect(m_mm, &SelectMergeMode::mergeModesChanged,
            mpage, &WizardPage::setComplete);
}

Document::ItemList SelectCopyMergeDialog::items() const
{
    return m_sd->items();
}

Document::MergeMode SelectCopyMergeDialog::defaultMergeMode() const
{
    return m_mm->defaultMergeMode();
}

QHash<Document::Field, Document::MergeMode> SelectCopyMergeDialog::fieldMergeModes() const
{
    return m_mm->fieldMergeModes();
}

void SelectCopyMergeDialog::showEvent(QShowEvent *e)
{
    setFixedSize(sizeHint());
    QWizard::showEvent(e);
}


#include "moc_selectdocumentdialog.cpp"
#include "selectdocumentdialog.moc"
