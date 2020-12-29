/* Copyright (C) 2004-2020 Robert Griebl.All rights reserved.
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
#include <cstdlib>
#include <climits>
#include <clocale>

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QPrinter>
#include <QDesktopServices>
#include <QtDebug>
#include <QtScript>
#include <QtScriptTools>

#include "application.h"
#include "utility/currency.h"
#include "report.h"
#include "report_p.h"
#include "config.h"
#include "messagebox.h"
#include "framework.h"

#include "reportobjects.h"


Report::Report()
    : d(new ReportPrivate())
{ }

QString Report::name() const
{
    return d->m_label.isEmpty() ? d->m_name : d->m_label;
}

Report::~Report()
{ /* needed to use QScopedPointer on d */ }

Report *Report::load(const QString &filename)
{
    auto *r = new Report;

    QFileInfo fi(filename);

    QFile f(filename);
    if (f.open(QFile::ReadOnly)) {
        QTextStream ts(&f);
        r->d->m_code = ts.readAll();

        auto *eng = new QScriptEngine(r);
        r->d->m_engine.reset(eng);
        r->d->m_engine->setProperty("bsScriptPath", fi.absoluteFilePath());

        auto *dbg = new QScriptEngineDebugger(r);
        dbg->attachTo(eng);

        eng->evaluate(r->d->m_code, fi.baseName());

        if (!eng->hasUncaughtException()) {
            r->d->m_name = fi.baseName();

            QScriptValue load = eng->evaluate("load");

            if (load.isFunction() && eng->evaluate("print").isFunction() && !eng->hasUncaughtException()) {
                QScriptValue res = load.call();

                if (!eng->hasUncaughtException()) {
                    for (QScriptValueIterator it(res); it.hasNext(); ) {
                        it.next();
                        if (it.name() == QLatin1String("name"))
                            r->d->m_label = it.value().toString();
                    }
                    return r;
                }
            }
        }
        qDebug() << eng->uncaughtExceptionBacktrace();
    }
    else
        qDebug() << "Report::load() - failed to open " << filename;

    delete r;
    return nullptr;
}

void Report::print(QPaintDevice *pd, const Document *doc, const Document::ItemList &items) const
{
    Document::Statistics stat = doc->statistics(items);

    QScriptValue docVal = d->m_engine->newObject();
    docVal.setProperty("fileName", doc->fileName());
    docVal.setProperty("title", doc->title());

    QScriptValue statVal = d->m_engine->newObject();
    statVal.setProperty("errors", stat.errors());
    statVal.setProperty("items", stat.items());
    statVal.setProperty("lots", stat.lots());
    statVal.setProperty("minValue", stat.minValue());
    statVal.setProperty("value", stat.value());
    statVal.setProperty("weight", stat.weight());

    docVal.setProperty("statistics", statVal);

    if (doc->order()) {
        const BrickLink::Order *order = doc->order();
        QScriptValue orderVal = d->m_engine->newObject();

        orderVal.setProperty("id", order->id());
        orderVal.setProperty("type", (order->type() == BrickLink::OrderType::Placed ? "P" : "R"));
        orderVal.setProperty("date", d->m_engine->newDate(order->date()));
        orderVal.setProperty("statusChange", d->m_engine->newDate(order->statusChange()));
        orderVal.setProperty("other", order->other());
        orderVal.setProperty("buyer", (order->type() == BrickLink::OrderType::Placed ? Config::inst()->loginForBrickLink().first : order->other()));
        orderVal.setProperty("seller", (order->type() == BrickLink::OrderType::Placed ? order->other() : Config::inst()->loginForBrickLink().first));
        orderVal.setProperty("shipping", order->shipping());
        orderVal.setProperty("insurance", order->insurance());
        orderVal.setProperty("delivery", order->delivery());
        orderVal.setProperty("credit", order->credit());
        orderVal.setProperty("grandTotal", order->grandTotal());
        orderVal.setProperty("status", order->status());
        orderVal.setProperty("payment", order->payment());
        orderVal.setProperty("remarks", order->remarks());
        orderVal.setProperty("address", order->address());

        docVal.setProperty("order", orderVal);
    }

    QScriptValue itemList = d->m_engine->newArray();
    uint count = 0;

    for (const Document::Item *item : items) {
        QScriptValue iVal = d->m_engine->newObject();

        iVal.setProperty("id", item->item()->id());
        iVal.setProperty("name", item->item()->name());

        BrickLink::Picture *pic = BrickLink::core()->picture(item->item(), item->color(), true);
        iVal.setProperty("picture", qScriptValueFromValue(d->m_engine.data(), pic ? pic->image() : QImage()));

        QScriptValue statusVal = d->m_engine->newObject();
        statusVal.setProperty("include", (item->status() == BrickLink::Status::Include));
        statusVal.setProperty("exclude", (item->status() == BrickLink::Status::Exclude));
        statusVal.setProperty("extra", (item->status() == BrickLink::Status::Extra));
        iVal.setProperty("status", statusVal);

        iVal.setProperty("quantity", item->quantity());

        QScriptValue colorVal = d->m_engine->newObject();
        colorVal.setProperty("id", item->color() ? int(item->color()->id()) : -1);
        colorVal.setProperty("name", (item->color() ? item->color()->name() : QString()));
        colorVal.setProperty("rgb", qScriptValueFromValue(d->m_engine.data(), item->color() ? item->color()->color() : QColor()));
        colorVal.setProperty("picture", qScriptValueFromValue(d->m_engine.data(), BrickLink::core()->colorImage(item->color(), 20, 20)));
        iVal.setProperty("color", colorVal);

        QScriptValue condVal = d->m_engine->newObject();
        condVal.setProperty("new", (item->condition() == BrickLink::Condition::New));
        condVal.setProperty("used", (item->condition() == BrickLink::Condition::Used));
        iVal.setProperty("condition", condVal);

        iVal.setProperty("price", item->price());
        iVal.setProperty("total", item->total());
        iVal.setProperty("bulkQuantity", item->bulkQuantity());
        iVal.setProperty("sale", item->sale());
        iVal.setProperty("remarks", item->remarks());
        iVal.setProperty("comments", item->comments());

        QScriptValue categoryVal = d->m_engine->newObject();
        categoryVal.setProperty("id", item->category()->id());
        categoryVal.setProperty("name", item->category()->name());
        iVal.setProperty("category", categoryVal);

        QScriptValue itemtypeVal = d->m_engine->newObject();
        itemtypeVal.setProperty("id", item->itemType()->id());
        itemtypeVal.setProperty("name", item->itemType()->name());
        iVal.setProperty("itemType", itemtypeVal);

        QScriptValue tqVal = d->m_engine->newArray(3);
        QScriptValue tpVal = d->m_engine->newArray(3);
        for (int i = 0; i < 3; i++) {
            tqVal.setProperty(uint(i), item->tierQuantity(i));
            tpVal.setProperty(uint(i), item->tierPrice(i));
        }
        iVal.setProperty("tierQuantity", tqVal);
        iVal.setProperty("tierPrice", tpVal);

        iVal.setProperty("lotId", item->lotId());
        iVal.setProperty("retain", item->retain());

        QString stockroom;
        switch (item->stockroom()) {
        case BrickLink::Stockroom::A: stockroom = "A"; break;
        case BrickLink::Stockroom::B: stockroom = "B"; break;
        case BrickLink::Stockroom::C: stockroom = "C"; break;
        default: break;
        }
        iVal.setProperty("stockroom", stockroom);

        iVal.setProperty("reserved", item->reserved());
        iVal.setProperty("weight", item->item()->weight());
        iVal.setProperty("totalWeight", item->weight());

        itemList.setProperty(count++, iVal);
    }
    docVal.setProperty("items", itemList);


    auto *job = new ReportJob(pd);
    auto *ru = new ReportUtility();
    auto *ms = new ReportMoney();

    d->m_engine->globalObject().setProperty("Document", docVal);
    d->m_engine->globalObject().setProperty(job->objectName(), d->m_engine->newQObject(job));
    d->m_engine->globalObject().setProperty(ru->objectName(), d->m_engine->newQObject(ru));
    d->m_engine->globalObject().setProperty(ms->objectName(), d->m_engine->newQObject(ms));

    qScriptRegisterMetaType(d->m_engine.data(), Font::toScriptValue, Font::fromScriptValue);
    QScriptValue fontCtor = d->m_engine->newFunction(Font::createScriptValue);
    d->m_engine->globalObject().setProperty("Font", fontCtor);

    qScriptRegisterMetaType(d->m_engine.data(), Color::toScriptValue, Color::fromScriptValue);
    QScriptValue colorCtor = d->m_engine->newFunction(Color::createScriptValue);
    d->m_engine->globalObject().setProperty("Color", colorCtor);

    qScriptRegisterMetaType(d->m_engine.data(), Size::toScriptValue, Size::fromScriptValue);
    QScriptValue sizeCtor = d->m_engine->newFunction(Size::createScriptValue);
    d->m_engine->globalObject().setProperty("Size", sizeCtor);

    QScriptValue print = d->m_engine->evaluate("print");
    print.call();

    if (!d->m_engine->hasUncaughtException()) {
        if (!job->isAborted()) {
            job->dump();

            job->print(0, job->pageCount() - 1);
        }
    } else {
        QString msg = tr("Print script aborted with error:") +
                      QLatin1String("<br><br>") +
                      d->m_engine->uncaughtException().toString() +
                      QLatin1String("<br><br>Backtrace:<br><br>") +
                      d->m_engine->uncaughtExceptionBacktrace().join("<br>");
        MessageBox::warning(FrameWork::inst(), msg);
    }

    delete job;
}


ReportManager::ReportManager()
{
    m_printer = nullptr;
}

ReportManager::~ReportManager()
{
    delete m_printer;
    qDeleteAll(m_reports);
    s_inst = nullptr;
}

ReportManager *ReportManager::s_inst = nullptr;

ReportManager *ReportManager::inst()
{
    if (!s_inst)
        s_inst = new ReportManager();
    return s_inst;
}

bool ReportManager::reload()
{
    qDeleteAll(m_reports);
    m_reports.clear();

    QStringList spath = { QStringLiteral(":/print-templates") };

    spath << Application::inst()->externalResourceSearchPath("print-templates");

    QString dataloc = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if (!dataloc.isEmpty())
        spath.prepend(dataloc + QLatin1String("/print-templates"));

    for (const QString &dir : qAsConst(spath)) {
        const QFileInfoList fis = QDir(dir).entryInfoList(QStringList("*.qs"), QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : fis) {
            Report *rep = Report::load(fi.absoluteFilePath());

            if (rep)
                m_reports.append(rep);
        }
    }
    return !m_reports.isEmpty();
}

QList<Report *> ReportManager::reports() const
{
    return m_reports;
}

QPrinter *ReportManager::printer() const
{
    if (!m_printer)
        m_printer = new QPrinter(QPrinter::HighResolution);

    return m_printer;
}

#include "moc_report.cpp"
