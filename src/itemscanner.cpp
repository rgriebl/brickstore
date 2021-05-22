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
#include "itemscanner.h"

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
    ItemScannerPrivate *d;
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
        //d->detector = cv::ORB::create();
        //d->detector = cv::ORB::create(250, 1.2f, 8, 15, 0, 2, cv::ORB::HARRIS_SCORE, 15, 20);
        d->detector = cv::AKAZE::create();

        //d->matcher = cv::makePtr<cv::FlannBasedMatcher>(cv::makePtr<cv::flann::LshIndexParams>(12, 20, 2));
        d->matcher = cv::makePtr<cv::FlannBasedMatcher>(cv::makePtr<cv::flann::LshIndexParams>(6, 12, 1));
    } else {
        d->detector = cv::AKAZE::create();
        d->matcher = cv::makePtr<cv::FlannBasedMatcher>(cv::makePtr<cv::flann::LshIndexParams>(6, 12, 1));
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

        QImage cvimg = img;
        cvimg.convertTo(QImage::Format_RGB888);
        cv::Mat input(cvimg.height(), cvimg.width(), CV_8UC3, cvimg.bits());

        std::vector<cv::KeyPoint> keypoints;
        cv::Mat output;
        d->detector->detectAndCompute(input, cv::noArray(), keypoints, output);

        if (keypoints.size() < 10)
            throw Exception("Too few keypoints (%1)").arg(keypoints.size());

        QVector<QPointF> points;
        points.reserve(keypoints.size());
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

    auto sw = new stopwatch("load matcher db");

    QFile fdb(BrickLink::core()->dataPath() + "/opencv-orb_M"_l1);
    if (fdb.exists() && (fdb.size() > 0) && fdb.open(QIODevice::ReadOnly)) {
        QDataStream ds(&fdb);
        qint32 s;
        ds >> s;
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
    }

    delete sw;
    sw = new stopwatch("train matcher db");

    if (!d->items.isEmpty())
        d->matcher->train();

    //qWarning() << d->matcher.get();
    //qWarning() << static_cast<SaveableFlannMatcher *>(d->matcher.get())->getFlannIndex();
    //static_cast<SaveableFlannMatcher *>(d->matcher.get())->getFlannIndex()->save("c:\\users\\sandman\\xx.dat");


    delete sw;

    if (d->items.isEmpty())
        emit databaseLoaded(false, tr("Could not read the image database"));
    else
        emit databaseLoaded(true, { });
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

        QImage cvimg = img;
        bool backAndFront = (img.width() > (img.height() * 7 / 6)); // most likely front + back
        if (backAndFront)
            cvimg = cvimg.copy(0, 0, cvimg.width() / 2, cvimg.height());
        cvimg.convertTo(QImage::Format_RGB888);

        qWarning() << "Processing" << pic->item()->itemTypeId() << pic->item()->id()
                   << (backAndFront ? "( >8 )" : "");

        cv::Mat input(cvimg.height(), cvimg.width(), CV_8UC3, cvimg.bits());
        cv::Mat output;
        std::vector<cv::KeyPoint> cvKeypoints;

        s_inst->d->detector->detectAndCompute(input, cv::noArray(), cvKeypoints, output);

        itemDescriptors.append(qMakePair(pic->item(), output));
        return true;
    };

    stopwatch sw ("create orb db");

    QVector<BrickLink::Picture *> downloading;
    connect(BrickLink::core(), &BrickLink::Core::pictureUpdated,
            s_inst, [&](BrickLink::Picture *pic) {
        downloading.removeAll(pic);
        opencvDetect(pic);
    });

    QFile fdb(BrickLink::core()->dataPath() + "/opencv-orb_M"_l1);
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
