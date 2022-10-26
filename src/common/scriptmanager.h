/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
