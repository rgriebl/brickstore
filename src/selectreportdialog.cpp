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

#include "report.h"

#include "selectreportdialog.h"

#if defined(MODELTEST)
#  include "modeltest.h"
#  define MODELTEST_ATTACH(x)   { (void) new ModelTest(x, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif


class ReportModel : public QAbstractListModel
{
    Q_OBJECT

public:
    ReportModel()
        : m_reports(ReportManager::inst()->reports())
    {
        MODELTEST_ATTACH(this)
    }

public slots:
    void reload()
    {
        beginResetModel();
        ReportManager::inst()->reload();
        m_reports = ReportManager::inst()->reports();
        endResetModel();
    }

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    const Report *report(const QModelIndex &index) const
    {
        return index.isValid() ? static_cast<const Report *>(index.internalPointer()) : nullptr;
    }

    QModelIndex index(Report *report) const
    {
        return report ? createIndex(m_reports.indexOf(report), 0, report) : QModelIndex();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    QList<Report *> m_reports;
};

QModelIndex ReportModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent))
        return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Report *>(m_reports.at(row)));
    return {};
}

int ReportModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_reports.count();
}

QVariant ReportModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || !report(index))
        return QVariant();

    QVariant res;
    const Report *r = report(index);

    if (role == Qt::DisplayRole)
        res = r->label();

    return res;
}

QVariant ReportModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
        return tr("Name");
    return QVariant();
}


SelectReportDialog::SelectReportDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    auto *model = new ReportModel();

    w_list->setModel(model);

    connect(w_list->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SelectReportDialog::reportChanged);
    connect(w_list, &QAbstractItemView::activated,
            this, &SelectReportDialog::reportConfirmed);
    connect(w_update, &QAbstractButton::clicked,
            model, &ReportModel::reload);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    int selectionIndex = -1;
    for (int i = 0; i < model->rowCount(); ++i) {
        auto report = model->report(model->index(i, 0));
        if (report && report->name() == "standard") {
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

void SelectReportDialog::reportChanged()
{
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled((report()));
}

void SelectReportDialog::reportConfirmed()
{
    reportChanged();
    w_buttons->button(QDialogButtonBox::Ok)->animateClick();
}

const Report *SelectReportDialog::report() const
{
    if (w_list->selectionModel()->hasSelection()) {
        QModelIndex idx = w_list->selectionModel()->selectedIndexes().front();

        return static_cast<ReportModel *>(w_list->model())->report(idx);
    }
    else
        return nullptr;
}

#include "selectreportdialog.moc"

#include "moc_selectreportdialog.cpp"
