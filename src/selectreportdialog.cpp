/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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

namespace {

class ReportModel : public QAbstractListModel {
    Q_OBJECT

public:
    ReportModel()
    {
        MODELTEST_ATTACH(this)

        m_reports = ReportManager::inst()->reports();
    }

public slots:
    void reload()
    {
        ReportManager::inst()->reload();
        m_reports = ReportManager::inst()->reports();
        reset();
    }

public:
    QModelIndex index(int row, int column, const QModelIndex &parent) const
    {
        if (hasIndex(row, column, parent))
            return parent.isValid() ? QModelIndex() : createIndex(row, column, const_cast<Report *>(m_reports.at(row)));
        return QModelIndex();
    }

    const Report *report(const QModelIndex &index) const
    {
        return index.isValid() ? static_cast<const Report *>(index.internalPointer()) : 0;
    }

    QModelIndex index(Report *report) const
    {
        return report ? createIndex(m_reports.indexOf(report), 0, report) : QModelIndex();
    }

    virtual int rowCount(const QModelIndex &parent) const
    {
        return parent.isValid() ? 0 : m_reports.count();
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (!index.isValid() || index.column() != 0 || !report(index))
            return QVariant();

        QVariant res;
        const Report *r = report(index);

        if (role == Qt:: DisplayRole) {
            res = r->name();
        }

        return res;
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const
    {
        if ((orient == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0))
            return tr("Name");
        return QVariant();
    }

private:
    QList<Report *> m_reports;
};

} // namespace


SelectReportDialog::SelectReportDialog(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f)
{
    setupUi(this);

    ReportModel *model = new ReportModel();

    w_list->setModel(model);

    connect(w_list->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(reportChanged()));
    connect(w_list, SIGNAL(activated(const QModelIndex &)), this, SLOT(reportConfirmed()));
	connect(w_update, SIGNAL(clicked()), model, SLOT(reload()));

	w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
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
        return 0;
}

#include "selectreportdialog.moc"
