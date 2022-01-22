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

class Picture : public Ref
{
public:
    static quint64 key(const Item *item, const Color *color);

    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool highPriority = false);
    QDateTime lastUpdated() const      { return m_fetched; }
    void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    const QImage image() const;

    int cost() const;

    Picture(std::nullptr_t) : Picture(nullptr, nullptr) { } // for scripting only!
    ~Picture() override;

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
    bool loadFromDisk(QDateTime &fetched, QImage &image);

    friend class Core;
    friend class PictureLoaderJob;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::Picture *)


// tell Qt that Pictures are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::Picture>(BrickLink::Picture &c) { return c.refCount() == 0; }
