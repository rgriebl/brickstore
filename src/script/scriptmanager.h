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

#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQmlContext)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)

class Script;

namespace QmlWrapper {
class BrickLink;
class BrickStore;
}


class ScriptManager : public QObject
{
    Q_OBJECT

private:
    ScriptManager();
    static ScriptManager *s_inst;

public:
    ~ScriptManager();
    static ScriptManager *inst();

    bool initialize(::BrickLink::Core *core);

    bool reload();

    QVector<Script *> scripts() const;
    QVector<Script *> extensionScripts() const;
    QVector<Script *> printingScripts() const;

private:
    void clearScripts();
    void loadScript(const QString &fileName);
    void redirectQmlEngineWarnings(QQmlEngine *engine);

    QVector<Script *> m_scripts;

    QmlWrapper::BrickLink *m_brickLink = nullptr;
    QmlWrapper::BrickStore *m_brickStore = nullptr;

    Q_DISABLE_COPY(ScriptManager)
};
