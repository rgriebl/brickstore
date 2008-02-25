/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __CREBUILDDATABASE_H__
#define __CREBUILDDATABASE_H__

#include <QObject>
#include <QDateTime>

#include "bricklink.h"
#include "ctransfer.h"


class CRebuildDatabase : public QObject {
    Q_OBJECT

public:
    CRebuildDatabase(const QString &output);
    ~CRebuildDatabase();

    int exec();

private slots:
    void downloadJobFinished(CTransferJob *job);

private:
    int error(const QString &);

    bool download();
    bool downloadInventories(QVector<const BrickLink::Item *> &invs);

private:
    CTransfer *m_trans;
    QString m_output;
    QString m_error;
    int m_downloads_in_progress;
    int m_downloads_failed;
    int m_processed;
    int m_ptotal;
    QDateTime m_date;
};

#endif
