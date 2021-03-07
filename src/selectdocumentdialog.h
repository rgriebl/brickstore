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
#include <QWizard>
#include <QHash>
#include "document.h"
#include "bricklinkfwd.h"

class Document;
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QRadioButton)
QT_FORWARD_DECLARE_CLASS(QAbstractButton)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)


class SelectDocument : public QWidget
{
    Q_OBJECT
public:
    SelectDocument(const Document *self, QWidget *parent = nullptr);
    ~SelectDocument() override;

    bool isDocumentSelected() const;
    LotList lots() const;

signals:
    void documentSelected(bool valid);

private:
    LotList m_lotsFromClipboard;

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

    LotList lots() const;
    
private:
    SelectDocument *m_sd;
    QAbstractButton *m_ok;
};

class SelectMergeMode : public QWidget
{
    Q_OBJECT
public:
    SelectMergeMode(Document::MergeMode defaultMode, QWidget *parent = nullptr);

    Document::MergeMode defaultMergeMode() const;
    QHash<Document::Field, Document::MergeMode> fieldMergeModes() const;

signals:
    void mergeModesChanged(bool valid);

private:
    void createFields(QWidget *parent);

    QButtonGroup *m_allGroup;
    QVector<QButtonGroup *> m_groups;
};

class SelectCopyMergeDialog : public QWizard
{
    Q_OBJECT
public:
    SelectCopyMergeDialog(const Document *self, const QString &chooseDocText,
                          const QString &chooseFieldsText, QWidget *parent = nullptr);

    LotList lots() const;
    Document::MergeMode defaultMergeMode() const;
    QHash<Document::Field, Document::MergeMode> fieldMergeModes() const;

protected:
    void showEvent(QShowEvent *e) override;

private:
    void createFields(QWidget *parent);

    SelectDocument *m_sd;
    SelectMergeMode *m_mm;
};
