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
#include <QListView>
#include <QAbstractListModel>
#include <QToolButton>

#include "script.h"
#include "scriptmanager.h"

#include "selectprintingtemplatedialog.h"

#if defined(MODELTEST)
#  include <QAbstractItemModelTester>
#  define MODELTEST_ATTACH(x)   { (void) new QAbstractItemModelTester(x, QAbstractItemModelTester::FailureReportingMode::Warning, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif


class PrintingScriptModel : public QAbstractListModel
{
    Q_OBJECT

public:
    PrintingScriptModel()
    {
        MODELTEST_ATTACH(this)
        connect(ScriptManager::inst(), &ScriptManager::aboutToReload,
                this, [this]() {
            emit beginResetModel();
            m_prtScripts.clear();
        });
        connect(ScriptManager::inst(), &ScriptManager::reloaded,
                this, [this]() {
            refreshScripts();
            emit endResetModel();
        });
        refreshScripts();
    }

public slots:
    void reload()
    {
        ScriptManager::inst()->reload();
    }

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    PrintingScriptTemplate *script(const QModelIndex &index) const
    {
        return index.isValid() ? static_cast<PrintingScriptTemplate *>(index.internalPointer()) : nullptr;
    }

    QModelIndex index(PrintingScriptTemplate *script) const
    {
        return script ? createIndex(m_prtScripts.indexOf(script), 0, script) : QModelIndex();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    void refreshScripts()
    {
        m_prtScripts.clear();
        auto scripts = ScriptManager::inst()->printingScripts();
        for (auto script : scripts) {
            const auto printingTemplates = script->printingTemplates();
            for (auto printingTemplate : printingTemplates) {
                m_prtScripts.append(printingTemplate);
            }
        }
    }

    QVector<PrintingScriptTemplate *> m_prtScripts;
};

QModelIndex PrintingScriptModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<PrintingScriptTemplate *>(m_prtScripts.at(row)));
    return {};
}

int PrintingScriptModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_prtScripts.count();
}

QVariant PrintingScriptModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !script(index))
        return QVariant();

    QVariant res;
    const PrintingScriptTemplate *s = script(index);

    if (role == Qt::DisplayRole)
        res = s->text();

    return res;
}

QVariant PrintingScriptModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return QVariant();
}


SelectPrintingTemplateDialog::SelectPrintingTemplateDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    auto *model = new PrintingScriptModel();

    w_list->setModel(model);

    connect(w_list->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SelectPrintingTemplateDialog::scriptChanged);
    connect(w_list, &QAbstractItemView::activated,
            this, &SelectPrintingTemplateDialog::scriptConfirmed);
    connect(w_update, &QAbstractButton::clicked,
            model, &PrintingScriptModel::reload);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    int selectionIndex = -1;
    for (int i = 0; i < model->rowCount(); ++i) {
        auto script = model->script(model->index(i, 0));
        if (script && script->text() == "standard") {
            selectionIndex = i;
            break;
        }
    }
    if (selectionIndex < 0 && model->rowCount() == 1)
        selectionIndex = 0;
    if (selectionIndex >= 0)
        w_list->selectionModel()->select(model->index(selectionIndex, 0), QItemSelectionModel::SelectCurrent);
    w_list->setFocus();
}

void SelectPrintingTemplateDialog::scriptChanged()
{
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled((script()));
}

void SelectPrintingTemplateDialog::scriptConfirmed()
{
    scriptChanged();
    w_buttons->button(QDialogButtonBox::Ok)->animateClick();
}

PrintingScriptTemplate *SelectPrintingTemplateDialog::script() const
{
    if (w_list->selectionModel()->hasSelection()) {
        QModelIndex idx = w_list->selectionModel()->selectedIndexes().front();

        return static_cast<PrintingScriptModel *>(w_list->model())->script(idx);
    }
    else
        return nullptr;
}

#include "selectprintingtemplatedialog.moc"

#include "moc_selectprintingtemplatedialog.cpp"
