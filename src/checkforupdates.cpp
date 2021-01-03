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
#include <QBuffer>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonValue>

#include "application.h"
#include "config.h"
#include "progressdialog.h"
#include "checkforupdates.h"


CheckForUpdates::CheckForUpdates(ProgressDialog *pd)
    : m_progress(pd)
{
    connect(pd, SIGNAL(transferFinished()), this, SLOT(gotten()));

    const QString &appver = QCoreApplication::applicationVersion();
    m_current_version = QVersionNumber::fromString(appver);

    pd->setAutoClose(false);
    pd->setHeaderText(tr("Checking for program updates"));
    pd->setMessageText(tr("You are currently running %1 %2").arg(QCoreApplication::applicationName(), appver));


    // m_error = tr( "Could not retrieve version information from server:<br /><br />%1" );

    QString url = Application::inst()->applicationUrl();
    url.replace("github.com", "https://api.github.com/repos");
    url.append("/releases/latest");
    pd->get(url);
}

void CheckForUpdates::gotten()
{
    TransferJob *j = m_progress->job();
    QByteArray *data = j->data();
    bool ok = false;

    if (data && !data->isEmpty()) {
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(*data, &error);
        if (doc.isNull()) {
            qWarning() << data->constData();
            qWarning() << "\nCould not parse GitHub JSON reply:" << error.errorString();
            m_progress->setErrorText(tr("Could not parse server response."));
        } else {
            QString tag = doc["tag_name"].toString();
            if (tag.startsWith('v'))
                tag.remove(0, 1);
            QVersionNumber version = QVersionNumber::fromString(tag);
            if (version.isNull()) {
                qWarning() << "Cannot parse GitHub's latest tag_name:" << tag;
                m_progress->setErrorText(tr("Version information is not available."));
            } else {
                m_latest_version = version;
                QString str;
                if (m_latest_version <= m_current_version) {
                    str = tr("Your currently installed version is up-to-date.");
                } else {
                    str = tr("A newer version than the one currently installed is available:");
                    str += QString::fromLatin1("<br/><br/><strong>%1</strong><br/><br/>")
                            .arg(m_latest_version.toString());
                    str += QString::fromLatin1(R"(<a href="https://%1/blob/master/CHANGELOG.md">%2</a>)")
                            .arg(Application::inst()->applicationUrl())
                            .arg(tr("Detailed list of changes"));
                }
                m_progress->setMessageText(str);
                ok = true;
            }
        }
    }
    if (ok)
        m_progress->setProgressVisible(false);
    m_progress->setFinished(ok);
}

#include "moc_checkforupdates.cpp"
