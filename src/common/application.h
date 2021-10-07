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

#include <QStringList>
#include <QScopedPointer>
#include <QPointer>

#include "qcoro/task.h"

QT_FORWARD_DECLARE_CLASS(QTranslator)

class Announcements;
class UndoGroup;


class Application : public QObject
{
    Q_OBJECT
public:
    static Application *inst() { return s_inst; }

    Application(int &argc, char **argv);
    ~Application() override;

    virtual void init();
    void afterInit();

    QString applicationUrl() const;
    QString gitHubUrl() const;

    void setUILoggingHandler(QtMessageHandler callback);

    virtual void checkRestart();
    bool checkBrickLinkLogin();
    QCoro::Task<bool> updateDatabase();

    Announcements *announcements();
    QVariantMap about() const;

    UndoGroup *undoGroup();

signals:
    void openDocument(const QString &fileName);
    void showSettings(const QString &page = { });
    void languageChanged();

protected:
    void setupLogging();
    enum Theme { LightTheme, DarkTheme };
    void setIconTheme(Theme theme);

    bool initBrickLink(QString *errString);
    void exitBrickLink();

    void openQueuedDocuments();
    void updateTranslations();

    static void setupSentry();
    static void shutdownSentry();
    static void checkSentryConsent();
    static void addSentryBreadcrumb(QtMsgType msgType, const QMessageLogContext &msgCtx, const QString &msg);

    QCoro::Task<> restoreLastSession();

    virtual QCoro::Task<bool> closeAllViews() = 0;

    bool eventFilter(QObject *o, QEvent *e) override;

protected:
    QStringList m_startupErrors;
    QString m_translationOverride;
    QStringList m_queuedDocuments;
    bool m_canEmitOpenDocuments = false;
    qreal m_defaultFontSize = 0;

    QScopedPointer<QTranslator> m_trans_qt;
    QScopedPointer<QTranslator> m_trans_brickstore;

    QtMessageHandler m_defaultMessageHandler = nullptr;
    QtMessageHandler m_uiMessageHandler = nullptr;

    QPointer<Announcements> m_announcements;

    UndoGroup *m_undoGroup;

    static Application *s_inst;
};
