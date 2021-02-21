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
#pragma once

#include <QDialog>
#include "document.h"
#include "bricklinkfwd.h"

class Document;
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QRadioButton)
QT_FORWARD_DECLARE_CLASS(QAbstractButton)


class SelectDocument : public QWidget
{
    Q_OBJECT
public:
    SelectDocument(const Document *self, QWidget *parent = nullptr);
    ~SelectDocument() override;

    bool isDocumentSelected() const;
    BrickLink::InvItemList items() const;

signals:
    void documentSelected(bool valid);

private:
    BrickLink::InvItemList m_itemsFromClipboard;

    QRadioButton *m_clipboard;
    QRadioButton *m_document;
    QListWidget *m_documentList;
};

class SelectDocumentDialog : public QDialog
{
    Q_OBJECT
public:
    SelectDocumentDialog(const Document *self, const QString &headertext,
                         QWidget *parent = nullptr);

    BrickLink::InvItemList items() const;
    
private:
    SelectDocument *m_sd;
    QAbstractButton *m_ok;
};


class SelectCopyFieldsDialog : public QDialog
{
    Q_OBJECT
public:
    SelectCopyFieldsDialog(const Document *self, const QString &headertext,
                           const QVector<QPair<QString, bool>> &fields, QWidget *parent = nullptr);

    BrickLink::InvItemList items() const;
    const QVector<int> &selectedFields() const;

private:
    SelectDocument *m_sd;
    QVector<int> m_selectedFields;
    QAbstractButton *m_ok;
};

