// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>

class MobileFileOpenHandlerPrivate;

class MobileFileOpenHandler : public QObject
{
    Q_OBJECT

public:
    static MobileFileOpenHandler *create();
    ~MobileFileOpenHandler() override;

private:
    MobileFileOpenHandler(QObject *parent = nullptr);
    Q_INVOKABLE void openUrl(const QUrl &url);

    static MobileFileOpenHandler *s_inst;

    friend class MobileFileOpenHandlerPrivate;
};
