// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractListModel>

#include "common/document.h"


class DocumentList : public QAbstractListModel
{
    Q_OBJECT

public:
    static DocumentList *inst();

    int count() const;
    QStringList allFiles() const;
    const QVector<Document *> &documents() const;

    Document *documentForFile(const QString &fileName) const;
    Document *documentForModel(DocumentModel *model) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

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

    friend Document *Document::create(DocumentModel *, const QByteArray &, bool, QObject *);
    friend Document::~Document();
};
