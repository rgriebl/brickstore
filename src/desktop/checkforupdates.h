// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QVersionNumber>

#include <QCoro/QCoroTask>

QT_FORWARD_DECLARE_CLASS(QWidget)


class CheckForUpdates : public QObject
{
    Q_OBJECT
public:
    CheckForUpdates(const QString &baseUrl, QWidget *parent);

    QCoro::Task<> check(bool silent);

protected:
    bool event(QEvent *e) override;

private:
    void languageChange();
    QCoro::Task<> showVersionChanges(QVersionNumber latestVersion);
    QCoro::Task<> downloadInstaller();

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

