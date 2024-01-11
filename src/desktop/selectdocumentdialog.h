// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include <QWizard>
#include <QHash>
#include "common/documentmodel.h"

class DocumentModel;
class SelectMergeMode;
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QRadioButton)
QT_FORWARD_DECLARE_CLASS(QAbstractButton)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)


class SelectDocument : public QWidget
{
    Q_OBJECT
public:
    SelectDocument(const DocumentModel *self, QWidget *parent = nullptr);
    ~SelectDocument() override;

    bool isDocumentSelected() const;
    BrickLink::LotList lots() const;
    QString currencyCode() const;

signals:
    void documentSelected(bool valid);

private:
    LotList m_lotsFromClipboard;
    QString m_currencyCodeFromClipboard;

    QRadioButton *m_clipboard;
    QRadioButton *m_document;
    QListWidget *m_documentList;
};


class SelectDocumentDialog : public QDialog
{
    Q_OBJECT
public:
    SelectDocumentDialog(const DocumentModel *self, const QString &headertext,
                         QWidget *parent = nullptr);
    ~SelectDocumentDialog() override;

    LotList lots() const;
    QString currencyCode() const;
    
private:
    SelectDocument *m_sd;
    QAbstractButton *m_ok;
};


class SelectCopyMergeDialog : public QWizard
{
    Q_OBJECT
public:
    SelectCopyMergeDialog(const DocumentModel *self, const QString &chooseDocText,
                          const QString &chooseFieldsText, QWidget *parent = nullptr);
    ~SelectCopyMergeDialog() override;

    LotList lots() const;
    QString currencyCode() const;
    QHash<DocumentModel::Field, DocumentModel::MergeMode> fieldMergeModes() const;

protected:
    void showEvent(QShowEvent *e) override;

    SelectDocument *m_sd;
    SelectMergeMode *m_mm;
};
