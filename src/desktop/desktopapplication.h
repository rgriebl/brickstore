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

#include <QCommandLineParser>

#include "common/application.h"


class DesktopApplication : public Application
{
    Q_OBJECT

public:
    DesktopApplication(int &argc, char **argv);
    ~DesktopApplication() override;

    void init() override;

    void checkRestart() override;

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

    QCoro::Task<bool> closeAllViews() override;

private:
    bool notifyOtherInstance();
    void setUiTheme();
    void setDesktopIconTheme();

private:
    qreal m_defaultFontSize = 0;
    bool m_restart = false;
    QCommandLineParser m_clp;
};

