// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>


class TransferJob;

class AwaitableTransferJob : public QObject
{
    Q_OBJECT
public:
    AwaitableTransferJob(TransferJob *job);
    ~AwaitableTransferJob() override;

    operator TransferJob *() const;

    Q_SIGNAL void finished();

private:
    TransferJob *m_job = nullptr;
};
