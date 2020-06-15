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
#ifndef __CCHECKFORUPDATES_H__
#define __CCHECKFORUPDATES_H__

#include <qbuffer.h>
#include <q3valuelist.h>
//Added by qt3to4:
#include <Q3TextStream>

#include "capplication.h"
#include "cconfig.h"
#include "cprogressdialog.h"


class CCheckForUpdates : public QObject {
	Q_OBJECT

	struct VersionRecord;

public:
	CCheckForUpdates ( CProgressDialog *pd )
		: m_progress ( pd )
	{
        m_url = "http://" + cApp-> appURL ( ) + "/RELEASES";
        m_query << CKeyValue ( "version", cApp-> appVersion ( ));
        if ( !pd )
            m_query << CKeyValue ( "check", "startup" );

        m_current_version. fromString ( cApp-> appVersion ( ));

        if ( pd ) {
            connect ( pd, SIGNAL( transferFinished ( )), this, SLOT( gotten ( )));

            pd-> setAutoClose ( false );
            pd-> setHeaderText ( tr( "Checking for program updates" ));
            pd-> setMessageText ( tr( "You are currently running %1 %2" ). arg ( cApp-> appName ( ), cApp-> appVersion ( )));

            pd-> get ( m_url, m_query );
        }
    }

    void checkNow ( ) {
        if ( !m_progress ) {
            CTransfer *transfer = new CTransfer ( );
            connect ( transfer, SIGNAL( finished ( CTransfer::Job * )), this, SLOT( gotten ( CTransfer::Job * )));

            transfer-> setProxy ( CConfig::inst ( )-> useProxy ( ), CConfig::inst ( )-> proxyName ( ), CConfig::inst ( )-> proxyPort ( ));
            transfer-> init ( );
            transfer-> getIfNewer ( m_url. latin1 ( ), m_query, QDateTime ( ) );
        }
    }

signals:
    void newVersionAvailable ( );

private slots:
    void gotten ( CTransfer::Job *job = 0 )
	{
        CTransfer::Job *j = m_progress ? m_progress-> job ( ) : job;
		QByteArray *data = j-> data ( );
		bool ok = false;
        bool update_possible = false;

		if ( data && data-> size ( )) {
            QBuffer buf ( data );

			if ( buf. open ( QIODevice::ReadOnly )) {
				Q3TextStream ts ( &buf );
				QString line;

				while ( !( line = ts. readLine ( )). isNull ( )) {
					QStringList sl = QStringList::split ( '\t', line, true );

					if (( sl. count ( ) < 2 ) || ( sl [0]. length ( ) != 1 ) || sl [1]. isEmpty ( )) 
						continue;

					VersionRecord vr;

					switch ( sl [0][0]. latin1 ( )) {
						case 'S': vr. m_type = VersionRecord::Stable; break;
						case 'B': vr. m_type = VersionRecord::Beta; break;
						default : continue;
					}

					if ( !vr. fromString ( sl [1] ))
						continue;
					
					if (( sl. count ( ) >= 3 ) && ( sl [2]. length ( ) == 1 ))
						vr. m_has_errors = ( sl [2][0] == 'E' );

					int diff = m_current_version. compare ( vr );
						
					vr. m_is_newer = ( diff < 0 );
					vr. m_is_current = ( diff == 0 );

					if ( vr. m_is_current && vr. m_has_errors )
						m_current_version. m_has_errors = true;

					if ( vr. m_is_newer && !vr. m_has_errors )
						update_possible = true;

					m_versions << vr;
				}

				if ( m_versions. isEmpty ( )) {
                    if ( m_progress )
                        m_progress-> setErrorText ( tr( "Version information is not available." ));
				}
				else {
					QString str;
	
					if ( !update_possible ) {
						str = tr( "Your currently installed version is up-to-date." );
					}
					else {
						str = tr( "A newer version than the one currently installed is available:" );
						str += "<br /><br /><br /><table>";

						for ( Q3ValueList <VersionRecord>::iterator it = m_versions. begin ( ); it != m_versions. end ( ); ++it ) {
							const VersionRecord &vrr = *it;

							if ( vrr. m_is_newer && !vrr. m_has_errors ) {
								str += QString( "<tr><td><b>%1</b></td><td>%2</td></tr>" ).
									arg ( vrr. m_type == VersionRecord::Stable ? tr( "Stable release" ) : tr( "Beta release" )). 
									arg( vrr. toString ( ));
							}
						}
						str += "</table><br />";
						str += QString( "<a href=\"http://" + cApp-> appURL ( ) + "/changes\">%1</a>" ). arg ( tr( "Detailed list of changes" ));
					}
				
					if ( m_current_version. m_has_errors ) {
                        QString link = QString( "<a href=\"http://" + cApp-> appURL ( ) + "\">%1</a>" ). arg ( tr( "the BrickStock homepage" ));

                        str += "<br /><br /><br /><br /><br /><br /><table><tr><td><img src=\"brickstock-important\" align=\"left\" /></td><td>" +
							   tr( "<b>Please note:</b> Your currently installed version is flagged as defective. Please visit %1 to find out the exact cause." ). arg ( link ) +
							   "</td></tr></table>";
					}

                    if ( m_progress )
                        m_progress-> setMessageText ( str );
					ok = true;
				}
			}
		}
		else
            if ( m_progress )
                m_progress-> setErrorText ( tr( "Version information is not available." ));

        if ( ok ) {
            CConfig::inst ( )-> setLastApplicationUpdateCheck ( QDateTime::currentDateTime ( ));

            if ( m_progress )
                m_progress-> setProgressVisible ( false );
            else if ( update_possible )
                emit newVersionAvailable ( );
        }

        if ( m_progress )
            m_progress-> setFinished ( ok );
	}

private:
	struct VersionRecord {
		VersionRecord ( )
			: m_major ( 0 ), m_minor ( 0 ), m_revision ( 0 ), m_type ( Stable ), m_has_errors ( false ), m_is_newer ( false ), m_is_current ( false )
		{ }

		bool fromString ( const QString &str )
		{
			if ( !str. isEmpty ( )) {
				QStringList vl = QStringList::split ( '.', str );

                if ( vl. count ( ) >= 3 ) {
					m_major    = vl [0]. toInt ( );
					m_minor    = vl [1]. toInt ( );
					m_revision = vl [2]. toInt ( );

					return true;
				}
			}
			return false;
		}

		QString toString ( ) const
		{
			return QString( "%1.%2.%3" ). arg ( m_major ). arg ( m_minor ). arg ( m_revision );
		}

		int compare ( const VersionRecord &vr ) const
		{
			int d1 = m_major    - vr. m_major;
			int d2 = m_minor    - vr. m_minor;
			int d3 = m_revision - vr. m_revision;

			return d1 ? d1 : ( d2 ? d2 : d3 );
		}

		int m_major;
		int m_minor;
		int m_revision;

		enum Type {
			Stable,
			Beta
		} m_type;

		bool m_has_errors;
		bool m_is_newer;
		bool m_is_current;
	};

private:
	CProgressDialog *m_progress;
	VersionRecord   m_current_version;
	Q3ValueList <VersionRecord> m_versions;
    QString m_url;
    CKeyValueList m_query;
};

#endif
