// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <optional>

#include <QDialog>
#include <QAbstractTableModel>
#include <QHeaderView>
#include <QStringConverter>

#include "common/csvimport.h"

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)
QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)


// A read-only table of the tokenized preview rows. It also owns the per-column mapping and
// exposes each column's target as its horizontal header text.
class CsvPreviewModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    using QAbstractTableModel::QAbstractTableModel;

    void setRows(const QList<QStringList> &rows);   // recomputes columns; new columns -> Ignore
    void setMapping(const QList<CsvImport::Field> &mapping);
    void setFieldForColumn(int column, CsvImport::Field field);
    QList<CsvImport::Field> mapping() const { return m_mapping; }

    int rowCount(const QModelIndex &parent = { }) const override;
    int columnCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<QStringList> m_rows;
    int m_columns = 0;
    QList<CsvImport::Field> m_mapping;
};


// A horizontal header that draws a drop-down arrow per section, hinting that clicking it
// picks the column's target field. The click itself is handled by the dialog (which owns the
// field vocabulary) via the inherited sectionClicked() signal.
class CsvFieldHeader : public QHeaderView
{
    Q_OBJECT
public:
    explicit CsvFieldHeader(QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};


class CsvImportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CsvImportDialog(const QString &fileName, QWidget *parent = nullptr);

    QString fileName() const;
    QList<CsvImport::Field> mapping() const;
    QChar delimiter() const;
    QChar quote() const;
    std::optional<QStringConverter::Encoding> encoding() const;
    CsvImport::Options options() const;

private:
    void reparse(bool guessMapping);
    void pickField(int column);
    void updateOkState();
    QList<CsvImport::Field> guessMapping(const QStringList &headerRow) const;

    QString m_fileName;
    QByteArray m_bytes;

    QComboBox *m_encoding;
    QButtonGroup *m_separator;
    QComboBox *m_numberFormat;
    QCheckBox *m_firstRowHeader;

    QTableView *m_preview;
    CsvPreviewModel *m_model;
    QDialogButtonBox *m_buttons;
};
