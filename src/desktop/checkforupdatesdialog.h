// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QVersionNumber>

#include <QCoro/QCoroTask>

QT_FORWARD_DECLARE_CLASS(QWidget)
class CheckForUpdates;


class CheckForUpdatesDialog : public QObject
{
    Q_OBJECT

public:
    CheckForUpdatesDialog(QWidget *parent);

    void showVersionChanges(QVersionNumber version, QString changeLog, QUrl releaseUrl,
                            QString installerName, QUrl installerUrl);
protected:
    bool event(QEvent *e) override;

private:
    QCoro::Task<> downloadInstaller(QString installerName, QUrl installerUrl);
    void languageChange();

private:
    QWidget *m_parent;
    QString m_title;
    QString m_updatesPath;
};
