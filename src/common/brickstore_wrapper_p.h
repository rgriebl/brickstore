// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QItemSelectionModel>

class QmlDocument;
class Document;

class ProxySelectionModel : public QItemSelectionModel
{
    Q_OBJECT

public:
    explicit ProxySelectionModel(QmlDocument *qmlDoc, Document *doc);

private:
    void transfer(const QItemSelectionModel *from, QItemSelectionModel *to);

    QmlDocument *m_qmlDoc;
    Document *m_doc;
    bool m_interlock = false;
};
