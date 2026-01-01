// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QPointer>


QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQmlContext)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)

class Script;


class ScriptManager : public QObject
{
    Q_OBJECT

public:
    ~ScriptManager() override;
    static ScriptManager *create(QQmlEngine *engine);
    static ScriptManager *inst();

    bool reload();

    QVector<Script *> scripts() const;

    std::tuple<QString, bool> executeString(const QString &s);

signals:
    void aboutToReload();
    void reloaded();

private:
    ScriptManager(QQmlEngine *engine);
    static ScriptManager *s_inst;

    void clearScripts();
    void loadScript(const QString &fileName);

    QPointer<QQmlEngine> m_engine;
    QVector<Script *> m_scripts;

    QObject *m_rootObject = nullptr;

    Q_DISABLE_COPY(ScriptManager)
};
