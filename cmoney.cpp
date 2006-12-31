/* Copyright (C) 2004-2005 Robert Griebl.  All rights reserved.
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
#include <locale.h>

#include <qapplication.h>
#include <qlocale.h>
#include <qvalidator.h>
#include <qregexp.h>
#include <qlineedit.h>
#include <qevent.h>
#include <qdatastream.h>

#if defined( Q_OS_MACX )

#include <CoreFoundation/CFLocale.h>

extern QString cfstring2qstring ( CFStringRef str ); // defined in qglobal.cpp

#endif


#include "cconfig.h"
#include "cmoney.h"

QDataStream &operator << ( QDataStream &s, const money_t &m )
{
	s << m. val; return s;
}

QDataStream &operator >> ( QDataStream &s, money_t &m )
{
	s >> m. val; return s;
}

double money_t::toDouble ( ) const
{
	return double( val ) / 1000.;
}

QString money_t::toCString ( bool with_currency_symbol, int precision ) const
{
	QString s = QLocale::c ( ). toString ( toDouble ( ), 'f', precision );
	if ( with_currency_symbol )
		s. prepend ( "$ " );
	return s;
}

QString money_t::toLocalizedString ( bool with_currency_symbol, int precision ) const
{
	return CMoney::inst ( )-> toString ( toDouble ( ), with_currency_symbol, precision );
}

money_t money_t::fromCString ( const QString &str )
{
	return money_t ( QLocale::c ( ). toDouble ( str ));
}

money_t money_t::fromLocalizedString ( const QString &str )
{
	return CMoney::inst ( )-> toMoney ( str );
}



class CMoneyData : public QObject {
public:
	CMoneyData ( )
		: m_locale ( QLocale::system ( ))
	{ 
		qApp-> installEventFilter ( this );
	}

	virtual ~CMoneyData ( )
	{
		qApp-> removeEventFilter ( this );
	}

	virtual bool eventFilter ( QObject *o, QEvent *e )
	{
		bool res = false;

		if ((( e-> type ( ) == QEvent::KeyPress ) || ( e-> type ( ) == QEvent::KeyRelease )) && o-> isA ( "QLineEdit" )) {
			const QValidator *val = static_cast <QLineEdit *> ( o )-> validator ( );

			if ( val && val-> isA ( "CMoneyValidator" ))
				res = static_cast <const CMoneyValidator *> ( val )-> filterInput ( static_cast <QLineEdit *> ( o ), static_cast <QKeyEvent *> ( e ));
		}
		return res;
	}

	bool    m_localized;
	int     m_precision;
	double  m_factor;
	
	QLocale m_locale;
	QString m_csymbol;
	QChar   m_decpoint;
};


CMoney *CMoney::s_inst = 0;

CMoney *CMoney::inst ( )
{
	if ( !s_inst )
		s_inst = new CMoney ( );
	return s_inst;
}

CMoney::CMoney ( )
{
	d = new CMoneyData;
	
	d-> m_factor    = CConfig::inst ( )-> readDoubleEntry ( "/General/Money/Factor",    1. );
	d-> m_localized = CConfig::inst ( )-> readBoolEntry   ( "/General/Money/Localized", false );
	d-> m_precision = CConfig::inst ( )-> readNumEntry    ( "/General/Money/Precision", 3 );

	QString decpoint;
	QString csymbol, csymbolint;

#if defined( Q_OS_MACX )
	// BSD and MACOSX don't have support for LC_MONETARY !!!

	// we need at least 10.3 for CFLocale... stuff (weak import)
	if ( CFLocaleCopyCurrent != 0  && CFLocaleGetValue != 0 ) {
		CFLocaleRef loc = CFLocaleCopyCurrent ( );
		CFStringRef ds = (CFStringRef) CFLocaleGetValue ( loc, kCFLocaleDecimalSeparator );
		CFStringRef cs = (CFStringRef) CFLocaleGetValue ( loc, kCFLocaleCurrencySymbol );
		CFStringRef cc = (CFStringRef) CFLocaleGetValue ( loc, kCFLocaleCurrencyCode );

		decpoint = cfstring2qstring ( ds );
		csymbol = cfstring2qstring ( cs );
		csymbolint = cfstring2qstring ( cc );
	}

#else
	::setlocale ( LC_ALL, "" );  // initialize C-locale to OS supplied values
	::lconv *lc = ::localeconv ( );
	
	if ( lc-> mon_decimal_point && *lc-> mon_decimal_point && *lc-> mon_decimal_point != CHAR_MAX )
		decpoint = QString::fromLocal8Bit ( lc-> mon_decimal_point ) [0];
		
	csymbol = QString::fromLocal8Bit ( lc-> currency_symbol );
	csymbolint = QString::fromLocal8Bit ( lc-> int_curr_symbol );
#endif

	if ( !decpoint. isEmpty ( ))
		d-> m_decpoint = decpoint [0];
	else
		d-> m_decpoint = d-> m_locale. toString ( 1.5, 'f', 1 ) [1]; // simple hack as fallback
	
	if ( !csymbol. isEmpty ( )) 
		d-> m_csymbol = csymbol;
	else
		d-> m_csymbol = csymbolint;
}

CMoney::~CMoney ( )
{
	delete d;
	s_inst = 0;
}

QString CMoney::localCurrencySymbol ( ) const
{
	return d-> m_csymbol;
}

QString CMoney::currencySymbol ( ) const
{
	return d-> m_localized ? d-> m_csymbol : QChar ( '$' );
}

QChar CMoney::localDecimalPoint ( ) const
{
	return d-> m_decpoint;
}

QChar CMoney::decimalPoint ( ) const
{
	return d-> m_localized ? d-> m_decpoint : QChar ( '.' );
}

void CMoney::setLocalization ( bool loc )
{
	if ( loc != d-> m_localized ) {
		d-> m_localized = loc;
		CConfig::inst ( )-> writeEntry ( "/General/Money/Localized", loc );

		emit monetarySettingsChanged ( );
	}
}

bool CMoney::isLocalized ( ) const
{
	return d-> m_localized;
}

void CMoney::setFactor ( double f )
{
	if ( f != d-> m_factor ) {
		d-> m_factor = f;
		CConfig::inst ( )-> writeEntry ( "/General/Money/Factor", f );

		emit monetarySettingsChanged ( );
	}
}

double CMoney::factor ( ) const
{
	return d-> m_factor;
}

void CMoney::setPrecision ( int prec )
{
	if ( prec != d-> m_precision ) {
		d-> m_precision = prec;
		CConfig::inst ( )-> writeEntry ( "/General/Money/Precision", prec );

		emit monetarySettingsChanged ( );
	}
}

int CMoney::precision ( ) const
{
	return d-> m_precision;
}

QString CMoney::toString ( double v, bool with_currency_symbol, int precision ) const
{
	if ( d-> m_localized )
		v *= d-> m_factor;

	QString s = QString::number ( v, 'f', precision );

	if ( d-> m_localized )
		s. replace ( QChar( '.' ), d-> m_decpoint );

	if ( with_currency_symbol )
		return currencySymbol ( ) + " " + s;
	else
		return s;
}

money_t CMoney::toMoney ( const QString &str, bool *ok ) const
{
	QString s = str. stripWhiteSpace ( );
		
	if ( !s. isEmpty ( ) && s. startsWith ( currencySymbol ( )))
		s = s. mid ( currencySymbol ( ). length ( )). stripWhiteSpace ( );

	if ( d-> m_localized )
		s. replace ( d-> m_decpoint, QChar( '.' ));

	money_t v = s. toDouble ( ok );

	if ( d-> m_localized )
		return v /= d-> m_factor;
		
	return v;
}



CMoneyValidator::CMoneyValidator ( QObject *parent, const char *name )
	: QDoubleValidator ( parent, name )
{ }

CMoneyValidator::CMoneyValidator ( money_t bottom, money_t top, int decimals, QObject *parent, const char *name )
	: QDoubleValidator ( bottom. toDouble ( ), top. toDouble ( ), decimals, parent, name )
{ }

QValidator::State CMoneyValidator::validate ( QString &input, int &pos ) const
{
	double b = bottom ( ), t = top ( );
	int d = decimals ( );

	if ( CMoney::inst ( )-> isLocalized ( )) {
		double f = CMoney::inst ( )-> factor ( );

		b *= f;
		t *= f;
	}

	QChar dp = CMoney::inst ( )-> decimalPoint ( );

	QRegExp r ( QString ( " *-?\\d*\\%1?\\d* *" ). arg ( dp ));

	if ( b >= 0 && input. stripWhiteSpace ( ). startsWith ( QString::fromLatin1 ( "-" )))
		return Invalid;
	
	if ( r. exactMatch ( input )) {
		QString s = input;
		s. replace ( dp, QChar( '.' ));

		int i = s. find ( '.' );
		if ( i >= 0 ) {
			// has decimal point, now count digits after that
			i++;                                                        
			int j = i;
			while ( s [j]. isDigit ( ))
				j++;
			if ( j > i ) {
				while ( s [j - 1] == '0' )
					j--;
			}
				
			if ( j - i > d ) {
				return Intermediate;
			}
		}
		
		double entered = s. toDouble ( );

		if ( entered < b || entered > t )
			return Intermediate;
		else
			return Acceptable;
    }
	else if ( r. matchedLength ( ) == (int) input. length ( )) {
		return Intermediate;
    }
	else {
		pos = input. length ( );
		return Invalid;
	}
}

bool CMoneyValidator::filterInput ( QLineEdit * /*edit*/, QKeyEvent *ke ) const
{
	QString text = ke-> text ( );
	bool fixed = false;

	for ( int i = 0; i < int( text. length ( )); i++ ) {
		QCharRef ir = text [i];
		if ( ir == '.' || ir == ',' ) {
			ir = CMoney::inst ( )-> decimalPoint ( );
			fixed = true;
		}
	}

	if ( fixed )
		*ke = QKeyEvent ( ke-> type ( ), ke-> key ( ), ke-> ascii ( ), ke-> state ( ), text, ke-> isAutoRepeat ( ), ke-> count ( ));
	return false;
}
