// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QVector>
#include <QLoggingCategory>

#include "bricklink/global.h"

Q_DECLARE_LOGGING_CATEGORY(LogScanner)

namespace Scanner {

class CorePrivate;

class Core : public QObject
{
    Q_OBJECT
public:
    static Core *inst();
    Core(const Core &) = delete;
    ~Core() override;

    struct Backend {
        QByteArray id;
        QString    name;
        QByteArray itemTypeFilter; // "BCGIMOPS"
        int        preferredImageSize;
        QString    icon() const;
    };

    struct Result {
        // shouldn't be needed, but clang still lacks support for aggregate initialization
        Result(const BrickLink::Item *_item, double _score) : item(_item), score(_score) { }

        const BrickLink::Item *item = nullptr;
        double score = 0;
    };

    QByteArrayList availableBackendIds() const;
    void setDefaultBackendId(const QByteArray &id);
    QByteArray defaultBackendId() const;
    const Backend *backendFromId(const QByteArray &id) const;

    uint scan(const QImage &image, const BrickLink::ItemType *filter = nullptr,
              const QByteArray &backendId = { });

signals:
    void scanFinished(uint id, const QVector<Core::Result> &itemsAndScores);
    void scanFailed(uint id, const QString &error);

private:
    Core();
    static Core *s_inst;

    std::unique_ptr<CorePrivate> d;
};

inline Core *core() { return Core::inst(); }

} // namespace Scanner

#if 0
class QmlItemScanner
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ItemScanner)
    Q_PROPERTY(QVariantList backends READ backends CONSTANT FINAL)
    Q_PROPERTY(QString defaultBackendId READ defaultBackendId CONSTANT FINAL)

public:
    Q_INVOKABLE int scan(const QImage &img, const QString &itemTypeId = u"*"_qs, const QString &backendId = { })
    {
        return (int) ItemScanner::inst()->scan(img, itemTypeId.toUtf8().at(0), backendId.toUtf8());
    }

    QVariantList backends() const
    {
        QVariantList result;
        const auto bs = ItemScanner::inst()->availableBackends();
        for (const auto &backend : bs) {
            QVariantMap map;
            map["id"] = QString::fromUtf8(backend.id);
            map["name"] = backend.name;
            map["itemTypeFilter"] = QString::fromUtf8(backend.itemTypeFilter);
            map["preferredImageSize"] = backend.preferredImageSize;
            result.append(map);
        }
        return result;
    }
    QString defaultBackendId() const
    {
        return QString::fromUtf8(ItemScanner::inst()->defaultBackendId());
    }
};
#endif
