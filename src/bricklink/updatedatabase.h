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
#include "utility/transfer.h"


class UpdateDatabase : public QObject
{
    Q_OBJECT
public:
    UpdateDatabase(QObject *parent = nullptr);
    bool start(); // no force
    bool start(bool force);
    void cancel();

signals:
    void started();
    void progress(int progress, int total);
    void finished(bool success, const QString &error);

private:
    Transfer m_trans;
};
