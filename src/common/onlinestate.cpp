// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QNetworkInformation>
#include <QDebug>

#include "onlinestate.h"


OnlineState *OnlineState::s_inst = nullptr;

OnlineState *OnlineState::inst()
{
    if (!s_inst)
        s_inst = new OnlineState();
    return s_inst;
}

OnlineState::OnlineState(QObject *parent)
    : QObject(parent)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    if (!QNetworkInformation::load(QNetworkInformation::Feature::Reachability)) {
#elif QT_VERSION >= QT_VERSION_CHECK(6, 4, 0) && defined(Q_OS_LINUX)
    // the glib one works way better, plus the NM (default) one is blocked inside snaps
    if (!QNetworkInformation::loadBackendByName(u"glib") && !QNetworkInformation::loadDefaultBackend()) {
#else
    if (!QNetworkInformation::loadDefaultBackend()) {
#endif
        qWarning() << "Failed to load QNetworkInformation's default backend";
        return;
    }
    auto *qni = QNetworkInformation::instance();
    if (qni->supports(QNetworkInformation::Feature::Reachability)) {
        m_online = (qni->reachability() == QNetworkInformation::Reachability::Online);

        connect(qni, &QNetworkInformation::reachabilityChanged,
                this, [this](QNetworkInformation::Reachability r) {
            auto online = (r == QNetworkInformation::Reachability::Online);
            if (online != m_online) {
                m_online = online;
                emit onlineStateChanged(m_online);
            }
        });
    }
}

#include "moc_onlinestate.cpp"
