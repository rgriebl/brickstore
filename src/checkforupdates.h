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
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QVersionNumber>

QT_FORWARD_DECLARE_CLASS(QWidget)


class CheckForUpdates : public QObject
{
    Q_OBJECT
public:
    CheckForUpdates(const QString &baseUrl, QWidget *parent);

    void check(bool silent);

protected:
    bool event(QEvent *e) override;

private:
    void languageChange();
    void showVersionChanges(const QVersionNumber &latestVersion);
    void downloadInstaller();

    QNetworkAccessManager m_nam;
    QString m_checkUrl;
    QString m_changelogUrl;
    QString m_downloadUrl;
    QString m_installerName;
    QString m_installerUrl;
    QVersionNumber m_currentVersion;
    QString m_title;
    QWidget *m_parent;
    bool m_checking = false;
    bool m_silent = false;
    QString m_updatesPath;
};

