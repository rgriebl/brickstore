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

#include <QtCore/QDateTime>
#include <QtGui/QImage>

#include "global.h"
#include "utility/ref.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QSaveFile)

class TransferJob;


namespace BrickLink {

class Picture : public QObject, protected Ref
{
    Q_OBJECT
    Q_PROPERTY(const BrickLink::Item *item READ item CONSTANT)
    Q_PROPERTY(const BrickLink::Color *color READ color CONSTANT)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged)

public:
    static quint64 key(const Item *item, const Color *color);

    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    Q_INVOKABLE void update(bool highPriority = false);
    QDateTime lastUpdated() const      { return m_fetched; }
    Q_INVOKABLE void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    Q_INVOKABLE const QImage image() const;

    qsizetype cost() const;

    Picture(std::nullptr_t) : Picture(nullptr, nullptr) { } // for scripting only!
    ~Picture() override;

    Q_INVOKABLE void addRef() { Ref::addRef(); }
    Q_INVOKABLE void release() { Ref::release(); }
    Q_INVOKABLE int refCount() const { return Ref::refCount(); }

signals:
    void isValidChanged(bool newIsValid);
    void lastUpdatedChanged(const QDateTime &newLastUpdated);
    void updateStatusChanged(BrickLink::UpdateStatus newUpdateStatus);

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid;
    bool          m_updateAfterLoad;
    // 2 bytes padding here
    UpdateStatus  m_update_status;

    TransferJob * m_transferJob = nullptr;

    QImage        m_image;

private:
    Picture(const Item *item, const Color *color);

    QFile *readFile() const;
    QSaveFile *saveFile() const;
    bool loadFromDisk(QDateTime &fetched, QImage &image) const;
    void setIsValid(bool valid);
    void setUpdateStatus(UpdateStatus status);
    void setLastUpdated(const QDateTime &dt);

    friend class Core;
    friend class PictureLoaderJob;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Picture *)


// tell Qt that Pictures are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::Picture>(BrickLink::Picture &c) { return c.refCount() == 0; }
