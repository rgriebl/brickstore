// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtQml/qqmlregistration.h>

#include "global.h"
#include "lot.h"

class TransferJob;


namespace BrickLink {

class Store : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(bool valid READ isValid NOTIFY isValidChanged FINAL)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged FINAL)
    Q_PROPERTY(QString currencyCode READ currencyCode NOTIFY currencyCodeChanged FINAL)
    Q_PROPERTY(int lotCount READ lotCount NOTIFY lotCountChanged FINAL)

public:
    ~Store() override;

    bool isValid() const          { return m_valid; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    BrickLink::UpdateStatus updateStatus() const  { return m_updateStatus; }
    int lotCount() const          { return int(m_lots.count()); }
    const LotList &lots() const   { return m_lots; }
    QString currencyCode() const  { return m_currencyCode; }

    Q_INVOKABLE bool startUpdate();
    Q_INVOKABLE void cancelUpdate();

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void isValidChanged(bool valid);
    void updateStatusChanged(BrickLink::UpdateStatus updateStatus);
    void lastUpdatedChanged(const QDateTime &lastUpdated);
    void currencyCodeChanged(const QString &currencyCode);
    void lotCountChanged(int lotCount);

private:
    Store(Core *core);
    void setUpdateStatus(UpdateStatus updateStatus);
    void setLastUpdated(const QDateTime &lastUpdated);

    Core *m_core;
    bool m_valid = false;
    UpdateStatus m_updateStatus = UpdateStatus::UpdateFailed;
    TransferJob *m_job = nullptr;
    LotList m_lots;
    QDateTime m_lastUpdated;
    QString m_currencyCode;

    friend class Core;
};

} // namespace BrickLink
