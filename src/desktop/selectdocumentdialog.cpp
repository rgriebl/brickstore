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

#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentlist.h"
#include "selectmergemode.h"
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

    auto [lots, currencyCode] = DocumentLotsMimeData::lots(Application::inst()->mimeClipboardGet());
    m_lotsFromClipboard = lots;
    m_currencyCodeFromClipboard = currencyCode;

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

    if (hasDocs || hasClip)
        setFocusProxy(hasClip ? m_clipboard : m_document);
}

LotList SelectDocument::lots() const
{
    LotList srcList;

    if (!isDocumentSelected())
        return srcList;

    if (m_clipboard->isChecked()) {
        srcList = m_lotsFromClipboard;
    } else {
        const auto *model = m_documentList->selectedItems().constFirst()
                ->data(Qt::UserRole).value<DocumentModel *>();
        if (model)
            srcList = model->lots();
    }

    LotList list;
    for (const Lot *lot : qAsConst(srcList))
        list << new Lot(*lot);
    return list;
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
    dpage->setFocusProxy(m_sd);
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

    QByteArray ba = Config::inst()->value(u"MainWindow/SelectCopyMergeDialog/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);

    dpage->adjustSize();
    mpage->adjustSize();
    QSize s = mpage->size().expandedTo(dpage->size());
    dpage->setFixedSize(s);
    mpage->setFixedSize(s);

    ba = Config::inst()->value(u"MainWindow/SelectCopyMergeDialog/MergeMode"_qs)
            .toByteArray();
    m_mm->restoreState(ba);
}

SelectCopyMergeDialog::~SelectCopyMergeDialog()
{
    Config::inst()->setValue(u"MainWindow/SelectCopyMergeDialog/Geometry"_qs, saveGeometry());
    Config::inst()->setValue(u"MainWindow/SelectCopyMergeDialog/MergeMode"_qs, m_mm->saveState());
}


LotList SelectCopyMergeDialog::lots() const
{
    return m_sd->lots();
}

QString SelectCopyMergeDialog::currencyCode() const
{
    return m_sd->currencyCode();
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
