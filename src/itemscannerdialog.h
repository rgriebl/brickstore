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

#if defined(BS_HAS_OPENCV)

#include <QVector>
#include <QDialog>
#include <QCamera>
#include <QCameraImageCapture>
#include <QElapsedTimer>
#include <QTimer>

#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QCameraViewfinder)
QT_FORWARD_DECLARE_CLASS(QStackedLayout)

class VideoOverlay;


class ItemScannerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ItemScannerDialog(QWidget *parent = nullptr);
    ~ItemScannerDialog() override;

    QVector<const BrickLink::Item *> items() const;

protected:
    void keyPressEvent(QKeyEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void updateCamera(const QCameraInfo &camInfo);
    void capture();
    void onScanStarted(int id, const QVector<QPointF> &points);
    void onScanFinished(int id, const QVector<const BrickLink::Item *> &items);
    void onScanFailed(int id, const QString &error);

    QScopedPointer<QCamera> m_camera;
    QScopedPointer<QCameraImageCapture> m_imageCapture;
    QComboBox *m_selectCamera;
    //QCameraViewfinder *m_viewFinder;
    QVideoWidget *m_viewFinder;
    VideoOverlay *m_overlay;
    QLabel *m_status;
    QProgressBar *m_progress;
    QTimer *m_progressTimer;
    QStackedLayout *m_bottomStack;

    int m_currentScan = 0;
    static int s_averageScanTime;
    QElapsedTimer m_lastScanTime;

    QString m_noCameraText;
    QString m_noDbText;
    QString m_loadingDbText;
    QString m_noMatchText;
    QString m_okText;

    QVector<const BrickLink::Item *> m_items;
};

#endif
