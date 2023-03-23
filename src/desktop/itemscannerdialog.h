// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QVector>
#include <QDialog>
#include <QCamera>
#include <QElapsedTimer>
#include <QTimer>

#include "bricklink/global.h"
#include "common/itemscanner.h"

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QCameraDevice)
QT_FORWARD_DECLARE_CLASS(QCameraViewfinder)
QT_FORWARD_DECLARE_CLASS(QMediaCaptureSession)
QT_FORWARD_DECLARE_CLASS(QImageCapture)
QT_FORWARD_DECLARE_CLASS(QStackedLayout)
QT_FORWARD_DECLARE_CLASS(QVideoWidget)

class VideoOverlay;


class ItemScannerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ItemScannerDialog(const BrickLink::ItemType *itemType, QWidget *parent = nullptr);
    ~ItemScannerDialog() override;

    QVector<const BrickLink::Item *> items() const;

protected:
    void keyPressEvent(QKeyEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void updateCamera(const QByteArray &cameraId);
    void updateCameraResolution(int preferredImageSize);
    void updateBackend(const QByteArray &backendId);
    void capture();
    void onScanStarted(uint id);
    void onScanFinished(uint id, const QVector<ItemScanner::Result> &itemsAndScores);
    void onScanFailed(uint id, const QString &error);

    QScopedPointer<QCamera> m_camera;
    QComboBox *m_selectCamera;
    QComboBox *m_selectBackend;
    QButtonGroup *m_selectItemTypes;
    QVideoWidget *m_viewFinder;
    QMediaCaptureSession *m_captureSession;
    QImageCapture *m_imageCapture;

    QLabel *m_status;
    QProgressBar *m_progress;
    QTimer *m_progressTimer;
    QStackedLayout *m_bottomStack;

    uint m_currentScan = 0;
    static int s_averageScanTime;
    QElapsedTimer m_lastScanTime;

    QString m_noCameraText;
    QString m_noDbText;
    QString m_loadingDbText;
    QString m_noMatchText;
    QString m_okText;

    QVector<const BrickLink::Item *> m_items;
};
