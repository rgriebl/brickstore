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
#include <QStringBuilder>
#include <QFileInfo>
#include <QIcon>
#include <QPainter>

#include "documentlist.h"


DocumentList *DocumentList::s_inst = nullptr;

DocumentList *DocumentList::inst()
{
    if (!s_inst)
        s_inst = new DocumentList();
    return s_inst;
}

int DocumentList::count() const
{
    return int(m_documents.count());
}

QStringList DocumentList::allFiles() const
{
    QStringList files;
    for (const auto *doc : m_documents) {
        QString fileName = doc->filePath();
        if (!fileName.isEmpty())
            files << fileName;
    }
    return files;
}

const QVector<Document *> &DocumentList::documents() const
{
    return m_documents;
}

Document *DocumentList::documentForFile(const QString &fileName) const
{
    QString afp = QFileInfo(fileName).absoluteFilePath();

    for (auto *document : m_documents) {
        if (QFileInfo(document->filePath()).absoluteFilePath() == afp)
            return document;
    }
    return nullptr;
}

int DocumentList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_documents.count());
}

QVariant DocumentList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return { };

    static QIcon docIcon(u":/assets/generated-app-icons/brickstore_doc"_qs);

    Document *document = m_documents.at(index.row());
    switch (role) {
    case Qt::ToolTipRole: {
        QString s = document->filePathOrTitle();
        if (document->model()->isModified())
            s.append(u"*"_qs);
        return s;
    }
    case Qt::DisplayRole: {
        QString s = document->title();
        if (s.isEmpty()) {
            QFileInfo fi(document->filePath());
            s = fi.fileName();

            QStringList clashes;
            std::for_each(m_documents.cbegin(), m_documents.cend(),
                          [s, document, &clashes](const Document *otherDoc) {
                if ((otherDoc != document) && otherDoc->title().isEmpty()) {
                    QFileInfo otherFi(otherDoc->filePath());
                    if (otherFi.fileName() == s)
                        clashes << otherFi.absolutePath();
                }
            });
            if (!clashes.isEmpty()) {
                QString base = fi.absoluteFilePath();
                for (int i = 0; i < 10; ++i) {
                    QString minBase = base.section(u'/', -1 - i, -1);
                    bool noClash = true;

                    for (const auto &clash : qAsConst(clashes)) {
                        if (clash.section(u'/', -1 - i, -1) == minBase) {
                            noClash = false;
                            break;
                        }
                    }
                    if (noClash) {
                        s = minBase % u'/' % s;
                        break;
                    }
                }
                s = fi.absoluteFilePath();
            }
        }
        if (document->model()->isModified())
            s.append(u"*"_qs);
        return s;
    }
    case Qt::DecorationRole:
        if (!document->thumbnail().isNull()) {
            QImage img = document->thumbnail();
            if (img.height() != img.width()) {
                int d = qMax(img.height(), img.width());

                QImage canvas({ d, d }, QImage::Format_ARGB32);
                canvas.fill(Qt::transparent);
                QPainter p(&canvas);
                p.drawImage({ (d - img.width()) / 2, (d - img.height()) / 2 }, img, img.rect());
                p.end();
                img = canvas;
            }
            return QIcon(QPixmap::fromImage(img));
        } else {
            return docIcon;
        }

    case Qt::UserRole:
        return QVariant::fromValue(document);
    }
    return { };
}


void DocumentList::add(Document *document)
{
    // Please note: Document might not be 100% initialized at this point, because we got called
    // from the base constructor!

    QMetaObject::invokeMethod(this, [this, document]() {
        emit documentCreated(document);

        beginInsertRows({ }, rowCount(), rowCount());
        m_documents.append(document);
        endInsertRows();

        auto updateDisplay = [this, document]() {
            int row = int(m_documents.indexOf(document));
            emit dataChanged(index(row), index(row), { Qt::DisplayRole, Qt::ToolTipRole });
        };
        connect(document, &Document::filePathChanged,
                this, updateDisplay);
        connect(document, &Document::titleChanged,
                this, updateDisplay);
        connect(document->model(), &DocumentModel::modificationChanged,
                this, updateDisplay);

        emit documentAdded(document);
        emit countChanged(count());
    }, Qt::QueuedConnection);
}

void DocumentList::remove(Document *document)
{
    int row = int(m_documents.indexOf(document));
    if (row >= 0) {
        beginRemoveRows({ }, row, row);
        m_documents.removeAt(row);
        endRemoveRows();

        emit documentRemoved(document);
        emit countChanged(count());

        if (m_documents.isEmpty())
            emit lastDocumentClosed();
    }
}

#include "moc_documentlist.cpp"
