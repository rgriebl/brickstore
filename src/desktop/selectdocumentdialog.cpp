// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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

#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentlist.h"
#include "selectmergemode.h"
#include "selectdocumentdialog.h"


SelectDocument::SelectDocument(const DocumentModel *self, bool multipleDocuments, QWidget *parent)
    : QWidget(parent)
{
    m_clipboard = new QRadioButton(tr("Items from Clipboard"));
    m_document = new QRadioButton(tr(multipleDocuments ? "Items from already open documents:" : "Items from an already open document:"));
    m_documentList = new QListWidget();
    m_documentList->setTextElideMode(Qt::ElideMiddle);
    if (multipleDocuments) {
        m_documentList->setSelectionMode(QListWidget::MultiSelection);
    }

    auto layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setColumnMinimumWidth(0, 20);
    layout->setColumnStretch(1, 100);
    layout->setRowStretch(2, 100);
    layout->addWidget(m_clipboard, 0, 0, 1, 2);
    layout->addWidget(m_document, 1, 0, 1, 2);
    layout->addWidget(m_documentList, 2, 1, 1, 1);

    auto [lots, currencyCode] = DocumentLotsMimeData::lots(Application::inst()->mimeClipboardGet());
    m_lotsFromClipboard = lots;
    m_currencyCodeFromClipboard = currencyCode;

    const auto docs = DocumentList::inst()->documents();
    for (const Document *doc : docs) {
        auto model = doc->model();
        if (model != self) {
            auto *item = new QListWidgetItem(doc->filePathOrTitle(), m_documentList);
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

    if (hasDocs || hasClip)
        setFocusProxy(hasClip ? m_clipboard : m_document);
}

LotList SelectDocument::lots(const DocumentModel *model) const
{
    if (!isDocumentSelected())
        return LotList();

    if (m_clipboard->isChecked()) {
        LotList list;
        list.reserve(m_lotsFromClipboard.size());
        for (const Lot *lot : std::as_const(m_lotsFromClipboard)) {
            list << new Lot(*lot);
        }
        if (model) {
            model->adjustLotCurrencyToModel(list, m_currencyCodeFromClipboard);
        }
        return list;
    } else {
        LotList list;

        for (auto &&item: m_documentList->selectedItems()) {
            const auto *from_model = item->data(Qt::UserRole).value<DocumentModel *>();
            if (from_model) {
                LotList model_list;
                auto lots = from_model->lots();
                model_list.reserve( lots.size());
                for (const Lot *lot : lots) {
                    model_list << new Lot(*lot);
                }
                if (model) {
                    model->adjustLotCurrencyToModel(model_list, from_model->currencyCode());
                }
                list.append(model_list);
            }
        }
        return list;
    }
}

QString SelectDocument::currencyCode() const
{
    if (!isDocumentSelected())
        return { };

    if (m_clipboard->isChecked()) {
        return m_currencyCodeFromClipboard;
    } else  {
        const auto *model = m_documentList->selectedItems().constFirst()
                ->data(Qt::UserRole).value<DocumentModel *>();
        if (model)
            return model->currencyCode();
    }
    return { };
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
    m_sd = new SelectDocument(self, false);

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

    QByteArray ba = Config::inst()->value(u"MainWindow/SelectDocumentDialog/Geometry"_qs)
                        .toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
}

SelectDocumentDialog::~SelectDocumentDialog()
{
    Config::inst()->setValue(u"MainWindow/SelectDocumentDialog/Geometry"_qs, saveGeometry());
}

LotList SelectDocumentDialog::lots() const
{
    return m_sd->lots(nullptr);
}

QString SelectDocumentDialog::currencyCode() const
{
    return m_sd->currencyCode();
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
                                             const QString &chooseFieldsText, bool addDocuments, QWidget *parent)
    : QWizard(parent)
{
    setOptions(QWizard::IndependentPages);
    setWizardStyle(QWizard::ModernStyle);
    QString title = tr(addDocuments ? "Add items" : "Copy or merge values");

    m_sd = new SelectDocument(self, addDocuments);
    m_mm = nullptr;

    auto *dpage = new WizardPage();
    dpage->setTitle(title);
    dpage->setSubTitle(chooseDocText);
    auto *dlayout = new QVBoxLayout(dpage);
    dlayout->addWidget(m_sd);
    dpage->setFocusProxy(m_sd);
    addPage(dpage);

    WizardPage *mpage = nullptr;

    if (!addDocuments) {
        m_mm = new SelectMergeMode(DocumentModel::MergeMode::Merge);

        mpage = new WizardPage();
        mpage->setTitle(title);
        mpage->setSubTitle(chooseFieldsText);
        mpage->setFinalPage(true);
        mpage->setComplete(true);
        auto *mlayout = new QVBoxLayout(mpage);
        mlayout->addWidget(m_mm);
        addPage(mpage);
    }

    connect(m_sd, &SelectDocument::documentSelected,
            dpage, &WizardPage::setComplete);

    QByteArray ba = Config::inst()->value(u"MainWindow/SelectCopyMergeDialog/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    dpage->adjustSize();

    if (mpage) {
        mpage->adjustSize();
        QSize s = mpage->size().expandedTo(dpage->size());
        dpage->setFixedSize(s);
        mpage->setFixedSize(s);
    }

    if (m_mm) {
        ba = Config::inst()->value(u"MainWindow/SelectCopyMergeDialog/MergeMode"_qs)
                .toByteArray();
        m_mm->restoreState(ba);
    }
}

SelectCopyMergeDialog::~SelectCopyMergeDialog()
{
    Config::inst()->setValue(u"MainWindow/SelectCopyMergeDialog/Geometry"_qs, saveGeometry());
    if (m_mm) {
        Config::inst()->setValue(u"MainWindow/SelectCopyMergeDialog/MergeMode"_qs, m_mm->saveState());
    }
}


LotList SelectCopyMergeDialog::lots(const DocumentModel &model) const
{
    return m_sd->lots(&model);
}

QHash<DocumentModel::Field, DocumentModel::MergeMode> SelectCopyMergeDialog::fieldMergeModes() const
{
    return m_mm->fieldMergeModes();
}

void SelectCopyMergeDialog::showEvent(QShowEvent *e)
{
    setFixedSize(sizeHint());
    QWizard::showEvent(e);
    page(0)->setFocus();
}


#include "moc_selectdocumentdialog.cpp"
#include "selectdocumentdialog.moc"
