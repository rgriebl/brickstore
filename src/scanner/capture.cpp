// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QCamera>
#include <QMediaDevices>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QDeadlineTimer>
#include <QTimer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(permissions)
#  include <QCoreApplication>
#  include <QPermissions>
#  include <QPointer>
#  if defined(BS_DESKTOP)
#    include <QMessageBox>
#  endif
#endif

#include "bricklink/core.h"
#include "bricklink/itemtype.h"
#include "core.h"
#include "capture.h"


static struct SetQtMMBackend  // clazy:exclude=non-pod-global-static
{
    // QTBUG-120026: The default FFmpeg backend completely freezes the main thread for 1-3 sec
    // when enumerating camera devices on Windows and macOS.
    SetQtMMBackend()
    {
#if defined(Q_OS_WINDOWS)
        qputenv("QT_MEDIA_BACKEND", "windows");
#elif defined(Q_OS_MACOS)
        qputenv("QT_MEDIA_BACKEND", "darwin");
#endif
    }
} setQtMMBackend;


using namespace std::chrono_literals;

namespace Scanner {

class CapturePrivate
{
public:
    QMediaCaptureSession *captureSession;
    QImageCapture *imageCapture;
    QByteArray currentCameraId;
    std::unique_ptr<QCamera> camera;
    std::optional<int> currentCaptureId;
    QDeadlineTimer tryCaptureBefore;
    uint currentScanId = 0;
    QElapsedTimer currentScanTime;
    int averageScanTime = 1500;
    QByteArray currentBackendId;
    int progress = 0;
    Capture::State state = Capture::State::Idle;

    QString lastError;

    QTimer cameraStopTimer;
    QTimer noMatchMessageTimeout;
    QTimer errorMessageTimeout;
    QTimer progressTimer;

    QList<const BrickLink::ItemType *> supportedFilters;
    const BrickLink::ItemType *currentFilter = nullptr;

    static bool s_hasCameraPermission;
};

bool CapturePrivate::s_hasCameraPermission = false;


Capture::Capture(QObject *parent)
    : QObject(parent)
    , d(new CapturePrivate)
{
    d->captureSession = new QMediaCaptureSession(this);
    d->imageCapture = new QImageCapture(d->captureSession);
    d->captureSession->setImageCapture(d->imageCapture);

    connect(d->captureSession, &QMediaCaptureSession::videoOutputChanged,
            this, &Capture::videoOutputChanged);

    d->errorMessageTimeout.setInterval(10s);
    d->errorMessageTimeout.setSingleShot(true);
    connect(&d->errorMessageTimeout, &QTimer::timeout, this, [this]() {
        if (state() == State::Error)
            setState(State::Idle);
    });
    d->noMatchMessageTimeout.setInterval(10s);
    d->noMatchMessageTimeout.setSingleShot(true);
    connect(&d->noMatchMessageTimeout, &QTimer::timeout, this, [this]() {
        if (state() == State::NoMatch)
            setState(State::Idle);
    });

    d->cameraStopTimer.setInterval(10s);
    d->cameraStopTimer.setSingleShot(true);
    connect(&d->cameraStopTimer, &QTimer::timeout, this, [this]() {
        if (state() == State::SoonInactive)
            setState(State::Inactive);
    });

    d->progressTimer.setInterval(30ms);
    connect(&d->progressTimer, &QTimer::timeout, this, [this]() {
        if ((state() == State::Scanning) && d->averageScanTime) {
            d->progress = std::clamp(int(100 * d->currentScanTime.elapsed() / d->averageScanTime), 0, 100);
            emit progressChanged(d->progress);
        }
    });

    connect(core(), &Core::scanFinished,
            this, [this](uint scanId, const QVector<Core::Result> &itemsAndScores) {
        if (scanId == d->currentScanId) {
            d->currentScanId = 0;
            d->lastError.clear();

            auto elapsed = int(d->currentScanTime.elapsed());
            if (!d->averageScanTime)
                d->averageScanTime = 1;
            d->averageScanTime = std::max(1, (d->averageScanTime + elapsed) / 2);

            QVector<const BrickLink::Item *> items;
            items.reserve(itemsAndScores.size());
            for (const auto &is : itemsAndScores)
                items << is.item;

            if (items.isEmpty()) {
                setState(State::NoMatch);
            } else {
                setState(State::Idle);
                emit captureAndScanFinished(items);
            }
        }
    });

    connect(core(), &Core::scanFailed,
            this, [this](uint scanId, const QString &error) {
        if (scanId == d->currentScanId) {
            d->currentScanId = 0;
            d->lastError = error;
            setState(State::Error);
        }
    });

    connect(d->imageCapture, &QImageCapture::errorOccurred,
            this, [this](int id, QImageCapture::Error error, const QString &errorString) {
        Q_UNUSED(error)

        if (!d->currentCaptureId.has_value() || d->currentCaptureId.value() != id) {
            qCCritical(LogScanner) << "Ignoring errorOccurred(id:" << id << "), current:"
                                   << d->currentCaptureId.value_or(-1);
        }
        d->currentCaptureId.reset();
        d->lastError = errorString;
        setState(State::Error);
    });

    connect(d->imageCapture, &QImageCapture::readyForCaptureChanged,
            this, [this](bool ready) {
        if (ready && !d->tryCaptureBefore.hasExpired())
            captureAndScan();
    });

    connect(d->imageCapture, &QImageCapture::imageCaptured,
            this, [this](int id, const QImage &img) {
        if (!d->currentCaptureId.has_value() || d->currentCaptureId.value() != id) {
            qCCritical(LogScanner) << "Ignoring imageCaptured(id:" << id << "), current:"
                                   << d->currentCaptureId.value_or(-1);
        }
        d->currentCaptureId.reset();
        d->currentScanId = core()->scan(img, d->currentFilter, d->currentBackendId);

        if (!d->currentScanId) {
            d->lastError = tr("Scanning failed");
            setState(State::Error);
        } else {
            setState(State::Scanning);
        }
    });

    setCurrentBackendId(core()->defaultBackendId());
    setCurrentCameraId(QMediaDevices::defaultVideoInput().id());
}

Capture::~Capture()
{ }

QObject *Capture::videoOutput() const
{
    return d->captureSession->videoOutput();
}

void Capture::setVideoOutput(QObject *videoOutput)
{
    d->captureSession->setVideoOutput(videoOutput);
}

void Capture::setVideoOutputActiveState(ActiveState activeState)
{
    switch (activeState) {
    case ActiveState::Active:       setState(State::Idle); break;
    case ActiveState::Inactive:     setState(State::Inactive); break;
    case ActiveState::SoonInactive: setState(State::SoonInactive); break;
    }
}

void Capture::captureAndScan()
{
    if ((state() != State::Scanning) && (state() != State::Capturing)) {
        if (d->imageCapture->isReadyForCapture()) {
            setState(State::Capturing);
            d->currentCaptureId = d->imageCapture->capture();
        } else {
            // The cam might not be ready yet (e.g. window activation). Wait for it to become ready
            // and try again.
            d->tryCaptureBefore = QDeadlineTimer(1000);
        }
    }
}

void Capture::checkSystemPermissions(QObject *context, const std::function<void(bool)> &callback)
{
    if (CapturePrivate::s_hasCameraPermission) {
        if (callback)
            callback(true);
        return;
    }

    const QString requestDenied = tr("BrickStore's request for camera access was denied. You will not be able to use your webcam to identify parts until you grant the required permissions via your system's Settings application.");

#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
    QCameraPermission cameraPermission;
    switch (qApp->checkPermission(cameraPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(cameraPermission, context, [callback](const QPermission &p) {
            CapturePrivate::s_hasCameraPermission = (p.status() == Qt::PermissionStatus::Granted);
            if (callback)
                callback(CapturePrivate::s_hasCameraPermission);
        });
        return;
    case Qt::PermissionStatus::Denied:
#  if defined(BS_DESKTOP)
        QMessageBox::warning(nullptr, QCoreApplication::applicationName(), requestDenied);
#   endif
        if (callback)
            callback(false);
        return;
    case Qt::PermissionStatus::Granted:
        break; // Proceed
    }
#else
    Q_UNUSED(context)
    Q_UNUSED(requestDenied)
#endif
    CapturePrivate::s_hasCameraPermission = true;
    if (callback)
        callback(true);
    return;
}

Capture::State Capture::state() const
{
    return d->state;
}

void Capture::setState(State newState)
{
    switch (newState) {
    case State::Idle:
        if (d->camera && !d->camera->isActive())
            d->camera->start();
        d->cameraStopTimer.stop();
        break;
    case State::SoonInactive:
        d->cameraStopTimer.start();
        break;
    case State::Inactive:
        if (d->camera && d->camera->isActive())
            d->camera->stop();
        break;
    case State::Capturing:
        d->currentScanTime.start();
        break;
    case State::Scanning:
        break;
    case State::NoMatch:
        d->noMatchMessageTimeout.start();
        break;
    case State::Error:
        d->errorMessageTimeout.start();
        break;
    }
    if (newState == d->state)
        return;

    if (newState == State::Scanning) {
        d->progressTimer.start();
    } else {
        d->progressTimer.stop();
        d->progress = 0;
        emit progressChanged(d->progress);
    }

    d->state = newState;
    emit stateChanged(newState);
}

QString Capture::lastError() const
{
    return d->lastError;
}

int Capture::progress() const
{
    return d->progress;
}

bool Capture::isCameraActive() const
{
    return d->camera ? d->camera->isActive() : false;
}

QByteArray Capture::currentCameraId() const
{
    return d->currentCameraId;
}

void Capture::setCurrentCameraId(const QByteArray &newCameraId)
{
    if (d->currentCameraId == newCameraId)
        return;

    QCameraDevice newCameraDevice;
    const auto allCameraDevices = QMediaDevices::videoInputs();
    for (const auto &cameraDevice : allCameraDevices) {
        if (cameraDevice.id() == newCameraId) {
            newCameraDevice = cameraDevice;
            break;
        }
    }
    if (newCameraDevice.isNull())
        return;

    d->currentCameraId = newCameraId;
    emit currentCameraIdChanged(newCameraId);

    d->camera.reset(new QCamera(newCameraDevice));
    connect(d->camera.get(), &QCamera::activeChanged,
            this, &Capture::cameraActiveChanged);

    d->captureSession->setCamera(d->camera.get());
    d->camera->start();
}

QByteArray Capture::currentBackendId() const
{
    return d->currentBackendId;
}

void Capture::setCurrentBackendId(const QByteArray &backendId)
{
    if (d->currentBackendId == backendId)
        return;

    if (const auto *backend = core()->backendFromId(backendId)) {
        d->currentBackendId = backendId;

        auto oldFilters = d->supportedFilters;
        d->supportedFilters.clear();
        for (const char c : backend->itemTypeFilter) {
            if (const auto *itemType = BrickLink::core()->itemType(c))
                d->supportedFilters << itemType;
        }
        if (d->supportedFilters != oldFilters) {
            if (d->currentFilter && !d->supportedFilters.contains(d->currentFilter))
                setCurrentItemTypeFilter(nullptr);

            emit supportedItemTypeFiltersChanged(d->supportedFilters);
        }
        emit currentBackendIdChanged(backendId);
    }
}

const BrickLink::ItemType *Capture::currentItemTypeFilter() const
{
    return d->currentFilter;
}

void Capture::setCurrentItemTypeFilter(const BrickLink::ItemType *filter)
{
    if ((filter != d->currentFilter) && (!filter || d->supportedFilters.contains(filter))) {
        d->currentFilter = filter;
        emit currentItemTypeFilterChanged(filter);
    }
}

QList<const BrickLink::ItemType *> Capture::supportedItemTypeFilters() const
{
    return d->supportedFilters;
}

} // namespace Scanner

#include "moc_capture.cpp"
