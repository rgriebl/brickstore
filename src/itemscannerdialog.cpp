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
#include <QComboBox>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QCameraViewfinder>
#include <QCameraInfo>
#include <QCamera>
#include <QCameraImageCapture>
#include <QVideoSurfaceFormat>
#include <QAbstractVideoSurface>
#include <QLabel>
#include <QProgressBar>
#include <QPainter>
#include <QMouseEvent>
#include <QDir>

#include "stopwatch.h"
#include "bricklink.h"
#include "utility.h"
#include "itemscanner.h"
#include "itemscannerdialog.h"
#include "config.h"

Q_DECLARE_METATYPE(QCameraInfo)


class VideoOverlay : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    VideoOverlay(QVideoWidget *vw)
        : QAbstractVideoSurface(vw)
        , m_vw(vw)
    {
    }

    int delta(const QSize &frameSize)
    {
        double ratio = 60. / 80.;
        return int((frameSize.width() - frameSize.height() * ratio) / 2);
    }

    void pause(const QImage &img)
    {
        m_pauseImage = img.convertToFormat(QImage::Format_ARGB32);
        m_vw->videoSurface()->start(QVideoSurfaceFormat(img.size(), QVideoFrame::Format_ARGB32));
        present(m_pauseImage);
    }

    void pausePoints(const QVector<QPointF> &points)
    {
        if (!m_pauseImage.isNull()) {
            m_points = points;
            // no idea why we have to start again here, but without it sometimes doesn't update
            m_vw->videoSurface()->start(QVideoSurfaceFormat(m_pauseImage.size(), QVideoFrame::Format_ARGB32));
            present(m_pauseImage);
        }
    }

    void unpause()
    {
        m_vw->videoSurface()->stop();
        m_pauseImage = { };
        m_points.clear();
    }

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override
    {
        return m_vw->videoSurface()->supportedPixelFormats(type);
    }

    bool isFormatSupported(const QVideoSurfaceFormat &format) const override
    {
        return m_vw->videoSurface()->isFormatSupported(format);
    }
    QVideoSurfaceFormat nearestFormat(const QVideoSurfaceFormat &format) const override
    {
        return m_vw->videoSurface()->nearestFormat(format);
    }

    bool start(const QVideoSurfaceFormat &format) override
    {
        if (m_pauseImage.isNull())
            return m_vw->videoSurface()->start(format);
        return false;
    }
    void stop() override
    {
        if (m_pauseImage.isNull())
            m_vw->videoSurface()->stop();
    }

    bool present(const QVideoFrame &f) override
    {
        auto img = m_pauseImage.isNull() ? f.image() : m_pauseImage;
        int d = delta(img.size());

        QPainter p(&img);
        if (d > 0) {
            p.fillRect(QRect { 0, 0, d, img.height() }, QColor(0, 0, 0, 192));
            p.fillRect(QRect { img.width() - d, 0, d, img.height() }, QColor(0, 0, 0, 192));
        }
        if (!m_points.isEmpty()) {
            p.setPen(Qt::green);
            p.translate(d, 0);
            for (const auto &pt : qAsConst(m_points))
                p.drawEllipse(pt, 2.5, 2.5);
        }
        p.end();

        return m_vw->videoSurface()->present(img);
    }

private:
    QVideoWidget *m_vw;
    QImage m_pauseImage;
    QVector<QPointF> m_points;
};

int ItemScannerDialog::s_averageScanTime = 0;

ItemScannerDialog::ItemScannerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Minifig Scanner"));
    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

    connect(ItemScanner::inst(), &ItemScanner::scanStarted,
            this, &ItemScannerDialog::onScanStarted);
    connect(ItemScanner::inst(), &ItemScanner::scanFinished,
            this, &ItemScannerDialog::onScanFinished);
    connect(ItemScanner::inst(), &ItemScanner::scanFailed,
            this, &ItemScannerDialog::onScanFailed);

    m_selectCamera = new QComboBox();
    m_selectCamera->setFocusPolicy(Qt::NoFocus);
    for (const QCameraInfo &cameraInfo : cameras)
        m_selectCamera->addItem(cameraInfo.description(), QVariant::fromValue(cameraInfo));

    m_viewFinder = new QVideoWidget();
    m_viewFinder->installEventFilter(this);

    m_overlay = new VideoOverlay(m_viewFinder);

    m_progress = new QProgressBar();
    m_progress->setTextVisible(false);
    m_progressTimer = new QTimer(m_progress);
    m_progressTimer->setInterval(30);
    connect(m_progressTimer, &QTimer::timeout,
            this, [=]() {
        m_progress->setValue(qMin(int(m_lastScanTime.elapsed()), m_progress->maximum()));
    });

    m_status = new QLabel();
    QPalette pal = m_status->palette();
    pal.setColor(QPalette::Text, pal.color(QPalette::Active, QPalette::Text));
    m_status->setPalette(pal);
    m_status->setTextFormat(Qt::RichText);
    m_status->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_selectCamera);
    layout->addWidget(m_viewFinder, 100);
    m_bottomStack = new QStackedLayout();
    m_bottomStack->setContentsMargins(0, 0, 0, 0);
    m_bottomStack->addWidget(m_status);
    m_bottomStack->addWidget(m_progress);
    layout->addLayout(m_bottomStack, 0);

    connect(m_selectCamera, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index) {
        updateCamera(m_selectCamera->itemData(index).value<QCameraInfo>());
    });
    updateCamera(QCameraInfo::defaultCamera());
    int idx = Config::inst()->value("/MainWindow/ItemScannerDialog/CameraIndex"_l1).toInt();
    if (idx >= 0 && idx < m_selectCamera->count())
        m_selectCamera->setCurrentIndex(idx);

    m_okText = tr("Click into the camera preview or press Space to capture an image.");
    m_noDbText = tr("Cannot access the image database:") % "<br><b>"_l1 %
            ItemScanner::inst()->databaseLoadingError() % "</b>"_l1;
    m_loadingDbText = tr("The image database is still loading...");
    m_noCameraText = "<b>"_l1 % tr("There is no camera connected to this computer.") % "</b>"_l1;
    m_noMatchText = "<b>"_l1 % tr("No matching item found - try again.") % "</b><br>"_l1 % m_okText;

    if (cameras.isEmpty()) {
        setEnabled(false);
        m_status->setText(m_noCameraText);
    } else if (ItemScanner::inst()->isDatabaseLoading()) {
        connect(ItemScanner::inst(), &ItemScanner::databaseLoadingFinished,
                this, [=]() {
            if (ItemScanner::inst()->isDatabaseLoaded()) {
                m_status->setText(m_okText);
                setEnabled(true);
            } else {
                m_status->setText(m_noDbText);
            }
        });
        setEnabled(false);
        m_status->setText(m_loadingDbText);
    } else if (!ItemScanner::inst()->isDatabaseLoaded()) {
        setEnabled(false);
        m_status->setText(m_noDbText);
    } else {
        m_status->setText(m_okText);
    }

    restoreGeometry(Config::inst()->value("/MainWindow/ItemScannerDialog/Geometry"_l1).toByteArray());
}

ItemScannerDialog::~ItemScannerDialog()
{
    Config::inst()->setValue("/MainWindow/ItemScannerDialog/Geometry"_l1, saveGeometry());
    Config::inst()->setValue("/MainWindow/ItemScannerDialog/CameraIndex"_l1, m_selectCamera->currentIndex());
}

void ItemScannerDialog::updateCamera(const QCameraInfo &camInfo)
{
    m_camera.reset(new QCamera(camInfo));
    m_camera->setViewfinder(m_overlay);
    m_camera->setCaptureMode(QCamera::CaptureStillImage);
    m_camera->start();

    m_imageCapture.reset(new QCameraImageCapture(m_camera.data()));
    QList<QSize> allRes = m_imageCapture->supportedResolutions();

    if (!allRes.isEmpty()) {
        std::sort(allRes.begin(), allRes.end(), [](const auto &r1, const auto &r2) {
            return r1.width() < r2.width();
        });

        const int preferredWidth = 800;
        QSize usedRes = allRes.at(0);

        for (const QSize &s : allRes) {
            if (s.width() > usedRes.width())
                usedRes = s;
            if (s.width() >= preferredWidth) // try to find a better one
                break;
        }

        QImageEncoderSettings ies = m_imageCapture->encodingSettings();
        ies.setResolution(usedRes);
        m_imageCapture->setEncodingSettings(ies);
    }

    connect(m_imageCapture.data(), &QCameraImageCapture::imageSaved,
            this, [=](int id, const QString &filename) {
        Q_UNUSED(id)
        QFile::remove(filename);
    });

    connect(m_imageCapture.data(), &QCameraImageCapture::imageCaptured,
            this, [=](int id, const QImage &rawimg) {
        Q_UNUSED(id)

        int scaleh = rawimg.height();

        int scalew = scaleh * 6 / 8;
        QImage img = rawimg.copy((rawimg.width() - scalew) / 2, 0, scalew, scaleh);

        m_lastScanTime.restart();

        m_camera->setViewfinder(static_cast<QVideoWidget *>(nullptr));
        m_overlay->pause(rawimg);

        m_currentScan = ItemScanner::inst()->scan(img);
        if (!m_currentScan) {
            onScanFailed(0, tr("Scanning failed"));
            return;
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

        m_imageCapture->capture(QDir::temp().absoluteFilePath("brickstore-capture.png"_l1));
    }
}

void ItemScannerDialog::onScanStarted(int id, const QVector<QPointF> &points)
{
    if (id == m_currentScan) {
        m_overlay->pausePoints(points);
    }
}

void ItemScannerDialog::onScanFinished(int id, const QVector<const BrickLink::Item *> &items)
{
    if (id == m_currentScan) {
        m_currentScan = 0;
        m_items = items;

        m_bottomStack->setCurrentWidget(m_status);
        m_progressTimer->stop();

        int elapsed = m_lastScanTime.elapsed();
        s_averageScanTime = s_averageScanTime ? (s_averageScanTime + elapsed) / 2 : elapsed;

        if (items.isEmpty()) {
            m_status->setText(m_noMatchText);
            m_overlay->unpause();
            m_camera->setViewfinder(m_overlay);
        } else {
            accept();
        }
    }
}

void ItemScannerDialog::onScanFailed(int id, const QString &error)
{
    if (id == m_currentScan) {
        m_currentScan = 0;

        m_bottomStack->setCurrentWidget(m_status);
        m_progressTimer->stop();

        m_overlay->unpause();
        m_camera->setViewfinder(m_overlay);

        m_status->setText("<b>"_l1 % tr("An error occured:") % "</b><br>"_l1 % error);
    }
}


#include "moc_itemscannerdialog.cpp"
#include "itemscannerdialog.moc"
