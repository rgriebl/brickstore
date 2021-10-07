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

#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQmlContext)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)

class Script;
class QmlBrickLink;
class QmlBrickStore;


class ScriptManager : public QObject
{
    Q_OBJECT

private:
    ScriptManager();
    static ScriptManager *s_inst;

public:
    ~ScriptManager() override;
    static ScriptManager *inst();

    void initialize();

    bool reload();

    QVector<Script *> scripts() const;

    bool executeString(const QString &s);

signals:
    void aboutToReload();
    void reloaded();

private:
    void clearScripts();
    void loadScript(const QString &fileName);

    QVector<Script *> m_scripts;

    QScopedPointer<QQmlEngine> m_engine;
    QObject *m_rootObject = nullptr;

    Q_DISABLE_COPY(ScriptManager)
};
