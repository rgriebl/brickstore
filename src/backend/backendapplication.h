// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QCommandLineParser>
#include <QCoro/QCoroTask>


class BackendApplication : public QObject
{
    Q_OBJECT

public:
    BackendApplication(int &argc, char **argv);
    ~BackendApplication() override;

    void init();
    void afterInit() { }
    int exec();
    void checkRestart() { }

private:
    QCoro::Task<int> rebuildDatabase();

    QCommandLineParser m_clp;
};
