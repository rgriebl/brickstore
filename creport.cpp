/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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

#include <qapplication.h>
#include <qfile.h>
#include <qdir.h>
#include <qpainter.h>
#include <qscriptengine.h>
#include <qtextstream.h>

#include "cutility.h"
#include "cresource.h"
#include "cmoney.h"
#include "creport.h"
#include "creport_p.h"
#include "cconfig.h"
#include "creportobjects.h"

namespace {
    static inline QScriptValue evaluate ( QScriptEngine *engine, const QString &code, const QString &scriptName = QString::null )
    {
        char *oldloc = ::setlocale ( LC_ALL, 0 );
        ::setlocale ( LC_ALL, "C" );
        QScriptValue res = engine-> evaluate ( code, scriptName );
        ::setlocale ( LC_ALL, oldloc );
        return res;
    }
}

CReport::CReport ( )
{
    d = new CReportPrivate ( );
    d-> m_loaded = -1;
    d-> m_engine = 0;
}

CReport::~CReport ( )
{
    delete d-> m_engine;
    delete d;
}

QString CReport::name ( ) const
{
    return d-> m_label. isEmpty ( ) ? d-> m_name : d-> m_label;
}

static QScriptValue construct_QFont(QScriptContext *, QScriptEngine *engine)
{
    return engine->toScriptValue(QFont());
}

static QScriptValue construct_QColor(QScriptContext *ctx, QScriptEngine *engine)
{
    QString t = ctx->argument(0).toString();
    return engine->toScriptValue(QColor(ctx->argument(0).toString()));
}

static QScriptValue construct_CReportPage(QScriptContext *, QScriptEngine *engine) {
    return engine->newQObject(new CReportPage( 0 ));
}

static QScriptValue creportpageToScriptValue(QScriptEngine *engine, CReportPage * const &in){
    return engine->newQObject(in, QScriptEngine::AutoOwnership);
}

static void creportpageFromScriptValue(const QScriptValue &object, CReportPage * &out){
    out = qobject_cast<CReportPage *>(object.toQObject());
}

static QScriptValue qsizeToScriptValue(QScriptEngine *engine, const QSize &s)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("width", QScriptValue(engine, s.width()));
    obj.setProperty("height", QScriptValue(engine, s.height()));
    return obj;
}

static void qsizeFromScriptValue(const QScriptValue &obj, QSize &s)
{
    s.setWidth( obj.property("width").toInt32() );
    s.setHeight( obj.property("height").toInt32() );
}

static QScriptValue qfontToScriptValue(QScriptEngine *engine, const QFont &f)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty("family", QScriptValue(engine, f.family()));
    obj.setProperty("pointSize", QScriptValue(engine, f.pointSize()));
    obj.setProperty("bold", QScriptValue(engine, f.bold()));
    obj.setProperty("italic", QScriptValue(engine, f.italic()));
    return obj;
}

static void qfontFromScriptValue(const QScriptValue &obj, QFont &f)
{
    f.setFamily( obj.property("family").toString() );
    f.setPointSize( obj.property("pointSize").toInt32() );
    f.setBold( obj.property("bold").toBool() );
    f.setItalic( obj.property("italic").toBool() );
}

QMap<QString, QVariant> documentData ( const CDocument *doc, const CDocument::ItemList &items ) {
    CDocument::Statistics stat = doc-> statistics ( items );

    QMap<QString, QVariant> docmap;
    docmap ["fileName"] = doc-> fileName ( );
    docmap ["title"]    = doc-> title ( );

    QMap<QString, QVariant> statmap;
    statmap ["errors"]   = stat. errors ( );
    statmap ["items"]    = stat. items ( );
    statmap ["lots"]     = stat. lots ( );
    statmap ["minValue"] = stat. minValue ( ). toDouble ( );
    statmap ["value"]    = stat. value ( ). toDouble ( );
    statmap ["weight"]   = stat. weight ( );

    docmap ["statistics"] = statmap;

    if ( doc-> order ( )) {
        const BrickLink::Order *order = doc-> order ( );
        QMap<QString, QVariant> ordermap;

        ordermap ["id"]           = order-> id ( );
        ordermap ["type"]         = ( order-> type ( ) == BrickLink::Order::Placed ? "P" : "R" );
        ordermap ["date"]         = order-> date ( ). toString ( Qt::TextDate );
        ordermap ["statusChange"] = order-> statusChange ( );
        ordermap ["other"]        = order-> other ( );
        ordermap ["buyer"]        = ( order-> type ( ) == BrickLink::Order::Placed ? CConfig::inst()->blLoginUsername() : order-> other ( ));
        ordermap ["seller"]       = ( order-> type ( ) == BrickLink::Order::Placed ? order-> other ( ) : CConfig::inst()->blLoginUsername());
        ordermap ["shipping"]     = order-> shipping ( ). toDouble ( );
        ordermap ["insurance"]    = order-> insurance ( ). toDouble ( );
        ordermap ["delivery"]     = order-> delivery ( ). toDouble ( );
        ordermap ["credit"]       = order-> credit ( ). toDouble ( );
        ordermap ["grandTotal"]   = order-> grandTotal ( ). toDouble ( );
        ordermap ["status"]       = order-> status ( );
        ordermap ["payment"]      = order-> payment ( );
        ordermap ["remarks"]      = order-> remarks ( );
        ordermap ["address"]      = order-> address ( );

        docmap ["order"] = ordermap;
    }

    QList<QVariant> itemslist;

    foreach ( CDocument::Item *item, items ) {
        QMap<QString, QVariant> imap;

        imap ["id"]        = item-> item ( )-> id ( );
        imap ["name"]      = item-> item ( )-> name ( );

        BrickLink::Picture *pic = BrickLink::inst ( )-> picture ( item-> item ( ), item-> color ( ), true );
        imap ["picture"]   = pic ? pic-> pixmap ( ) : QPixmap ( );

        QMap<QString, QVariant> status;
        status ["include"] = ( item-> status ( ) == BrickLink::InvItem::Include );
        status ["exclude"] = ( item-> status ( ) == BrickLink::InvItem::Exclude );
        status ["extra"]   = ( item-> status ( ) == BrickLink::InvItem::Extra );
        imap ["status"]    = status;

        imap ["quantity"]  = item-> quantity ( );

        QMap<QString, QVariant> color;
        color ["id"]       = item-> color ( ) ? (int) item-> color ( )-> id ( ) : -1;
        color ["name"]     = item-> color ( ) ? item-> color ( )-> name ( ) : "";
        color ["rgb"]      = item-> color ( ) ? item-> color ( )-> color ( ) : QColor ( );
        color ["picture"]  = *BrickLink::inst ( )-> colorImage ( item-> color ( ), 20, 20 );
        imap ["color"]     = color;

        QMap<QString, QVariant> cond;
        cond ["new"]       = ( item-> condition ( ) == BrickLink::New );
        cond ["used"]      = ( item-> condition ( ) == BrickLink::Used );
        imap ["condition"] = cond;

        QMap<QString, QVariant> scond;
        cond ["complete"]  = ( item-> subCondition ( ) == BrickLink::Complete );
        cond ["incomplete"]= ( item-> subCondition ( ) == BrickLink::Incomplete );
        cond ["misb"]      = ( item-> subCondition ( ) == BrickLink::MISB );
        imap ["subcondition"] = scond;

        imap ["price"]     = item-> price ( ). toDouble ( );
        imap ["total"]     = item-> total ( ). toDouble ( );
        imap ["bulkQuantity"] = item-> bulkQuantity ( );
        imap ["sale"]      = item-> sale ( );
        imap ["remarks"]   = item-> remarks ( );
        imap ["comments"]  = item-> comments ( );

        QMap<QString, QVariant> category;
        category ["id"]    = item-> category ( )-> id ( );
        category ["name"]  = item-> category ( )-> name ( );
        imap ["category"]  = category;

        QMap<QString, QVariant> itemtype;
        itemtype ["id"]    = item-> itemType ( )-> id ( );
        itemtype ["name"]  = item-> itemType ( )-> name ( );
        imap ["itemType"]  = itemtype;

        QList<QVariant> tq;
        tq << item-> tierQuantity ( 0 ) << item-> tierQuantity ( 1 ) << item-> tierQuantity ( 2 );
        imap ["tierQuantity"] = QVariant( tq );

        QList<QVariant> tp;
        tp << item-> tierPrice ( 0 ). toDouble ( ) << item-> tierPrice ( 1 ). toDouble ( ) << item-> tierPrice ( 2 ). toDouble ( );
        imap ["tierPrice"] = QVariant( tp );

        imap ["lotId"]     = item-> lotId ( );
        imap ["retain"]    = item-> retain ( );
        imap ["stockroom"] = item-> stockroom ( );
        imap ["reserved"]  = item-> reserved ( );

        itemslist << imap;
    }
    docmap ["items"] = QVariant( itemslist );

    return docmap;
}

bool CReport::load ( const QString &filename )
{
    QFileInfo fi ( filename );

    // deleted or updated?
    if (( d-> m_loaded >= 0 ) && (( !fi. exists ( )) || ( d-> m_loaded < (time_t) fi. lastModified ( ). toTime_t ( )))) {
        // unload
        delete d-> m_engine;
        d-> m_engine = 0;
        d-> m_code = QString ( );
        d-> m_loaded = -1;
    }

    if ( d-> m_loaded >= 0 )
        return false;

    QFile f ( filename );
    if ( f. open ( QFile::ReadOnly )) {
        QTextStream ts ( &f );
        d-> m_code = ts. readAll ( );

        d-> m_engine = new QScriptEngine ( this );
        evaluate ( d-> m_engine, d-> m_code, fi. baseName ( ));

        if ( !d-> m_engine-> hasUncaughtException ( )) {
            d-> m_name = fi. baseName ( );

            QScriptValue load = d-> m_engine-> evaluate( "load" );
            QScriptValue printDoc = d-> m_engine-> evaluate( "printDoc" );

            if ( load.isFunction() && printDoc.isFunction() ) {
                QScriptValue res = load.call ();

                if ( !d-> m_engine-> hasUncaughtException ( )) {
                    d-> m_loaded = fi. lastModified ( ). toTime_t ( );

                    if ( res.toVariant().isValid() && res. toVariant ( ). type ( ) == QVariant::Map )
                        d-> m_label = res. toVariant ( ). asMap ( ) ["name"]. toString ( );
                }
            }
        }
    }
//    else
//        qDebug ( "CReport::load() - failed to open %s", filename. latin1 ( ));

    return ( d-> m_loaded >= 0 );
}

void CReport::print ( QPrinter *pd, const CDocument *doc, const CDocument::ItemList &items ) const
{
    QMap<QString, QVariant> docmap = documentData ( doc, items );
    CReportJob *job = new CReportJob ( pd );

    qScriptRegisterMetaType(d->m_engine, creportpageToScriptValue, creportpageFromScriptValue );
    qScriptRegisterMetaType(d->m_engine, qsizeToScriptValue, qsizeFromScriptValue );
    qScriptRegisterMetaType(d->m_engine, qfontToScriptValue, qfontFromScriptValue );

    d->m_engine->globalObject().setProperty( "Utility", d->m_engine->newQObject( new CReportUtility ( ) ) );
    d->m_engine->globalObject().setProperty( "Job", d->m_engine->newQObject( job ) );
    d->m_engine->globalObject().setProperty( "CReportPage", d->m_engine->newQMetaObject( &CReportPage::staticMetaObject, d->m_engine->newFunction( construct_CReportPage ) ) );
    d->m_engine->globalObject().setProperty( "Money", d->m_engine->newQObject( new CReportMoneyStatic ( d-> m_engine ) ) );
    d->m_engine->globalObject().setProperty( "Document", d->m_engine->toScriptValue(docmap));
    d->m_engine->globalObject().setProperty( "Font", d->m_engine->newFunction(construct_QFont));
    d->m_engine->globalObject().setProperty( "Color", d->m_engine->newFunction(construct_QColor));

    evaluate ( d-> m_engine, d-> m_code, d-> m_name );

    QScriptValue printDoc = d-> m_engine-> evaluate( "printDoc" );
    printDoc.call();

    if ( !d-> m_engine-> hasUncaughtException ( )) {
        if ( !job-> isAborted ( )) {
            //job-> dump ( );
            job-> print ( 0, job-> pageCount ( ) - 1 );
        }
    }
//    else
//        qDebug ( ) << d->m_engine->uncaughtException().toString();
	
    delete job;
}

CReportManager::CReportManager ( )
{
    m_printer = 0;
}

CReportManager::~CReportManager ( )
{
    delete m_printer;
    s_inst = 0;
}

CReportManager *CReportManager::s_inst = 0;

CReportManager *CReportManager::inst ( )
{
    if ( !s_inst )
        s_inst = new CReportManager ( );
    return s_inst;
}

bool CReportManager::reload ( )
{
    while (!m_reports.isEmpty())
        delete m_reports.takeFirst();

    QString dir = CResource::inst ( )-> locate ( "print-templates", CResource::LocateDir );

    if ( !dir. isNull ( )) {
        QDir d ( dir );

        QStringList files = d. entryList ( "*.qs" );

        for ( QStringList::iterator it = files. begin ( ); it != files. end ( ); ++it ) {
            CReport *rep = new CReport ( );

            if ( rep-> load ( d. absFilePath ( *it )))
                m_reports. append ( rep );
            else
                delete rep;
        }
    }
    return false;
}

const QList <CReport *> &CReportManager::reports ( ) const
{
    return m_reports;
}

QPrinter *CReportManager::printer ( ) const
{
    if ( !m_printer )
        m_printer = new QPrinter ( QPrinter::HighResolution );
		
    return m_printer;
}
