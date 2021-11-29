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
#include <QtGlobal>
#include <QThread>
#if defined(Q_OS_WINDOWS)
#  if defined(Q_CC_MINGW)
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <windows.h>
#  include <wininet.h>
#  include <QProcess>
#elif defined(Q_OS_MACOS)
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <SystemConfiguration/SCNetworkReachability.h>
#elif defined(Q_OS_LINUX)
#  include <QProcess>
#endif
#include "utility/utility.h"
#include "onlinestate.h"


#define CHECK_IP "178.63.92.134" // brickforge.de

class CheckThread : public QThread
{
public:
    CheckThread(OnlineState *onlineState)
        : QThread(onlineState)
        , m_onlineState(onlineState)
    { }

    void run() override
    {
        while (!isInterruptionRequested()) {
            bool online = OnlineState::checkOnline();
            if (online != m_onlineState->m_online) {
                QMetaObject::invokeMethod(m_onlineState, [this, online]() {
                    m_onlineState->setOnline(online);
                }, Qt::QueuedConnection);
            }

            const int msecDelay = 5000;
            const int msecStep = 100;

            for (int i = 0; i < msecDelay; i += msecStep) {
                if (isInterruptionRequested())
                    return;
                msleep(msecStep);
            }
        }
    }
private:
    OnlineState *m_onlineState;
};


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
    m_checkThread = new CheckThread(this);
    m_checkThread->start();
}

OnlineState::~OnlineState()
{
    m_checkThread->requestInterruption();
    m_checkThread->wait();
}

bool OnlineState::isOnline() const
{
    return m_online;
}

void OnlineState::setOnline(bool online)
{
    if (online != m_online) {
        m_online = online;
        emit onlineStateChanged(m_online);
    }
}

bool OnlineState::checkOnline()
{
    bool online = false;

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    int res = ::system("ip route get " CHECK_IP "/32 >/dev/null 2>/dev/null");

    //qWarning() << "Linux NET change: " << res << WIFEXITED(res) << WEXITSTATUS(res);
    if (!WIFEXITED(res) || (WEXITSTATUS(res) == 0 || WEXITSTATUS(res) == 127))
        online = true;

#elif defined(Q_OS_MACOS)
    static SCNetworkReachabilityRef target = nullptr;

    if (!target) {
        struct ::sockaddr_in sock;
        sock.sin_family = AF_INET;
        sock.sin_port = htons(80);
        sock.sin_addr.s_addr = inet_addr(CHECK_IP);

        target = SCNetworkReachabilityCreateWithAddress(nullptr, reinterpret_cast<sockaddr *>(&sock));
        // in theory we should CFRelease(target) when exitting
    }

    SCNetworkReachabilityFlags flags = 0;
    if (!target || !SCNetworkReachabilityGetFlags(target, &flags)
            || ((flags & (kSCNetworkReachabilityFlagsReachable | kSCNetworkReachabilityFlagsConnectionRequired))
                         == kSCNetworkReachabilityFlagsReachable)) {
        online = true;
    }

#elif defined(Q_OS_WINDOWS) && !defined(Q_CC_MINGW)
    DWORD flags;
    online = InternetGetConnectedStateEx(&flags, nullptr, 0, 0);

#else
    online = true;
#endif
    return online;
}

#include "moc_onlinestate.cpp"
