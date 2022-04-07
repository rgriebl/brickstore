/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include <QObject>
#include <QHash>
#include <QDate>
#include <QString>
#include <QByteArray>
#include <QLoggingCategory>
#include <QVector>
#include <QColor>
#include <QQmlEngine>

#include "qcoro/task.h"
#include "utility/q3cache.h"

Q_DECLARE_LOGGING_CATEGORY(LogLDraw)

class Transfer;
class TransferJob;
class MiniZip;


namespace LDraw {

class Part;
class PartElement;

void registerQmlTypes();

enum class UpdateStatus  { Ok, Loading, Updating, UpdateFailed };

class Color
{
    Q_GADGET
    QML_NAMED_ELEMENT(LDrawColor)
    QML_UNCREATABLE("")
    Q_PROPERTY(QString name MEMBER m_name)
    Q_PROPERTY(QColor color MEMBER m_color)
    Q_PROPERTY(QColor edgeColor MEMBER m_edgeColor)
    Q_PROPERTY(int id MEMBER m_id)
    Q_PROPERTY(float luminance MEMBER m_luminance)
    Q_PROPERTY(bool transparent READ isTransparent)
    Q_PROPERTY(bool chrome MEMBER m_chrome)
    Q_PROPERTY(bool metal MEMBER m_metal)
    Q_PROPERTY(bool matteMetallic MEMBER m_matteMetallic)
    Q_PROPERTY(bool pearlescent MEMBER m_pearlescent)
    Q_PROPERTY(bool rubber MEMBER m_rubber)

public:
    Color() = default;
    Color(const Color &copy) = default;

private:
    bool isTransparent() { return m_color.alpha() < 255; }

    QString m_name;
    QColor m_color;
    QColor m_edgeColor;
    int m_id = -1;
    float m_luminance = 0;
    bool m_chrome        : 1 = false;
    bool m_metal         : 1 = false;
    bool m_matteMetallic : 1 = false;
    bool m_rubber        : 1 = false;
    bool m_pearlescent   : 1 = false;

    friend class Library;
};

class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(LDraw::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged)
    Q_PROPERTY(QDate lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

public:
    ~Library() override;

    QString path() const;
    QCoro::Task<bool> setPath(const QString &path, bool forceReload = false);

    bool isValid() const               { return m_valid; }
    QDate lastUpdated() const          { return m_lastUpdated; }
    UpdateStatus updateStatus() const  { return m_updateStatus; }

    bool startUpdate();
    bool startUpdate(bool force);
    void cancelUpdate();

    Color color(int id, int baseId = -1) const;

    Part *partFromId(const QByteArray &id);
    Part *partFromFile(const QString &filename);

    static QVector<std::tuple<QString, QDate> > potentialLDrawDirs();
    static std::tuple<bool, QDate> checkLDrawDir(const QString &dir);

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void updateStatusChanged(LDraw::UpdateStatus updateStatus);
    void lastUpdatedChanged(const QDate &lastUpdated);
    void validChanged(bool valid);
    void libraryAboutToBeReset();
    void libraryReset();

private:
    Library(QObject *parent = nullptr);

    static Library *inst();
    static Library *s_inst;
    friend Library *library();

    Part *findPart(const QString &filename, const QString &parentdir = { });
    static std::tuple<QHash<int, Color>, QDate> parseLDconfig(const QByteArray &contents);
    QByteArray readLDrawFile(const QString &filename);
    void setUpdateStatus(UpdateStatus updateStatus);

    void clear();

    bool m_valid = false;
    UpdateStatus m_updateStatus = UpdateStatus::UpdateFailed;
    int m_updateInterval = 0;
    QDate m_lastUpdated;
    Transfer *m_transfer;
    TransferJob *m_job = nullptr;

    QString m_path;
    bool m_isZip = false;
    bool m_locked = false; // during updates/loading
    std::unique_ptr<MiniZip> m_zip;
    QStringList m_searchpath;
    QHash<int, Color> m_colors;  // id -> color struct
    Q3Cache<QString, Part> m_cache;  // path -> part

    friend class PartElement;
};

inline Library *library() { return Library::inst(); }

} // namespace LDraw

Q_DECLARE_METATYPE(LDraw::Color)
