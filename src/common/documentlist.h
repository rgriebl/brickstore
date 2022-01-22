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

#include <QAbstractListModel>

#include "common/document.h"


class DocumentList : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    static DocumentList *inst();

    int count() const;
    QStringList allFiles() const;
    const QVector<Document *> &documents() const;

    Document *documentForFile(const QString &fileName) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void lastDocumentClosed();
    void countChanged(int count);
    void documentAdded(Document *document);
    void documentRemoved(Document *document);
    void documentCreated(Document *document);

private:
    DocumentList() = default;
    void add(Document *document);
    void remove(Document *document);

    QVector<Document *> m_documents;
    static DocumentList *s_inst;

    friend Document::Document(DocumentModel *, const QByteArray &, QObject *);
    friend Document::~Document();
};
