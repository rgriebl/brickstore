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
#pragma once

#include <QObject>


class CheckThread;

class OnlineState : public QObject
{
    Q_OBJECT

public:
    static OnlineState *inst();
    ~OnlineState() override;

    bool isOnline() const;

signals:
    void onlineStateChanged(bool isOnline);

private:
    OnlineState(QObject *parent = nullptr);
    void setOnline(bool online);
    static bool checkOnline();

    bool m_online = true;
    CheckThread *m_checkThread;

    static OnlineState *s_inst;

    friend class CheckThread;
};
