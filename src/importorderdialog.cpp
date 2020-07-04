/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QLineEdit>
#include <QValidator>
#include <QPushButton>
#include <QDateTimeEdit>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QHeaderView>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QPalette>
#include <QVariant>
#include <QCache>

#if defined( MODELTEST )
#  include "modeltest.h"
#  define MODELTEST_ATTACH(x)   { (void) new ModelTest(x, x); }
#else
#  define MODELTEST_ATTACH(x)   ;
#endif

#include "bricklink.h"
#include "import.h"
#include "progressdialog.h"
#include "utility.h"
#include "config.h"
#include "transfer.h"

#include "importorderdialog.h"

Q_DECLARE_METATYPE(BrickLink::Order *)
Q_DECLARE_METATYPE(BrickLink::InvItemList *)

enum {
    OrderPointerRole = 0x0560ec9b,
    ItemListPointerRole
};


class OrderListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    OrderListModel(QObject *parent)
        : QAbstractTableModel(parent)
    {
        MODELTEST_ATTACH(this)

        m_trans = new Transfer;
        connect(m_trans, &Transfer::finished,
                this, &OrderListModel::flagReceived);
    }

    ~OrderListModel() override
    {
        setOrderList(QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> >());
        delete m_trans;
    }

    void setOrderList(const QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > &orderlist)
    {
        beginResetModel();
        for (auto &order : qAsConst(m_orderlist)) {
            delete order.first;
            delete order.second;
        }
        m_orderlist = orderlist;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent) const override
    {
        return parent.isValid() ? 0 : m_orderlist.size();
    }

    int columnCount(const QModelIndex &parent) const override
    {
        return parent.isValid() ? 0 : 5;
    }

    bool isReceived(const QPair<BrickLink::Order *, BrickLink::InvItemList *> &order) const
    {
        return (order.first->type() == BrickLink::OrderType::Received);
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
            return QVariant();

        QVariant res;
        const QPair<BrickLink::Order *, BrickLink::InvItemList *> &order = m_orderlist [index.row()];
        int col = index.column();

        if (role == Qt::DisplayRole) {
            switch (col) {
            case 0: res = isReceived(order) ? ImportOrderDialog::tr("Received") : ImportOrderDialog::tr("Placed"); break;
            case 1: res = order.first->id(); break;
            case 2: res = QLocale().toString(order.first->date().date(), QLocale::ShortFormat); break;
            case 3: {
                int firstline = order.first->address().indexOf('\n');

                if (firstline <= 0)
                    return order.first->other();
                else
                    return QString("%1 (%2)").arg(order.first->address().left(firstline), order.first->other());
            }
            case 4: res = Currency::toString(order.first->grandTotal(), order.first->currencyCode(), Currency::InternationalSymbol, 2); break;
            }
        } else if (role == Qt::DecorationRole) {
            switch (col) {
            case 3:
                QImage *flag = nullptr;
                QString cc = order.first->countryCode();
                if (cc.length() == 2)
                    flag = m_flags[cc];
                if (!flag) {
                    QString url = QString::fromLatin1("https://www.bricklink.com/images/flagsS/%1.gif").arg(cc);

                    TransferJob *job = TransferJob::get(url);
                    int userData = cc[0].unicode() | (cc[1].unicode() << 16);
                    job->setUserData<void>(userData, nullptr);
                    m_trans->retrieve(job);
                }
                if (flag)
                    res = *flag;
                break;
            }
        } else if (role == Qt::TextAlignmentRole) {
            res = (col == 4) ? Qt::AlignRight : Qt::AlignLeft;
        }
        else if (role == Qt::BackgroundColorRole) {
            if (col == 0) {
                QColor c = isReceived(order) ? Qt::green : Qt::blue;
                c.setAlphaF(0.2);
                res = c;
            }
        }
        else if (role == Qt::ToolTipRole) {
            QString tt = data(index, Qt::DisplayRole).toString();

            if (!order.first->address().isEmpty())
                tt = tt + QLatin1String("\n\n") + order.first->address();
            res = tt;
        }
        else if (role == OrderPointerRole) {
            res.setValue(order.first);
        }
        else if (role == ItemListPointerRole) {
            res.setValue(order.second);
        }

        return res;
    }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override
    {
        QVariant res;

        if (orient == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                case  0: res = ImportOrderDialog::tr("Type"); break;
                case  1: res = ImportOrderDialog::tr("Order #"); break;
                case  2: res = ImportOrderDialog::tr("Date"); break;
                case  3: res = ImportOrderDialog::tr("Buyer/Seller"); break;
                case  4: res = ImportOrderDialog::tr("Total"); break;
                }
            }
            else if (role == Qt::TextAlignmentRole) {
                res = (section == 4) ? Qt::AlignRight : Qt::AlignLeft;
            }
        }
        return res;
    }

    void sort(int section, Qt::SortOrder so) override
    {
        emit layoutAboutToBeChanged();
        std::sort(m_orderlist.begin(), m_orderlist.end(), orderCompare(section, so));
        emit layoutChanged();
    }


    class orderCompare {
    public:
        orderCompare(int section, Qt::SortOrder so) : m_section(section), m_so(so) { }

        bool operator()(const QPair<BrickLink::Order *, BrickLink::InvItemList *> &o1, const QPair<BrickLink::Order *, BrickLink::InvItemList *> &o2)
        {
            bool d = false;

            switch (m_section) {
            case  0: d = (o1.first->type() < o2.first->type()); break;
            case  1: d = (o1.first->id() <  o2.first->id()); break;
            case  2: d = (o1.first->date() < o2.first->date()); break;
            case  3: d = (o1.first->other().toLower() < o2.first->other().toLower()); break;
            case  4: d = (o1.first->grandTotal() < o2.first->grandTotal()); break;
            }
            return m_so == Qt::DescendingOrder ? d : !d;
        }

    private:
        int m_section;
        Qt::SortOrder m_so;
    };

private slots:
    void flagReceived(TransferJob *j)
    {
        if (!j || !j->data())
            return;

        QPair<int, void *> ud = j->userData<void>();
        QString cc;
        cc.append(QChar(ud.first & 0x0000ffff));
        cc.append(QChar(ud.first >> 16));

        auto *img = new QImage;
        if (img->loadFromData(*j->data())) {
            m_flags.insert(cc, img);

            for (int r = 0; r < rowCount(QModelIndex()); ++r) {
                QModelIndex idx = index(r, 3, QModelIndex());
                auto *order = qvariant_cast<BrickLink::Order *>(data(idx, OrderPointerRole));

                if (order->countryCode() == cc)
                    emit dataChanged(idx, idx);
            }
        } else {
            delete img;
        }
    }

private:
    QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > m_orderlist;
    QCache<QString, QImage> m_flags;
    Transfer *m_trans;
};


class TransHighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    TransHighlightDelegate(QObject *parent = nullptr)
            : QStyledItemDelegate(parent)
    { }

    void makeTrans(QPalette &pal, QPalette::ColorGroup cg, QPalette::ColorRole cr, const QColor &bg, qreal factor) const
    {
        QColor hl = pal.color(cg, cr);
        QColor th;
        th.setRgbF(hl.redF()   * (1.0 - factor) + bg.redF()   * factor,
                   hl.greenF() * (1.0 - factor) + bg.greenF() * factor,
                   hl.blueF()  * (1.0 - factor) + bg.blueF()  * factor);

        pal.setColor(cg, cr, th);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem myoption(option);

        if (option.state & QStyle::State_Selected) {
            QVariant v = index.data(Qt::BackgroundColorRole);
            QColor c = qvariant_cast<QColor> (v);
            if (v.isValid() && c.isValid()) {
                QStyleOptionViewItem myoption(option);

                makeTrans(myoption.palette, QPalette::Active,   QPalette::Highlight, c, 0.2);
                makeTrans(myoption.palette, QPalette::Inactive, QPalette::Highlight, c, 0.2);
                makeTrans(myoption.palette, QPalette::Disabled, QPalette::Highlight, c, 0.2);

                QStyledItemDelegate::paint(painter, myoption, index);
                return;
            }
        }

        QStyledItemDelegate::paint(painter, option, index);
    }
};


bool  ImportOrderDialog::s_last_by_number = false;
QDate ImportOrderDialog::s_last_from      = QDate::currentDate().addDays(-7);
QDate ImportOrderDialog::s_last_to        = QDate::currentDate();
int   ImportOrderDialog::s_last_type      = 1;


ImportOrderDialog::ImportOrderDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_order_number->setValidator(new QIntValidator(1, 99999999, w_order_number));
    w_order_list->setModel(new OrderListModel(this));
    w_order_list->setItemDelegate(new TransHighlightDelegate());

    connect(w_order_number, &QLineEdit::textChanged,
            this, &ImportOrderDialog::checkId);
    connect(w_by_number, &QAbstractButton::toggled,
            this, &ImportOrderDialog::checkId);
    connect(w_order_from, &QDateTimeEdit::dateChanged,
            this, &ImportOrderDialog::checkId);
    connect(w_order_to, &QDateTimeEdit::dateChanged,
            this, &ImportOrderDialog::checkId);

    connect(w_order_list->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImportOrderDialog::checkSelected);

    connect(w_next, &QAbstractButton::clicked,
            this, &ImportOrderDialog::download);
    connect(w_back, &QAbstractButton::clicked,
            this, &ImportOrderDialog::start);

    w_order_from->setDate(s_last_from);
    w_order_to->setDate(s_last_to);
    w_order_type->setCurrentIndex(s_last_type);
    w_by_number->setChecked(s_last_by_number);

    start();
    //resize(sizeHint());

#if defined(Q_OS_WINDOWS)
    // Qt bug: the sizeHint() is a little bit too small with the popup enabled
    w_order_from->setMinimumSize(w_order_from->minimumSizeHint() + QSize(fontMetrics().horizontalAdvance('x') * 2, 0));
    w_order_to->setMinimumSize(w_order_to->minimumSizeHint() + QSize(fontMetrics().horizontalAdvance('x') * 2, 0));
#endif
}

void ImportOrderDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
    QDialog::changeEvent(e);
}

void ImportOrderDialog::accept()
{
    s_last_by_number = w_by_number->isChecked();
    s_last_from      = w_order_from->date();
    s_last_to        = w_order_to->date();
    s_last_type      = w_order_type->currentIndex();

    QDialog::accept();
}

void ImportOrderDialog::start()
{
    w_stack->setCurrentIndex(0);

    if (w_by_number->isChecked())
        w_order_number->setFocus();
    else
        w_order_from->setFocus();

    w_ok->hide();
    w_back->hide();
    w_next->show();
    w_next->setDefault(true);

    checkId();
}

void ImportOrderDialog::download()
{
    Transfer trans;
    ProgressDialog progress(&trans, this);
    ImportBLOrder *import;

    if (w_by_number->isChecked())
        import = new ImportBLOrder(w_order_number->text(), orderType(), &progress);
    else
        import = new ImportBLOrder(w_order_from->date(), w_order_to->date(), orderType(), &progress);

    bool ok = (progress.exec() == QDialog::Accepted);

    if (ok && !import->orders().isEmpty()) {
        w_stack->setCurrentIndex(2);

        static_cast<OrderListModel *>(w_order_list->model())->setOrderList(import->orders());
        w_order_list->sortByColumn(2, Qt::AscendingOrder);

        w_order_list->selectionModel()->select(w_order_list->model()->index(0, 0), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
        w_order_list->scrollTo(w_order_list->model()->index(0, 0));
        w_order_list->header()->resizeSections(QHeaderView::ResizeToContents);
        w_order_list->setFocus();

        w_ok->show();
        w_ok->setDefault(true);
        w_next->hide();
        w_back->show();

        checkSelected();
    }
    else {
        w_message->setText(tr("There was a problem downloading the data for the specified order(s). This could have been caused by three things:<ul><li>a network error occured.</li><li>the order number and/or type you entered is invalid.</li><li>there are no orders of the specified type in the given time period.</li></ul>"));
        w_stack->setCurrentIndex(1);

        w_ok->hide();
        w_next->hide();
        w_back->show();
        w_back->setDefault(true);
    }

    delete import;
}

QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > ImportOrderDialog::orders() const
{
    QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > list;

    foreach (const QModelIndex &idx, w_order_list->selectionModel()->selectedRows()) {
        QPair<BrickLink::Order *, BrickLink::InvItemList *> pair;
        pair.first = qvariant_cast<BrickLink::Order *>(w_order_list->model()->data(idx, OrderPointerRole));
        pair.second = qvariant_cast<BrickLink::InvItemList *>(w_order_list->model()->data(idx, ItemListPointerRole));

        if (pair.first && pair.second)
            list << pair;
    }

    return list;
}

BrickLink::OrderType ImportOrderDialog::orderType() const
{
    switch (w_order_type->currentIndex()) {
    case 2 : return BrickLink::OrderType::Placed;
    case 1 : return BrickLink::OrderType::Received;
    case 0 :
    default: return BrickLink::OrderType::Any;
    }
}

void ImportOrderDialog::checkId()
{
    bool ok = true;

    if (w_by_number->isChecked())
        ok = w_order_number->hasAcceptableInput() && (w_order_number->text().length() >= 6) && (w_order_number->text().length() <= 8);
    else
        ok = (w_order_from->date() <= w_order_to->date()) && (w_order_to->date() <= QDate::currentDate());

    w_next->setEnabled(ok);
}

void ImportOrderDialog::checkSelected()
{
    w_ok->setEnabled(w_order_list->selectionModel()->hasSelection());
}

void ImportOrderDialog::activateItem()
{
    checkSelected();
    w_ok->animateClick();
}

#include "importorderdialog.moc"
#include "moc_importorderdialog.cpp"
