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

#include "common/config.h"
#include "common/document.h"
#include "common/documentlist.h"
#include "utility/utility.h"
#include "selectdocumentdialog.h"


SelectDocument::SelectDocument(const DocumentModel *self, QWidget *parent)
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

    m_lotsFromClipboard = DocumentLotsMimeData::lots(QApplication::clipboard()->mimeData());

    const auto docs = DocumentList::inst()->documents();
    for (const Document *doc : docs) {
        auto model = doc->model();
        if (model != self) {
            QListWidgetItem *item = new QListWidgetItem(doc->filePathOrTitle(), m_documentList);
            item->setData(Qt::UserRole, QVariant::fromValue(model));
        }
    }

    bool hasClip = !m_lotsFromClipboard.empty();
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

LotList SelectDocument::lots() const
{
    LotList srcList;

    if (m_clipboard->isChecked()) {
        srcList = m_lotsFromClipboard;
    } else {
        if (!m_documentList->selectedItems().isEmpty()) {
            const auto *model = m_documentList->selectedItems().constFirst()
                    ->data(Qt::UserRole).value<DocumentModel *>();
            if (model)
                srcList = model->lots();
        }
    }

    LotList list;
    for (const Lot *lot : qAsConst(srcList))
        list << new Lot(*lot);
    return list;
}

SelectDocument::~SelectDocument()
{
    qDeleteAll(m_lotsFromClipboard);
}

bool SelectDocument::isDocumentSelected() const
{
    return m_clipboard->isChecked() || !m_documentList->selectedItems().isEmpty();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SelectDocumentDialog::SelectDocumentDialog(const DocumentModel *self, const QString &headertext,
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

LotList SelectDocumentDialog::lots() const
{
    return m_sd->lots();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SelectMergeMode::SelectMergeMode(DocumentModel::MergeMode defaultMode, QWidget *parent)
    : QWidget(parent)
{
    createFields(this);
    setDefaultMode(defaultMode);
}

DocumentModel::MergeMode SelectMergeMode::defaultMergeMode() const
{
    return DocumentModel::MergeMode::Ignore;
}

QHash<DocumentModel::Field, DocumentModel::MergeMode> SelectMergeMode::fieldMergeModes() const
{
    QHash<DocumentModel::Field, DocumentModel::MergeMode> mergeModes;
    auto dmm = defaultMergeMode();

    for (QButtonGroup *bg : m_groups) {
        auto mm = DocumentModel::MergeMode(bg->checkedId());
        if (mm != dmm) {
            const auto fields = bg->property("documentField").value<QVector<int>>();
            for (const auto &field : fields)
                mergeModes.insert(DocumentModel::Field(field), mm);
        }
    }
    return mergeModes;
}

QByteArray SelectMergeMode::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("SMM") << qint32(1);

    ds << qint32(m_groups.size());
    for (QButtonGroup *bg : m_groups)
        ds << qint32(bg->checkedId());

    return ba;
}

bool SelectMergeMode::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "SMM") || (version != 1))
        return false;

    qint32 size = -1;
    ds >> size;
    if (size != m_groups.size())
        return false;

    for (int i = 0; i < size; ++i) {
        qint32 mode = qint32(DocumentModel::MergeMode::Ignore);
        ds >> mode;
        if (auto *button = m_groups.at(i)->button(mode))
            button->setChecked(true);
    }

    if (ds.status() != QDataStream::Ok)
        return false;
    return true;
}


void SelectMergeMode::createFields(QWidget *parent)
{
    static const struct {
        DocumentModel::MergeMode mergeMode;
        const char *name;
        const char *toolTip;
        const char *icon;
    } modes[] = {
    { DocumentModel::MergeMode::Ignore,
                QT_TR_NOOP("Ignore"),
                QT_TR_NOOP("Leave the destination value as is"),
                "process-stop", },
    { DocumentModel::MergeMode::Copy,
                QT_TR_NOOP("Copy"),
                QT_TR_NOOP("Set to source value"),
                "edit-copy", },
    { DocumentModel::MergeMode::Merge,
                QT_TR_NOOP("Merge"),
                QT_TR_NOOP("Set to source value, but only if destination is at default value"),
                "join", },
    { DocumentModel::MergeMode::MergeText,
                QT_TR_NOOP("Merge text"),
                QT_TR_NOOP("Merge the text from the source and the destination"),
                "edit-select-text", },
    { DocumentModel::MergeMode::MergeAverage,
                QT_TR_NOOP("Merge average"),
                QT_TR_NOOP("Merge field by calculating a quantity average"),
                "taxes-finances", },
    };

    static const struct {
        const char *name;
        QVector<int> fields;
        DocumentModel::MergeModes mergeModes;

    } fields[] = {
    { QT_TR_NOOP("Price"),           { DocumentModel::Price },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge | DocumentModel::MergeMode::MergeAverage },
    { QT_TR_NOOP("Cost"),            { DocumentModel::Cost },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge | DocumentModel::MergeMode::MergeAverage },
    { QT_TR_NOOP("Tier prices"),     { DocumentModel::TierP1, DocumentModel::TierP2, DocumentModel::TierP3,
                                       DocumentModel::TierQ1, DocumentModel::TierQ2, DocumentModel::TierQ3 },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge | DocumentModel::MergeMode::MergeAverage },
    { QT_TR_NOOP("Quantity"),        { DocumentModel::Quantity },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge },
    { QT_TR_NOOP("Bulk quantity"),   { DocumentModel::Bulk },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge },
    { QT_TR_NOOP("Sale percentage"), { DocumentModel::Sale },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge },
    { QT_TR_NOOP("Comment"),         { DocumentModel::Comments },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge | DocumentModel::MergeMode::MergeText },
    { QT_TR_NOOP("Remark"),          { DocumentModel::Remarks },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge | DocumentModel::MergeMode::MergeText },
    { QT_TR_NOOP("Reserved"),        { DocumentModel::Reserved },
        DocumentModel::MergeMode::Copy | DocumentModel::MergeMode::Merge },
    { QT_TR_NOOP("Retain flag"),     { DocumentModel::Retain },
        DocumentModel::MergeMode::Copy },
    { QT_TR_NOOP("Stockroom"),       { DocumentModel::Stockroom },
        DocumentModel::MergeMode::Copy },
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
        tb->setIcon(QIcon::fromTheme(QLatin1String(mode.icon)));
        tb->setText(tr(mode.name));
        tb->setToolTip(tr(mode.toolTip));
        tb->setAutoRaise(true);
        grid->addWidget(tb, row, col + 1);
        m_allGroup->addButton(tb, int(mode.mergeMode));
        ++col;
    }

    connect(m_allGroup, &QButtonGroup::idClicked,
            this, [this](int id) {
        auto mergeMode = DocumentModel::MergeMode(id);
        setDefaultMode(mergeMode);
        emit mergeModesChanged(mergeMode != DocumentModel::MergeMode::Ignore);
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
            if ((mode.mergeMode == DocumentModel::MergeMode::Ignore)
                    || (field.mergeModes & mode.mergeMode)) {
                auto tb = new QToolButton();
                tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                tb->setIcon(QIcon::fromTheme(QLatin1String(mode.icon)));
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

        connect(group, &QButtonGroup::idClicked,
                this, [this]() {
            bool ignoreAll = true;
            for (QButtonGroup *bg : qAsConst(m_groups))
                ignoreAll = ignoreAll && (bg->checkedId() == int(DocumentModel::MergeMode::Ignore));
            emit mergeModesChanged(!ignoreAll);
        });

        ++row;
        col = 0;
    }
}

void SelectMergeMode::setDefaultMode(DocumentModel::MergeMode defaultMode)
{
    for (QButtonGroup *group : qAsConst(m_groups)) {
        int m = int(defaultMode);
        while (true) {
            if (auto *b = group->button(m)) {
                b->setChecked(true);
                break;
            } else {
                m >>= 1;
            }
        }
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


SelectCopyMergeDialog::SelectCopyMergeDialog(const DocumentModel *self, const QString &chooseDocText,
                                             const QString &chooseFieldsText, QWidget *parent)
    : QWizard(parent)
{
    setOptions(QWizard::IndependentPages);
    setWizardStyle(QWizard::ModernStyle);
    QString title = tr("Copy or merge values");

    m_sd = new SelectDocument(self);
    m_mm = new SelectMergeMode(DocumentModel::MergeMode::Merge);

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
    mpage->setComplete(true);
    auto *mlayout = new QVBoxLayout(mpage);
    mlayout->addWidget(m_mm);
    addPage(mpage);

    connect(m_sd, &SelectDocument::documentSelected,
            dpage, &WizardPage::setComplete);

    QByteArray ba = Config::inst()->value(u"/MainWindow/SelectCopyMergeDialog/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    dpage->adjustSize();
    mpage->adjustSize();
    QSize s = mpage->size().expandedTo(dpage->size());
    dpage->setFixedSize(s);
    mpage->setFixedSize(s);

    ba = Config::inst()->value(u"/MainWindow/SelectCopyMergeDialog/MergeMode"_qs)
            .toByteArray();
    m_mm->restoreState(ba);
}

SelectCopyMergeDialog::~SelectCopyMergeDialog()
{
    Config::inst()->setValue(u"/MainWindow/SelectCopyMergeDialog/Geometry"_qs, saveGeometry());
    Config::inst()->setValue(u"/MainWindow/SelectCopyMergeDialog/MergeMode"_qs, m_mm->saveState());
}


LotList SelectCopyMergeDialog::lots() const
{
    return m_sd->lots();
}

DocumentModel::MergeMode SelectCopyMergeDialog::defaultMergeMode() const
{
    return m_mm->defaultMergeMode();
}

QHash<DocumentModel::Field, DocumentModel::MergeMode> SelectCopyMergeDialog::fieldMergeModes() const
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
