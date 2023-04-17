// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QComboBox>
#include <QToolButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QStackedLayout>
#include <QMediaDevices>
#include <QCamera>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QVideoWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPainter>
#include <QMouseEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(permissions)
#  include <QCoreApplication>
#  include <QPermissions>
#  include <QMessageBox>
#endif

#include "bricklink/core.h"
#include "bricklink/item.h"
#include "common/config.h"
#include "common/itemscanner.h"
#include "itemscannerdialog.h"


int ItemScannerDialog::s_averageScanTime = 0;

bool ItemScannerDialog::checkSystemPermissions()
{
    const QString requestDenied = tr("BrickStore's request for camera access was denied. You will not be able to use your webcam to identify parts until you grant the required permissions via your system's Settings application.");


#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
    QCameraPermission cameraPermission;
    switch (qApp->checkPermission(cameraPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(cameraPermission, [](const QPermission &) { });
        return false;
    case Qt::PermissionStatus::Denied:
        QMessageBox::warning(nullptr, QCoreApplication::applicationName(), requestDenied);
        return false;
    case Qt::PermissionStatus::Granted:
        break; // Proceed
    }
#endif
    return true;
}

ItemScannerDialog::ItemScannerDialog(const BrickLink::ItemType *itemType, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Item Scanner"));
    setSizeGripEnabled(true);

    const auto allCameraDevices = QMediaDevices::videoInputs();
    const auto allBackends = ItemScanner::inst()->availableBackends();

    connect(ItemScanner::inst(), &ItemScanner::scanFinished,
            this, &ItemScannerDialog::onScanFinished);
    connect(ItemScanner::inst(), &ItemScanner::scanFailed,
            this, &ItemScannerDialog::onScanFailed);

    m_selectCamera = new QComboBox;
    m_selectCamera->setFocusPolicy(Qt::NoFocus);
    for (const auto &cameraDevice : allCameraDevices)
        m_selectCamera->addItem(cameraDevice.description(), cameraDevice.id());

    m_selectBackend = new QComboBox;
    m_selectBackend->setFocusPolicy(Qt::NoFocus);
    for (const auto &backend : allBackends)
        m_selectBackend->addItem(backend.name, backend.id);

    m_selectItemTypes = new QButtonGroup(this);
    auto *itemTypesWidget = new QWidget;
    itemTypesWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *itemTypesLayout = new QHBoxLayout(itemTypesWidget);
    itemTypesLayout->setContentsMargins(0, 0, 0, 0);
    itemTypesLayout->setSpacing(2);

    auto createTypeButton = [&](const BrickLink::ItemType *itt) {
        auto *b = new QToolButton;
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        b->setAutoRaise(true);
        b->setCheckable(true);
        b->setChecked(itt == itemType);
        b->setText(itt ? itt->name() : tr("Any"));
        m_selectItemTypes->addButton(b, itt ? itt->id() : '*');
        itemTypesLayout->addWidget(b, 1);
    };

    createTypeButton(nullptr);
    for (const auto &itt : BrickLink::core()->itemTypes())
        createTypeButton(&itt);

    m_viewFinder = new QVideoWidget;
    int videoWidth = logicalDpiX() * 3; // ~7.5cm on-screen
    m_viewFinder->setMinimumSize(videoWidth, videoWidth * 9 / 16);
    m_viewFinder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_viewFinder->installEventFilter(this);

    m_captureSession = new QMediaCaptureSession(this);
    m_captureSession->setVideoOutput(m_viewFinder);

    m_imageCapture = new QImageCapture(m_captureSession);
    m_captureSession->setImageCapture(m_imageCapture);

    m_progress = new QProgressBar();
    m_progress->setTextVisible(false);
    m_progressTimer = new QTimer(m_progress);
    m_progressTimer->setInterval(30);
    connect(m_progressTimer, &QTimer::timeout,
            this, [this]() {
                m_progress->setValue(qMin(int(m_lastScanTime.elapsed()), m_progress->maximum()));
            });

    m_status = new QLabel();
    QPalette pal = m_status->palette();
    pal.setColor(QPalette::Text, pal.color(QPalette::Active, QPalette::Text));
    m_status->setPalette(pal);
    m_status->setTextFormat(Qt::RichText);
    m_status->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->addRow(new QLabel(tr("Camera")), m_selectCamera);
    form->addRow(new QLabel(tr("Service")), m_selectBackend);
    form->addRow(new QLabel(tr("Item type")), itemTypesWidget);
    layout->addLayout(form);
    layout->addWidget(m_viewFinder, 100);
    m_bottomStack = new QStackedLayout();
    m_bottomStack->setContentsMargins(0, 0, 0, 0);
    m_bottomStack->addWidget(m_status);
    m_bottomStack->addWidget(m_progress);
    layout->addLayout(m_bottomStack, 0);

    m_selectCamera->setCurrentIndex(-1);
    connect(m_selectCamera, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0)
            updateCamera(m_selectCamera->itemData(index).toByteArray());
    });
    m_selectBackend->setCurrentIndex(-1);
    connect(m_selectBackend, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0)
            updateBackend(m_selectBackend->itemData(index).toByteArray());
    });

    const QByteArray cameraId = Config::inst()->value(u"MainWindow/ItemScannerDialog/CameraId"_qs).toByteArray();
    int cameraIndex = m_selectCamera->findData(cameraId);
    m_selectCamera->setCurrentIndex(cameraIndex < 0 ? 0 : cameraIndex);

    const QByteArray backendId = Config::inst()->value(u"MainWindow/ItemScannerDialog/BackendId"_qs).toByteArray();
    int backendIndex = m_selectBackend->findData(backendId);
    m_selectBackend->setCurrentIndex(backendIndex < 0 ? 0 : backendIndex);

    m_okText = tr("Click into the camera preview or press Space to capture an image.");
    m_noCameraText = u"<b>" + tr("There is no camera connected to this computer.") + u"</b>";
    m_noMatchText = u"<b>" + tr("No matching item found - try again.") + u"</b><br>" + m_okText;

    if (allCameraDevices.isEmpty()) {
        setEnabled(false);
        m_status->setText(m_noCameraText);
    } else {
        m_status->setText(m_okText);
    }

    restoreGeometry(Config::inst()->value(u"MainWindow/ItemScannerDialog/Geometry"_qs).toByteArray());
}

ItemScannerDialog::~ItemScannerDialog()
{
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/Geometry"_qs, saveGeometry());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/CameraId"_qs, m_selectCamera->currentData().toByteArray());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/BackendId"_qs, m_selectBackend->currentData().toByteArray());
}

void ItemScannerDialog::updateCamera(const QByteArray &cameraId)
{
    QCamera *camera = nullptr;

    const auto allCameraDevices = QMediaDevices::videoInputs();
    for (const auto &cameraDevice : allCameraDevices) {
        if (cameraDevice.id() == cameraId) {
            camera = new QCamera(cameraDevice);
            break;
        }
    }

    if (!camera)
        return;

    m_camera.reset(camera);
    m_captureSession->setCamera(camera);
    m_camera->start();

    updateCameraResolution(1920);

    connect(m_imageCapture, &QImageCapture::imageCaptured,
            this, [this](int id, const QImage &img) {
        Q_UNUSED(id)

        m_lastScanTime.restart();

        // m_camera->setViewfinder(static_cast<QVideoWidget *>(nullptr));
        // m_overlay->pause(rawimg);

        m_currentScan = ItemScanner::inst()->scan(img, char(m_selectItemTypes->checkedId()),
                                                  m_selectBackend->currentData().toByteArray());

        if (!m_currentScan) {
            onScanFailed(0, tr("Scanning failed"));
            return;
        } else {
            onScanStarted(m_currentScan);
        }

        m_progress->setValue(0);
        if (s_averageScanTime) {
            m_progress->setRange(0, s_averageScanTime);
            m_progressTimer->start();
        } else {
            m_progress->setRange(0, 0);
        }
        m_bottomStack->setCurrentWidget(m_progress);
    });
}

void ItemScannerDialog::updateCameraResolution(int preferredImageSize)
{
    if (!m_camera)
        return;

    QList<QSize> allRes = m_camera->cameraDevice().photoResolutions();

    if (!allRes.isEmpty()) {
        std::sort(allRes.begin(), allRes.end(), [](const auto &r1, const auto &r2) {
            return r1.width() < r2.width();
        });

        QSize usedRes = allRes.at(0);

        for (const QSize &s : allRes) {
            if (s.width() > usedRes.width())
                usedRes = s;
            if (s.width() >= preferredImageSize) // try to find a better one
                break;
        }

        m_imageCapture->setResolution(usedRes);
    }
}

void ItemScannerDialog::updateBackend(const QByteArray &backendId)
{
    QByteArray itemTypeIds;
    int preferredImageSize = 0;

    for (const auto &backend : ItemScanner::inst()->availableBackends()) {
        if (backend.id == backendId) {
            itemTypeIds = backend.itemTypeFilter;
            preferredImageSize = backend.preferredImageSize;
            break;
        }
    }

    auto currentChecked = m_selectItemTypes->checkedId();
    QAbstractButton *first = nullptr;

    const auto buttons = m_selectItemTypes->buttons();
    for (auto *b : buttons) {
        int id = m_selectItemTypes->id(b);
        bool valid = itemTypeIds.contains(char(id));
        b->setVisible(valid);
        b->setChecked(valid && (id == currentChecked));
        if (!first && valid)
            first = b;
    }
    if ((m_selectItemTypes->checkedId() == -1) && first)
        first->setChecked(true);

    updateCameraResolution(preferredImageSize);
}

QVector<const BrickLink::Item *> ItemScannerDialog::items() const
{
    return m_items;
}

void ItemScannerDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space)
        capture();
    QDialog::keyPressEvent(e);
}

bool ItemScannerDialog::eventFilter(QObject *o, QEvent *e)
{
    if ((o == m_viewFinder) && (e->type() == QEvent::MouseButtonPress) && isEnabled())
        capture();
    return QDialog::eventFilter(o, e);
}

void ItemScannerDialog::capture()
{
    if (!m_currentScan) {
        m_items.clear();
        m_imageCapture->capture();
    }
}

void ItemScannerDialog::onScanStarted(uint id)
{
    if (id == m_currentScan) {
        //m_overlay->pausePoints(points);
    }
}

void ItemScannerDialog::onScanFinished(uint id, const QVector<ItemScanner::Result> &itemsAndScores)
{
    if (id == m_currentScan) {
        m_currentScan = 0;
        m_items.clear();
        for (const auto &is : itemsAndScores)
            m_items << is.item;

        m_bottomStack->setCurrentWidget(m_status);
        m_progressTimer->stop();

        int elapsed = m_lastScanTime.elapsed();
        s_averageScanTime = s_averageScanTime ? (s_averageScanTime + elapsed) / 2 : elapsed;

        if (m_items.isEmpty()) {
            m_status->setText(m_noMatchText);
//            m_overlay->unpause();
//            m_camera->setViewfinder(m_overlay);
        } else {
            accept();
        }
    }
}

void ItemScannerDialog::onScanFailed(uint id, const QString &error)
{
    if (id == m_currentScan) {
        m_currentScan = 0;

        m_bottomStack->setCurrentWidget(m_status);
        m_progressTimer->stop();

//        m_overlay->unpause();
//        m_camera->setViewfinder(m_overlay);

        m_status->setText(u"<b>"_qs + tr("An error occured:") + u"</b><br>" + error);
    }
}


#include "moc_itemscannerdialog.cpp"
