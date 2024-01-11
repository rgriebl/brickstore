// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QStringList>
#include <QPointer>
#include <QTimer>
#include <QMutex>
#include <QLoggingCategory>

#include <QCoro/QCoroTask>

QT_FORWARD_DECLARE_CLASS(QTranslator)
QT_FORWARD_DECLARE_CLASS(QQmlApplicationEngine)
QT_FORWARD_DECLARE_CLASS(QGuiApplication)
QT_FORWARD_DECLARE_CLASS(QMimeData)


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

    QString buildNumber() const;
    QString applicationUrl() const;
    QString gitHubUrl() const;
    QString gitHubPagesUrl() const;
    QString databaseUrl() const;
    QString ldrawUrl() const;

    static void openUrl(const QUrl &url);

    using UILogMessage = std::tuple<QtMsgType, QString, QString, int, QString>;
    using UIMessageHandler = void(*)(const UILogMessage &);

    void setUILoggingHandler(UIMessageHandler callback);

    virtual void checkRestart();
    QCoro::Task<bool> checkBrickLinkLogin();
    QCoro::Task<bool> updateDatabase();

    Announcements *announcements();
    QVariantMap about() const;

    UndoGroup *undoGroup();

    QQmlApplicationEngine *qmlEngine();

    void raise();

    virtual QCoro::Task<bool> closeAllDocuments();

    enum Theme { LightTheme, DarkTheme };
    void setIconTheme(Theme theme);

    void mimeClipboardClear();
    const QMimeData *mimeClipboardGet() const;
    void mimeClipboardSet(QMimeData *data);

signals:
    void openDocument(const QString &fileName);
    void showSettings(const QString &page = { });
    void show3DSettings();
    void showDeveloperConsole();
    void languageChanged();

protected:
    static void setupTerminateHandler();
    virtual void setupLogging();
    virtual void setupQml();
    void redirectQmlEngineWarnings(const QLoggingCategory &cat);

    bool initBrickLink();

    void openQueuedDocuments();
    void updateTranslations();

    static void setupSentry();
    static void shutdownSentry();
    static void checkSentryConsent();
    static void addSentryBreadcrumb(QtMsgType msgType, const QMessageLogContext &msgCtx, const QString &msg);

    QCoro::Task<> restoreLastSession();

    QCoro::Task<> setupLDraw();

protected:
    QStringList m_startupErrors;
    QStringList m_startupMessages;
    QString m_translationOverride;
    QStringList m_queuedDocuments;
    bool m_canEmitOpenDocuments = false;

    std::unique_ptr<QTranslator> m_trans_qt;
    std::unique_ptr<QTranslator> m_trans_brickstore;

    QtMessageHandler m_defaultMessageHandler = nullptr;
    UIMessageHandler m_uiMessageHandler = nullptr;
    QTimer m_loggingTimer;
    QMutex m_loggingMutex;
    QVector<UILogMessage> m_loggingMessages;
    QLoggingCategory::CategoryFilter m_defaultLoggingFilter = nullptr;

    QPointer<Announcements> m_announcements;

    UndoGroup *m_undoGroup;

    std::unique_ptr<QMimeData> m_clipboardMimeData;

    QQmlApplicationEngine *m_engine = nullptr;
    QGuiApplication *m_app = nullptr;
    static Application *s_inst;
};
