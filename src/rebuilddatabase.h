/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QDateTime>

#include "bricklinkfwd.h"
#include "transfer.h"


class RebuildDatabase : public QObject
{
    Q_OBJECT
public:
    RebuildDatabase(bool skipDownload = false);
    ~RebuildDatabase();

    int exec();

private slots:
    void downloadJobFinished(ThreadPoolJob *job);

private:
    int error(const QString &);

    bool download();
    bool downloadInventories(QVector<const BrickLink::Item *> &invs);

private:
    Transfer *m_trans;
    QString m_error;
    bool m_skip_download;
    int m_downloads_in_progress;
    int m_downloads_failed;
    int m_processed;
    int m_ptotal;
    QDateTime m_date;
};
