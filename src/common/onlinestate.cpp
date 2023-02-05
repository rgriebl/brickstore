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
