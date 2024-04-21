// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>

#include <QVector>
#include <QDialog>

#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QStackedLayout)
QT_FORWARD_DECLARE_CLASS(QMediaDevices)

namespace Scanner {
class CameraPreviewWidget;
class Capture;
}


class ItemScannerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ItemScannerDialog(QWidget *parent = nullptr);
    ~ItemScannerDialog() override;

    static void checkSystemPermissions(QObject *context, const std::function<void(bool)> &callback);

    void setItemType(const BrickLink::ItemType *itemType);

signals:
    void itemsScanned(const QVector<const BrickLink::Item *> &items);

protected:
    void changeEvent(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    void updateCameraDevices();
    void updateItemTypeFilters();
    void updateStatusText();
    void languageChange();

    QComboBox *m_selectCamera;
    QComboBox *m_selectBackend;
    QButtonGroup *m_selectItemType;
    QLabel *m_labelCamera;
    QLabel *m_labelBackend;
    QLabel *m_labelItemType;
    Scanner::CameraPreviewWidget *m_cameraPreviewWidget;
    QLabel *m_status;
    QProgressBar *m_progress;
    QStackedLayout *m_bottomStack;
    QToolButton *m_pinWindow;

    QMediaDevices *m_mediaDevices;
    Scanner::Capture *m_capture;
};
