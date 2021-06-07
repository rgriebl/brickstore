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
#include <QSaveFile>
#include <QDir>
#include <QCoreApplication>

#include <opencv2/features2d.hpp>
#include <opencv2/opencv.hpp>

#include "stopwatch.h"
#include "bricklink.h"
#include "exception.h"
#include "utility.h"
#include "version.h"
#include "bs_lzma.h"
#include "itemscanner.h"


// Default parameters used for creating the database
static const char *dbFileName = "image-search-database-v%1";
static const char *dbType = "BrickStore Image Search DB";
static qint32 dbVersion = 1;

struct AkazeParams
{
    qint32 /*cv::AKAZE::DescriptorType*/ descriptor_type = cv::AKAZE::DESCRIPTOR_MLDB;
    qint32 descriptor_size = 0;
    qint32 descriptor_channels = 3;
    float threshold = 0.001f;
    qint32 nOctaves = 4;
    qint32 nOctaveLayers = 4;
    qint32 /*cv::KAZE::DiffusivityType*/ diffusivity = cv::KAZE::DIFF_PM_G2;
};

struct FlannLshParams
{
    qint32 table_number = 12;
    qint32 key_size = 20;
    qint32 multi_probe_level = 2;
};



class SaveableFlannMatcher : public cv::FlannBasedMatcher
{
public:
    cv::flann::Index *getFlannIndex()
    {
        return flannIndex.get();
    }
};


QDataStream &operator>>(QDataStream &ds, cv::Mat &mat)
{
    qint32 type, elemSize, cols, rows;

    ds >> type >> elemSize >> cols >> rows;
    mat = cv::Mat::ones(rows, cols, type);

    ds.readRawData(reinterpret_cast<char *>(mat.data), rows * cols * elemSize);
    return ds;
}

QDataStream &operator<<(QDataStream &ds, const cv::Mat &mat)
{
    ds << qint32(mat.type()) << qint32(mat.elemSize())
       << qint32(mat.cols) << qint32(mat.rows);
    ds.writeRawData(reinterpret_cast<char *>(mat.data), int(mat.cols * mat.rows * mat.elemSize()));
    return ds;
}



class OpencvWorker : public QObject
{
    Q_OBJECT

public:
    OpencvWorker(ItemScannerPrivate *isp)
        : d(isp)
    { }

    Q_INVOKABLE void scan(int id, QImage image);
    Q_INVOKABLE void loadDatabase();

signals:
    void scanStarted(int id, QVector<QPointF> points);
    void scanFinished(int id, bool successful, QString error, QVector<const BrickLink::Item *> items);
    void databaseLoaded(bool successful, QString error);

private:
    void downloadDatabase(const QString &filePath);

    ItemScannerPrivate *d;
    Transfer *m_transfer = nullptr;
};


class ItemScannerPrivate
{
public:
    QThread opencvThread;
    OpencvWorker *opencvWorker;

    //std::shared_ptr<cv::ORB> detector;
    std::shared_ptr<cv::Feature2D> detector;
    std::shared_ptr<cv::FlannBasedMatcher> matcher;
    QVector<const BrickLink::Item *> items;

    QHash<const BrickLink::Item *, QPair<std::vector<cv::KeyPoint>, cv::Mat>> detectionMap;

    bool dbLoading = true;
    bool dbLoaded = false;
    bool dbTriedToDownload = false;
    QString dbError;
    int nextId = 0;
};


ItemScanner *ItemScanner::s_inst = nullptr;

ItemScanner *ItemScanner::inst()
{
    if (!s_inst)
        s_inst = new ItemScanner();
    return s_inst;
}

ItemScanner::~ItemScanner()
{
    delete d;
    s_inst = nullptr;
}

bool ItemScanner::isDatabaseLoading() const
{
    return d->dbLoading;
}

bool ItemScanner::isDatabaseLoaded() const
{
    return d->dbLoaded;
}

QString ItemScanner::databaseLoadingError() const
{
    return d->dbError;
}

ItemScanner::ItemScanner(bool createMode)
    : QObject()
    , d(new ItemScannerPrivate)
{
    qRegisterMetaType<QVector<const BrickLink::Item *>>();
    qRegisterMetaType<QVector<QPointF>>();

    d->opencvWorker = new OpencvWorker(d);
    d->opencvWorker->moveToThread(&d->opencvThread);
    connect(&d->opencvThread, &QThread::finished,
            d->opencvWorker, &QObject::deleteLater);
    connect(d->opencvWorker, &OpencvWorker::scanStarted,
            this, &ItemScanner::onWorkerScanStarted);
    connect(d->opencvWorker, &OpencvWorker::scanFinished,
            this, &ItemScanner::onWorkerScanFinished);
    connect(d->opencvWorker, &OpencvWorker::databaseLoaded,
            this, &ItemScanner::onWorkerDatabaseLoaded);
    d->opencvThread.start(QThread::LowPriority);

    if (createMode) {
        AkazeParams ap;
        d->detector = cv::AKAZE::create(cv::AKAZE::DescriptorType(ap.descriptor_type),
                                        ap.descriptor_size, ap.descriptor_channels, ap.threshold,
                                        ap.nOctaves, ap.nOctaveLayers,
                                        cv::KAZE::DiffusivityType(ap.diffusivity));

        FlannLshParams fp;
        d->matcher = cv::makePtr<cv::FlannBasedMatcher>(cv::makePtr<cv::flann::LshIndexParams>(
                                                            fp.table_number,
                                                            fp.key_size, fp.multi_probe_level));
    } else {
        QMetaObject::invokeMethod(d->opencvWorker, "loadDatabase", Qt::QueuedConnection);
    }
}

void ItemScanner::onWorkerScanStarted(int id, QVector<QPointF> points)
{
    emit scanStarted(id, points);
}

void ItemScanner::onWorkerScanFinished(int id, bool successful, QString error,
                                       QVector<const BrickLink::Item *> items)
{
    if (successful)
        emit scanFinished(id, items);
    else
        emit scanFailed(id, error);
}

void ItemScanner::onWorkerDatabaseLoaded(bool successful, QString error)
{
    d->dbLoaded = successful;
    d->dbError = error;
    d->dbLoading = false;

    if (!successful)
        qWarning() << "Loading the image search database failed:" << error;

    emit databaseLoadingFinished();
}

int ItemScanner::scan(const QImage &image)
{
    if (!d->dbLoaded)
        return 0;

    int id = ++d->nextId;
    QMetaObject::invokeMethod(d->opencvWorker, "scan", Qt::QueuedConnection,
                              Q_ARG(int, id), Q_ARG(QImage, image));
    return id;
}

void OpencvWorker::scan(int id, QImage img)
{
    try {
        if (!d->dbLoaded)
            throw Exception("No OpenCV database available");

        QImage cvimg = img.convertToFormat(QImage::Format_RGB888);
        cv::Mat input(cvimg.height(), cvimg.width(), CV_8UC3, cvimg.bits());

        std::vector<cv::KeyPoint> keypoints;
        cv::Mat output;
        d->detector->detectAndCompute(input, cv::noArray(), keypoints, output);

        if (keypoints.size() < 10)
            throw Exception("Too few keypoints (%1)").arg(keypoints.size());

        QVector<QPointF> points;
        points.reserve(int(keypoints.size()));
        for (const auto &kp : qAsConst(keypoints))
            points.append({ kp.pt.x, kp.pt.y });
        emit scanStarted(id, points);

        auto *sw = new stopwatch("match");

        std::vector<std::vector<cv::DMatch>> matches;
        int k = 4;
        d->matcher->knnMatch(output, matches, k);

        delete sw;

        qWarning() << "MATCHES" << matches.size();
        std::vector<cv::DMatch> results;
        for (const auto &m : matches) {
            if (k == 2) { // only take the better, if it is different enough
                if ((m.size() == 2) && (m.at(0).distance < (m.at(1).distance * .95f)))
                    results.push_back(m.at(0));
            } else { // take all feature matches
                for (const auto &mm : m)
                    results.push_back(mm);
            }
        }
        qWarning() << "RESULTS" << results.size();

        QHash<int, QPair<int, double>> distances;

        for (const auto &r : results) {
            ++distances[r.imgIdx].first;
            distances[r.imgIdx].second += r.distance;
        }

        QVector<QPair<const BrickLink::Item *, double>> scores;

        for (auto it = distances.begin(); it != distances.end(); ++it) {
            if (it.value().first > 5) {
                double dist = it.value().second / (it.value().first * it.value().first);
                scores.append(qMakePair(d->items.at(it.key()), dist));
            }
        }

        std::sort(scores.begin(), scores.end(), [](const auto &s1, const auto &s2) {
            return s1.second < s2.second;
        });

        QVector<const BrickLink::Item *> items;
        for (const auto &score : scores) {
            items.append(score.first);
            qWarning() << score.first->id() << score.second;
        }

        emit scanFinished(id, true, { }, items);

    } catch (const Exception &e) {
        emit scanFinished(id, false, e.error(), { });
    }
}

void OpencvWorker::loadDatabase()
{
    d->items.clear();
    if (d->matcher)
        d->matcher->clear();

    QString fileName = QString::fromLatin1(dbFileName).arg(dbVersion);
    QFile fdb(BrickLink::core()->dataPath() % u'/' % fileName);

    try {
        stopwatch sw("load image search db");


        if (!fdb.open(QIODevice::ReadOnly))
            throw Exception(&fdb, "Could not read the image search database");

        QDataStream ds(&fdb);
        QByteArray type;
        qint32 version;
        AkazeParams ap;
        FlannLshParams fp;
        qint32 s;

        ds >> type >> version;
        if (type != dbType || version != dbVersion) {
            throw Exception("invalid type or version (%1 / %2").arg(QLatin1String(dbType))
                    .arg(dbVersion);
        }
        ds >> ap.descriptor_type >> ap.descriptor_size >> ap.descriptor_channels >> ap.threshold
                >> ap.nOctaves >> ap.nOctaveLayers >> ap.diffusivity;
        ds >> fp.table_number >> fp.key_size >> fp.multi_probe_level;

        d->detector = cv::AKAZE::create(cv::AKAZE::DescriptorType(ap.descriptor_type),
                                        ap.descriptor_size, ap.descriptor_channels, ap.threshold,
                                        ap.nOctaves, ap.nOctaveLayers,
                                        cv::KAZE::DiffusivityType(ap.diffusivity));

        d->matcher = cv::makePtr<cv::FlannBasedMatcher>(cv::makePtr<cv::flann::LshIndexParams>(
                                                            fp.table_number,
                                                            fp.key_size, fp.multi_probe_level));

        if (!d->detector || !d->matcher)
            throw Exception("invalid OpenCV parameters");
        ds >> s;

        if (s <= 0 || s > 1'000'000)
            throw Exception("invalid item count (%1)").arg(s);

        d->items.reserve(s);
        while (s--) {
            qint8 itemType;
            QByteArray id;
            cv::Mat mat;

            ds >> itemType >> id >> mat;

            if (const auto *item = BrickLink::core()->item(itemType, id)) {
                d->items.append(item);
                d->matcher->add({ mat });
            }
        }
        if (ds.status() != QDataStream::Ok)
            throw Exception("invalid stream state (%1)").arg(ds.status());

        sw.restart("train image search db");

        if (!d->items.isEmpty() && d->matcher)
            d->matcher->train();

        // This is how we could save the trained index
//        if (d->matcher) {
//            auto index = static_cast<SaveableFlannMatcher *>(d->matcher.get())->getFlannIndex();
//            if (index)
//                index->save("/tmp/flann.idx");
//        }
        emit databaseLoaded(true, { });

    } catch (const Exception &e) {
        d->items.clear();
        d->matcher.reset();
        d->detector.reset();

        if (d->dbTriedToDownload) { // only try downloading once
            emit databaseLoaded(false,
                                tr("Could not read the image database: %1").arg(e.error()));
        } else {
            d->dbTriedToDownload = true;
            downloadDatabase(fdb.fileName());
        }
    }
}

void OpencvWorker::downloadDatabase(const QString &filePath)
{
    qWarning() << "Downloading the image search database";

    TransferJob *transJob = nullptr;
    if (!m_transfer)
        m_transfer = new Transfer(this);

    connect(m_transfer, &Transfer::finished, this, [this](TransferJob *job) {
        auto *file = qobject_cast<QFile *>(job->file());

        if (job->wasNotModifiedSince()) {
            file->remove();
            loadDatabase();

        } else if (file && file->size()) {
            QString basepath = file->fileName();
            basepath.truncate(basepath.length() - 5);      // strip '.lzma'

            qWarning() << "Decompressing the image search database";
            int lastPct = -1;
            QString error = LZMA::decompress(file->fileName(), basepath, [&lastPct](int p, int t) {
                int pct = int(t ? (10 * qint64(p) / t) : 0);
                if (pct != lastPct) {
                    lastPct = pct;
                    qWarning() << "  " << (pct * 10) << "%";
                }
            });

            if (error.isNull())
                loadDatabase();
            else
                emit databaseLoaded(false, error);
        } else {
            emit databaseLoaded(false, tr("Downloaded file is empty."));
        }
        m_transfer->deleteLater();
        m_transfer = nullptr;
    });

    QFileInfo dbInfo(filePath);
    QString remotefile = QLatin1String(BRICKSTORE_DATABASE_URL) % dbInfo.fileName();
    QString localfile = filePath;

    QDateTime dt;
    if (dbInfo.exists() && dbInfo.isFile())
        dt = dbInfo.fileTime(QFileDevice::FileModificationTime);

    QFile *file = new QFile(localfile % u".lzma");

    if (!file->open(QIODevice::WriteOnly))
        throw Exception(tr("Could not write to file: %1").arg(file->fileName()));

    transJob = TransferJob::getIfNewer(QUrl(remotefile % u".lzma"), dt, file);
    m_transfer->retrieve(transJob);
}


bool ItemScanner::createDatabase()
{
    if (s_inst)
        return false;
    s_inst = new ItemScanner(true /* createMode */);

    QVector<QPair<const BrickLink::Item *, cv::Mat>> itemDescriptors;

    auto opencvDetect = [&](BrickLink::Picture *pic) -> bool {
        const QImage img = pic->image();
        if (img.isNull()) {
            qWarning() << "ERROR: loaded empty large picture for" << pic->item()->id();
            return false;
        }

        QImage cvimg = img.convertToFormat(QImage::Format_RGB888);
        bool backAndFront = (img.width() > (img.height() * 7 / 6)); // most likely front + back
        if (backAndFront)
            cvimg = cvimg.copy(0, 0, cvimg.width() / 2, cvimg.height());

        qWarning() << "Processing" << pic->item()->itemTypeId() << pic->item()->id()
                   << (backAndFront ? "( >8 )" : "");

        cv::Mat input(cvimg.height(), cvimg.width(), CV_8UC3, cvimg.bits());
        cv::Mat output;
        std::vector<cv::KeyPoint> cvKeypoints;

        s_inst->d->detector->detectAndCompute(input, cv::noArray(), cvKeypoints, output);

        itemDescriptors.append(qMakePair(pic->item(), output));
        return true;
    };

    stopwatch sw ("create image search db");

    QVector<BrickLink::Picture *> downloading;
    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            s_inst, [&](BrickLink::Picture *pic) {
        downloading.removeAll(pic);
        opencvDetect(pic);
    });

    QFile fdb(BrickLink::core()->dataPath() % u'/' %
              QString::fromLatin1(dbFileName).arg(dbVersion));
    if (fdb.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        const auto &items = BrickLink::core()->items();
        for (const BrickLink::Item &item : items) {
            if (item.itemTypeId() != 'M') // we only handle minifigs for now
                continue;

            auto pic = BrickLink::core()->largePicture(&item, true);
            if (!pic->isValid()) {
                if (pic->updateStatus() == BrickLink::UpdateStatus::Updating)
                    downloading << pic;
                else
                    qWarning() << "ERROR: Cannot load large picture for" << item.id();
            } else {
                opencvDetect(pic);
            }
            QCoreApplication::processEvents();
        }
        while (!downloading.isEmpty())
            QCoreApplication::processEvents();

        QDataStream ds(&fdb);
        // type and version
        ds << QByteArray(dbType) << qint32(dbVersion);
        // AKAZE parameters
        AkazeParams ap;
        ds << ap.descriptor_type << ap.descriptor_size << ap.descriptor_channels << ap.threshold
           << ap.nOctaves << ap.nOctaveLayers << ap.diffusivity;
        // FLANN/LSH parameters
        FlannLshParams fp;
        ds << fp.table_number << fp.key_size << fp.multi_probe_level;
        // the actual db entries
        ds << qint32(itemDescriptors.size());
        for (const auto &p : qAsConst(itemDescriptors))
            ds << qint8(p.first->itemTypeId()) << p.first->id() << p.second;

        return true;
    } else {
        qWarning() << "ERROR: cannot create image db" << fdb.fileName();
        return false;
    }
}

#include "itemscanner.moc"

#include "moc_itemscanner.cpp"
