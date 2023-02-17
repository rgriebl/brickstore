// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QCommandLineParser>


class BackendApplication : public QObject
{
    Q_OBJECT

public:
    BackendApplication(int &argc, char **argv);
    ~BackendApplication() override;

    void init();
    void afterInit();
    void checkRestart();

private:
    QCommandLineParser m_clp;
};
