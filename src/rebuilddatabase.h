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
    ~RebuildDatabase() override;

    int exec();

private slots:
    void downloadJobFinished(TransferJob *job);

private:
    int error(const QString &);

    bool download();
    bool downloadInventories(const std::vector<const BrickLink::Item *> &invs);

private:
    Transfer *m_trans;
    QString m_error;
    bool m_skip_download;
    int m_downloads_in_progress = 0;
    int m_downloads_failed = 0;
    QDateTime m_date;
};
