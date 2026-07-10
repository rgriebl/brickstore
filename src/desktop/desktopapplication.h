// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QCommandLineParser>

#include "common/application.h"


class DeveloperConsole;
class ToastMessage;

class DesktopApplication : public Application
{
    Q_OBJECT

public:
    static DesktopApplication *inst() { return qobject_cast<DesktopApplication *>(s_inst); }

    DesktopApplication(int &argc, char **argv);
    ~DesktopApplication() override;

    void init() override;

    static void checkRestart();
    DeveloperConsole *developerConsole();

    bool isWaitingForUserInput() const override;

    QCoro::Task<> shutdown();

protected:
    void setupLogging() override;

    QCoro::Task<bool> closeAllDocuments() override;

private:
    bool notifyOtherInstance();
    void setUITheme();
    void setDesktopIconTheme();

private:
    double m_defaultFontSize = 0;
    static bool s_restart;
    QCommandLineParser m_clp;
    QPointer<DeveloperConsole> m_devConsole;
};

