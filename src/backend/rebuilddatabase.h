// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QDateTime>

#include "bricklink/global.h"
#include "utility/transfer.h"


class RebuildDatabase : public QObject
{
    Q_OBJECT
public:
    RebuildDatabase(bool skipDownload = false, QObject *parent = nullptr);
    ~RebuildDatabase() override;

    int exec();

private slots:
    void downloadJobFinished(TransferJob *job);

private:
    int error(const QString &);

    bool download();
    bool downloadInventories(const std::vector<BrickLink::Item> &invs, const std::vector<bool> &processedInvs);

private:
    Transfer *m_trans;
    QString m_error;
    bool m_skip_download;
    int m_downloads_in_progress = 0;
    int m_downloads_failed = 0;
    QDateTime m_date;
    QString m_rebrickableApiKey;
    QString m_affiliateApiKey;
};
