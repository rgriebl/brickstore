/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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
#include <stdlib.h>
#include <limits.h>
#include <locale.h>

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QPrinter>

#include <QtScript>
//#include <qsinputdialogfactory.h>
//#include <qsobjectfactory.h>

#include "capplication.h"
#include "cmoney.h"
#include "creport.h"
#include "creport_p.h"

//#include "creportobjects.h"

#if 0
namespace {

static inline QSArgument evaluate(QSInterpreter *interp, const QString &code, QObject *context = 0, const QString &scriptName = QString::null)
{
    QCString oldloc = ::setlocale(LC_ALL, 0);
    ::setlocale(LC_ALL, "C");
    QSArgument res = interp->evaluate(code, context, scriptName);
    ::setlocale(LC_ALL, oldloc);
    return res;
}


class CReportFactory : public QSObjectFactory {
public:
    CReportFactory()
    {
        registerClass("Money", new CReportMoneyStatic(interpreter()));
    }

    QObject *create(const QString &, const QSArgumentList &, QObject *)
    {
        return 0;
    }
};

}
#endif

CReport::CReport()
{
    d = new CReportPrivate();
    d->m_loaded = -1;
    d->m_engine = 0;
}

CReport::~CReport()
{
    delete d->m_engine;
    delete d;
}

QString CReport::name() const
{
    return d->m_label.isEmpty() ? d->m_name : d->m_label;
}

bool CReport::load(const QString &filename)
{
    QFileInfo fi(filename);

    // deleted or updated?
    if ((d->m_loaded >= 0) && ((!fi.exists()) || (d->m_loaded < (time_t) fi.lastModified().toTime_t ()))) {
        // unload
        delete d->m_engine;
        d->m_engine = 0;
        d->m_code = QString();
        d->m_loaded = -1;
    }

    if (d->m_loaded >= 0)
        return false;

    QFile f(filename);
    if (f.open(QFile::ReadOnly)) {
        QTextStream ts(&f);
        d->m_code = ts.readAll();

        d->m_engine = new QScriptEngine(this);
        //TODO: d->m_engine->addObjectFactory(new QSInputDialogFactory());
        //TODO: d->m_engine->addObjectFactory(new CReportFactory());

        d->m_engine->evaluate(d->m_code, fi.baseName());

        if (!d->m_engine->hasUncaughtException()) {
            d->m_name = fi.baseName();

// evalute("print")->isFunction()
            QScriptValue load = d->m_engine->evaluate("load");
        
            if (load.isFunction() && d->m_engine->evaluate("print").isFunction() && !d->m_engine->hasUncaughtException()) {
                QScriptValue res = load.call();

                if (!d->m_engine->hasUncaughtException()) {
                    d->m_loaded = fi.lastModified().toTime_t ();

	                for (QScriptValueIterator it(res); it.hasNext(); ) {
    	                it.next();
        	            if (it.name() == QLatin1String("name"))
            	            d->m_label = it.value().toString();
            	    }
                }
            }
        }
    }
    else
        qDebug() << "CReport::load() - failed to open " << filename;

    return (d->m_loaded >= 0);
}

void CReport::print(QPaintDevice *pd, const CDocument *doc, const CDocument::ItemList &items) const
{
    CDocument::Statistics stat = doc->statistics(items);

    QVariantMap docmap;
    docmap ["fileName"] = doc->fileName();
    docmap ["title"]    = doc->title();

    QVariantMap statmap;
    statmap ["errors"]   = stat.errors();
    statmap ["items"]    = stat.items();
    statmap ["lots"]     = stat.lots();
    statmap ["minValue"] = stat.minValue().toDouble();
    statmap ["value"]    = stat.value().toDouble();
    statmap ["weight"]   = stat.weight();

    docmap ["statistics"] = statmap;

    if (doc->order()) {
        const BrickLink::Order *order = doc->order();
        QVariantMap ordermap;

        ordermap ["id"]           = order->id();
        ordermap ["type"]         = (order->type() == BrickLink::Order::Placed ? "P" : "R");
        ordermap ["date"]         = order->date().toString(Qt::TextDate);
        ordermap ["statusChange"] = order->statusChange();
        ordermap ["buyer"]        = order->buyer();
        ordermap ["shipping"]     = order->shipping().toDouble();
        ordermap ["insurance"]    = order->insurance().toDouble();
        ordermap ["delivery"]     = order->delivery().toDouble();
        ordermap ["credit"]       = order->credit().toDouble();
        ordermap ["grandTotal"]   = order->grandTotal().toDouble();
        ordermap ["status"]       = order->status();
        ordermap ["payment"]      = order->payment();
        ordermap ["remarks"]      = order->remarks();

        docmap ["order"] = ordermap;
    }

    QVariantList itemslist;

    foreach(CDocument::Item *item, items) {
        QVariantMap imap;

        imap ["id"]        = item->item()->id();
        imap ["name"]      = item->item()->name();

        BrickLink::Picture *pic = BrickLink::inst()->picture(item->item(), item->color(), true);
        imap ["picture"]   = pic ? pic->pixmap() : QPixmap();

        QVariantMap status;
        status ["include"] = (item->status() == BrickLink::Include);
        status ["exclude"] = (item->status() == BrickLink::Exclude);
        status ["extra"]   = (item->status() == BrickLink::Extra);
        imap ["status"]    = status;

        imap ["quantity"]  = item->quantity();

        QVariantMap color;
        color ["id"]       = item->color() ? (int) item->color()->id() : -1;
        color ["name"]     = item->color() ? item->color()->name() : "";
        color ["rgb"]      = item->color() ? item->color()->color() : QColor();
        QPixmap colorpix;
        colorpix.fromImage(BrickLink::inst()->colorImage(item->color(), 20, 20));
        color ["picture"]  = colorpix;
        imap ["color"]     = color;

        QVariantMap cond;
        cond ["new"]       = (item->condition() == BrickLink::New);
        cond ["used"]      = (item->condition() == BrickLink::Used);
        imap ["condition"] = cond;

        imap ["price"]     = item->price().toDouble();
        imap ["total"]     = item->total().toDouble();
        imap ["bulkQuantity"] = item->bulkQuantity();
        imap ["sale"]      = item->sale();
        imap ["remarks"]   = item->remarks();
        imap ["comments"]  = item->comments();

        QVariantMap category;
        category ["id"]    = item->category()->id();
        category ["name"]  = item->category()->name();
        imap ["category"]  = category;

        QVariantMap itemtype;
        itemtype ["id"]    = item->itemType()->id();
        itemtype ["name"]  = item->itemType()->name();
        imap ["itemType"]  = itemtype;

        QVariantList tq;
        tq << item->tierQuantity(0) << item->tierQuantity(1) << item->tierQuantity(2);
        imap ["tierQuantity"] = tq;

        QVariantList tp;
        tp << item->tierPrice(0).toDouble() << item->tierPrice(1).toDouble() << item->tierPrice(2).toDouble();
        imap ["tierPrice"] = tp;

        imap ["lotId"]     = item->lotId();
        imap ["retain"]    = item->retain();
        imap ["stockroom"] = item->stockroom();
        imap ["reserved"]  = item->reserved();

        itemslist << imap;
    }
    docmap ["items"] = itemslist;


    //CReportJob *job = new CReportJob(pd);
    
    QScriptContext *context = d->m_engine->pushContext();
    context->activationObject().setProperty("Document", d->m_engine->newVariant(docmap));
    //context->activationObject().setProperty(job->objectName(), d->m_engine->newQObject(job));

    //TODO: d->m_engine->addTransientObject(new CReportUtility());

    QScriptValue print = d->m_engine->evaluate("print");
	print.call();

    if (!d->m_engine->hasUncaughtException()) {
        //if (!job->isAborted()) {
        //    job->dump();

        //    job->print(0, job->pageCount() - 1);
        //}
    }
    d->m_engine->popContext();

    //delete job;
}


CReportManager::CReportManager()
{
    m_printer = 0;
}

CReportManager::~CReportManager()
{
    delete m_printer;
    qDeleteAll(m_reports);
    s_inst = 0;
}

CReportManager *CReportManager::s_inst = 0;

CReportManager *CReportManager::inst()
{
    if (!s_inst)
        s_inst = new CReportManager();
    return s_inst;
}

bool CReportManager::reload()
{
	qDeleteAll(m_reports);
    m_reports.clear();

    QStringList files;
    QDir sdir(QLatin1String(":/print-templates"));
    files << sdir.entryList("*.qs");
    QDir udir(cApp->userDataDir() + QLatin1String("/print-templates"));
    files << udir.entryList("*.qs");

    foreach (QString s, files) {
        CReport *rep = new CReport();

        if (rep->load(d.absoluteFilePath(s)))
            m_reports.append(rep);
        else
            delete rep;
    }
    return false;
}

const QList<CReport *> &CReportManager::reports() const
{
    return m_reports;
}

QPrinter *CReportManager::printer() const
{
    if (!m_printer)
        m_printer = new QPrinter(QPrinter::HighResolution);

    return m_printer;
}

