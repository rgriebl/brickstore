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

#include <QCommandLineParser>
#include <QTimer>
#include <QMutex>

#include "common/application.h"


class DeveloperConsole;


class DesktopApplication : public Application
{
    Q_OBJECT

public:
    static DesktopApplication *inst() { return qobject_cast<DesktopApplication *>(s_inst); }

    DesktopApplication(int &argc, char **argv);
    ~DesktopApplication() override;

    void init() override;

    void checkRestart() override;
    DeveloperConsole *developerConsole();

protected:
    void setupLogging() override;

    QCoro::Task<bool> closeAllViews() override;

private:
    bool notifyOtherInstance();
    void setUiTheme();
    void setDesktopIconTheme();

private:
    qreal m_defaultFontSize = 0;
    bool m_restart = false;
    QCommandLineParser m_clp;
    QPointer<DeveloperConsole> m_devConsole;
    QTimer m_loggingTimer;
    QMutex m_loggingMutex;
    QVector<std::tuple<QtMsgType, QMessageLogContext *, QString>> m_loggingMessages;
};

