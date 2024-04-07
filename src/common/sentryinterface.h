// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <QDebug>

class SentryInterfacePrivate;

class SentryInterface
{
public:
    static bool isEnabled();

    SentryInterface(const char *dsn, const char *release);
    ~SentryInterface();
    void setDebug(bool on);
    void setUserConsentRequired(bool on);
    bool init();
    void close();

    void userConsentReset();
    void userConsentGive();
    void userConsentRevoke();

    void setTag(const QByteArray &key, const QByteArray &value);
    void addBreadcrumb(QtMsgType msgType, const QMessageLogContext &msgCtx, const QString &msg);

private:
    std::unique_ptr<SentryInterfacePrivate> d;
};
