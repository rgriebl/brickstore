// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QVersionNumber>

#include <QCoro/QCoroTask>


class CheckForUpdates : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        NotifyBeforeUpdate,
        NotifyAfterUpdate
    };

    ~CheckForUpdates() override;

    static CheckForUpdates *inst();
    void initialize(const QString &baseUrl, Mode mode);

    QCoro::Task<> check(bool silent);

signals:
    void versionWasUpdated(const QVersionNumber &version, const QString &changeLog, const QUrl &releaseUrl);
    void versionCanBeUpdated(const QVersionNumber &version, const QString &changeLog, const QUrl &releaseUrl,
                             const QString &installerName, const QUrl &installerUrl);

private:
    CheckForUpdates(QObject *parent = nullptr);

    QString changeLogHeader(const QString &title, const QVersionNumber &version);
    QCoro::Task<QString> versionChangeLog(QVersionNumber fromVersion, QVersionNumber toVersion);

    QNetworkAccessManager m_nam;
    QString m_checkUrl;
    QString m_changelogUrl;
    QString m_releaseUrl;
    QString m_installerName;
    QString m_installerUrl;
    QVersionNumber m_currentVersion;
    bool m_checking = false;
    bool m_silent = false;
    bool m_initialized = false;
    QVersionNumber m_lastRunVersion;
    Mode m_mode = Mode::NotifyBeforeUpdate;

    static CheckForUpdates *s_inst;
};
