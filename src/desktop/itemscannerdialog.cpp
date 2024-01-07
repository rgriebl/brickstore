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
#include <QLabel>
#include <QProgressBar>
#include <QPainter>
#include <QMouseEvent>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(permissions)
#  include <QCoreApplication>
#  include <QMessageBox>
#  include <QPermissions>
#endif

#include "bricklink/core.h"
#include "bricklink/item.h"
#include <common/application.h>
#include "common/config.h"
#include "scanner/itemscanner.h"
#include "scanner/camerapreviewwidget.h"
#include "itemscannerdialog.h"

using namespace std::chrono_literals;

static struct SetQtMMBackend  // clazy:exclude=non-pod-global-static
{
    // QTBUG-120026: The default FFmpeg backend completely freezes the main thread for 1-3 sec
    // when enumerating camera devices.
    SetQtMMBackend()
    {
#if defined(Q_OS_WINDOWS)
        qputenv("QT_MEDIA_BACKEND", "windows");
#elif defined(Q_OS_MACOS)
        qputenv("QT_MEDIA_BACKEND", "darwin");
#elif defined(Q_OS_ANDROID)
        qputenv("QT_MEDIA_BACKEND", "android");
#elif defined(Q_OS_LINUX)
        qputenv("QT_MEDIA_BACKEND", "gstreamer");
#endif
    }
} setQtMMBackend;


int ItemScannerDialog::s_averageScanTime = 1500;
bool ItemScannerDialog::s_hasCameraPermission = false;

bool ItemScannerDialog::checkSystemPermissions()
{
    if (s_hasCameraPermission)
        return true;

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
#else
    Q_UNUSED(requestDenied)
#endif
    s_hasCameraPermission = true;
    return true;
}

ItemScannerDialog::ItemScannerDialog(QWidget *parent)
    : QDialog(parent)
    , m_mediaDevices(std::make_unique<QMediaDevices>())
{
    connect(m_mediaDevices.get(), &QMediaDevices::videoInputsChanged,
            this, &ItemScannerDialog::updateCameraDevices);

    setSizeGripEnabled(true);

    const auto allBackends = ItemScanner::inst()->availableBackends();
    connect(ItemScanner::inst(), &ItemScanner::scanFinished,
            this, &ItemScannerDialog::onScanFinished);
    connect(ItemScanner::inst(), &ItemScanner::scanFailed,
            this, &ItemScannerDialog::onScanFailed);

    m_selectCamera = new QComboBox(this);
    m_selectCamera->setFocusPolicy(Qt::NoFocus);

    m_selectBackend = new QComboBox(this);
    m_selectBackend->setFocusPolicy(Qt::NoFocus);
    for (const auto &backend : allBackends)
        m_selectBackend->addItem(backend.icon(), backend.name, backend.id);

    m_selectItemType = new QButtonGroup(this);
    auto *itemTypesWidget = new QWidget(this);
    itemTypesWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *itemTypesLayout = new QHBoxLayout(itemTypesWidget);
    itemTypesLayout->setContentsMargins(0, 0, 0, 0);
    itemTypesLayout->setSpacing(2);

    auto createTypeButton = [&, this](const BrickLink::ItemType *itt) {
        auto *b = new QToolButton(this);
        b->setFocusPolicy(Qt::NoFocus);
        b->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        b->setAutoRaise(true);
        b->setCheckable(true);
        b->setChecked(!itt);
        b->setText(itt ? itt->name() : tr("Any"));
        m_selectItemType->addButton(b, itt ? itt->id() : '*');
        itemTypesLayout->addWidget(b, 1);
    };

    createTypeButton(nullptr);
    for (const auto &itt : BrickLink::core()->itemTypes())
        createTypeButton(&itt);

    m_captureSession = new QMediaCaptureSession(this);

    m_cameraPreviewWidget = new CameraPreviewWidget(Application::inst()->qmlEngine(), this);
    m_captureSession->setVideoOutput(m_cameraPreviewWidget->videoOutput());
    setFocusProxy(m_cameraPreviewWidget);
    m_cameraPreviewWidget->setFocus();

    connect(m_cameraPreviewWidget, &CameraPreviewWidget::clicked,
            this, &ItemScannerDialog::capture);

    m_imageCapture = new QImageCapture(m_captureSession);
    m_captureSession->setImageCapture(m_imageCapture);

    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progressTimer = new QTimer(m_progress);
    m_progressTimer->setInterval(30ms);
    connect(m_progressTimer, &QTimer::timeout,
            this, [this]() {
        m_progress->setValue(qMin(int(m_lastScanTime.elapsed()), m_progress->maximum()));
    });

    m_status = new QLabel(this);
    QPalette pal = m_status->palette();
    pal.setColor(QPalette::Text, pal.color(QPalette::Active, QPalette::Text));
    m_status->setPalette(pal);
    m_status->setTextFormat(Qt::RichText);
    m_status->setAlignment(Qt::AlignCenter);

    m_pinWindow = new QToolButton(this);
    m_pinWindow->setFocusPolicy(Qt::NoFocus);
    m_pinWindow->setCheckable(true);
    m_pinWindow->setAutoRaise(true);

    auto updatePinIcon = [this]() {
        bool b = m_pinWindow->isChecked();
        m_pinWindow->setIcon(QIcon::fromTheme(b ? u"window-unpin"_qs : u"window-pin"_qs));
    };
    connect(m_pinWindow, &QToolButton::toggled, this, updatePinIcon);
    updatePinIcon();

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->addRow(m_labelCamera = new QLabel(this), m_selectCamera);
    form->addRow(m_labelBackend = new QLabel(this), m_selectBackend);
    form->addRow(m_labelItemType = new QLabel(this), itemTypesWidget);
    layout->addLayout(form);
    layout->addWidget(m_cameraPreviewWidget, 100);
    m_bottomStack = new QStackedLayout();
    m_bottomStack->setContentsMargins(0, 0, 0, 0);
    m_bottomStack->addWidget(m_status);
    m_bottomStack->addWidget(m_progress);
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(m_pinWindow);
    bottomLayout->addLayout(m_bottomStack, 1);
    layout->addLayout(bottomLayout, 0);

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

    updateCameraDevices();

    const QByteArray cameraId = Config::inst()->value(u"MainWindow/ItemScannerDialog/CameraId"_qs).toByteArray();
    int cameraIndex = m_selectCamera->findData(cameraId);
    m_selectCamera->setCurrentIndex(cameraIndex < 0 ? 0 : cameraIndex);

    const QByteArray backendId = Config::inst()->value(u"MainWindow/ItemScannerDialog/BackendId"_qs).toByteArray();
    int backendIndex = m_selectBackend->findData(backendId);
    m_selectBackend->setCurrentIndex(backendIndex < 0 ? 0 : backendIndex);

    languageChange();

    restoreGeometry(Config::inst()->value(u"MainWindow/ItemScannerDialog/Geometry"_qs).toByteArray());
    m_pinWindow->setChecked(Config::inst()->value(u"MainWindow/ItemScannerDialog/Pinned"_qs, false).toBool());

    m_errorMessageTimeout.setInterval(10s);
    m_errorMessageTimeout.setSingleShot(true);
    connect(&m_errorMessageTimeout, &QTimer::timeout, this, [this]() {
        if (m_state == State::Error)
            setState(State::Idle);
    });
    m_noMatchMessageTimeout.setInterval(10s);
    m_noMatchMessageTimeout.setSingleShot(true);
    connect(&m_noMatchMessageTimeout, &QTimer::timeout, this, [this]() {
        if (m_state == State::NoMatch)
            setState(State::Idle);
    });

    m_cameraStopTimer.setInterval(10s);
    m_cameraStopTimer.setSingleShot(true);
    connect(&m_cameraStopTimer, &QTimer::timeout, this, [this]() {
        if (m_state == State::SoonInactive)
            setState(State::Inactive);
    });
}

ItemScannerDialog::~ItemScannerDialog()
{
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/Geometry"_qs, saveGeometry());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/CameraId"_qs, m_selectCamera->currentData().toByteArray());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/BackendId"_qs, m_selectBackend->currentData().toByteArray());
    Config::inst()->setValue(u"MainWindow/ItemScannerDialog/Pinned"_qs, m_pinWindow->isChecked());
}

void ItemScannerDialog::setItemType(const BrickLink::ItemType *itemType)
{
    QAbstractButton *first = nullptr;
    bool checkedAny = false;

    const auto buttons = m_selectItemType->buttons();
    for (auto *b : buttons) {
        int id = m_selectItemType->id(b);
        const auto itt = BrickLink::core()->itemType(char(id));
        bool valid = m_validItemTypes.contains(itt);
        bool checked = valid && (itt == itemType);
        b->setVisible(valid);
        b->setChecked(checked);
        checkedAny = checkedAny || checked;
        if (!first && valid)
            first = b;
    }
    if (!checkedAny && first)
        first->setChecked(true);
}

void ItemScannerDialog::updateCameraDevices()
{
    const auto allCameraDevices = m_mediaDevices->videoInputs();

    m_selectCamera->clear();
    const auto camIcon = QIcon::fromTheme(u"camera-photo"_qs);
    for (const auto &cameraDevice : allCameraDevices)
        m_selectCamera->addItem(camIcon, cameraDevice.description(), cameraDevice.id());

    setEnabled(!allCameraDevices.isEmpty());
    setState(State::Idle);
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

    if (m_camera) {
        disconnect(m_camera.get(), &QCamera::activeChanged,
                   m_cameraPreviewWidget, &CameraPreviewWidget::setActive);
    }

    if (camera) {
        connect(camera, &QCamera::activeChanged,
                m_cameraPreviewWidget, &CameraPreviewWidget::setActive);
    } else {
        return;
    }

    m_camera.reset(camera);
    m_captureSession->setCamera(camera);
    m_camera->start();


    updateCameraResolution(1920);

    connect(m_imageCapture, &QImageCapture::imageCaptured,
            this, [this](int id, const QImage &img) {
        Q_UNUSED(id)

        m_lastScanTime.restart();
        m_currentScan = ItemScanner::inst()->scan(img, char(m_selectItemType->checkedId()),
                                                  m_selectBackend->currentData().toByteArray());

        if (!m_currentScan) {
            onScanFailed(0, tr("Scanning failed"));
            return;
        }

        m_progress->setValue(0);
        m_progress->setRange(0, s_averageScanTime);
        setState(State::Scaning);
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

    m_validItemTypes.clear();
    for (char c : std::as_const(itemTypeIds)) {
        const auto itt = BrickLink::core()->itemType(c);
        if (itt || (c == '*'))
            m_validItemTypes << itt;
    }

    setItemType(BrickLink::core()->itemType(char(m_selectItemType->checkedId())));
    updateCameraResolution(preferredImageSize);
}

void ItemScannerDialog::changeEvent(QEvent *e)
{
    if ((e->type() == QEvent::ActivationChange) && m_camera) {
        setState(isActiveWindow() ? State::Idle : State::SoonInactive);
    } else if (e->type() == QEvent::LanguageChange) {
        languageChange();
    }
    QDialog::changeEvent(e);
}

void ItemScannerDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space)
        capture();
    QDialog::keyPressEvent(e);
}

void ItemScannerDialog::capture()
{
    if (!m_currentScan) {
        if (m_imageCapture->isReadyForCapture()) {
            m_imageCapture->capture();
        } else {
            // The cam might not be ready yet (e.g. window activation). Wait for it to become ready
            // and try again.
            QElapsedTimer timer;
            timer.start();
            connect(m_imageCapture, &QImageCapture::readyForCaptureChanged,
                    this, [this, timer]() {
                if (!timer.hasExpired(1000) && !m_currentScan && m_imageCapture->isReadyForCapture())
                    m_imageCapture->capture();
            });
        }
    }
}

void ItemScannerDialog::onScanFinished(uint id, const QVector<ItemScanner::Result> &itemsAndScores)
{
    if (id == m_currentScan) {
        m_currentScan = 0;
        QVector<const BrickLink::Item *> items;
        items.reserve(itemsAndScores.size());
        for (const auto &is : itemsAndScores)
            items << is.item;

        int elapsed = m_lastScanTime.elapsed();
        s_averageScanTime = s_averageScanTime ? (s_averageScanTime + elapsed) / 2 : elapsed;

        if (items.isEmpty()) {
            setState(State::NoMatch);
        } else {
            setState(State::Idle);
            if (!m_pinWindow->isChecked())
                accept();
            emit itemsScanned(items);
        }
    }
}

void ItemScannerDialog::onScanFailed(uint id, const QString &error)
{
    if (id == m_currentScan) {
        m_currentScan = 0;
        m_lastError = error;
        setState(State::Error);
    }
}

void ItemScannerDialog::languageChange()
{
    setWindowTitle(tr("Item Scanner"));
    setState(m_state);

    m_labelCamera->setText(tr("Camera"));
    m_labelBackend->setText(tr("Service"));
    m_labelItemType->setText(tr("Item type"));

    m_pinWindow->setToolTip(tr("Keep this window open"));
}

void ItemScannerDialog::setState(State newState)
{
    switch (newState) {
    case State::Idle:
        if (m_selectCamera->count())
            m_status->setText(tr("Click into the camera preview or press Space to capture an image."));
        else
            m_status->setText(u"<b>" + tr("There is no camera connected to this computer.") + u"</b>");
        if (m_camera && !m_camera->isActive())
            m_camera->start();
        m_cameraStopTimer.stop();
        break;
    case State::SoonInactive:
        m_cameraStopTimer.start();
        break;
    case State::Inactive:
        if (m_camera && m_camera->isActive())
            m_camera->stop();
        break;
    case State::Scaning:
        break;
    case State::NoMatch:
        m_status->setText(u"<b>" + tr("No matching item found - try again.") + u"</b>");
        m_noMatchMessageTimeout.start();
        break;
    case State::Error:
        m_status->setText(u"<b>"_qs + tr("An error occurred:") + u"</b><br>" + m_lastError);
        m_errorMessageTimeout.start();
        break;
    }
    if (newState == m_state)
        return;

    if (newState == State::Scaning) {
        m_progressTimer->start();
        m_bottomStack->setCurrentWidget(m_progress);
    } else {
        m_progressTimer->stop();
        m_bottomStack->setCurrentWidget(m_status);
    }
    m_state = newState;
}

#include "moc_itemscannerdialog.cpp"
