// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QVariantMap>

#include "common/application.h"


class MobileApplication : public Application
{
    Q_OBJECT

public:
    MobileApplication(int &argc, char **argv);
    ~MobileApplication() override;

    void init() override;

protected:
    QCoro::Task<bool> closeAllDocuments() override;
    void setupQml() override;
    void setupLogging() override;

private:
    QVariantMap m_actionMap;
};
