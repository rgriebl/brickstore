// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QUndoCommand>
#include <QPointer>

#include "documentmodel.h"


class AddRemoveCmd : public QUndoCommand
{
public:
    enum Type { Add, Remove };

    AddRemoveCmd(Type t, DocumentModel *model, const QVector<int> &positions,
                 const QVector<int> &sortedPositions, const QVector<int> &filteredPositions,
                 const LotList &lots);
    ~AddRemoveCmd() override;
    int id() const override;

    void redo() override;
    void undo() override;

    static QString genDesc(bool is_add, int count);

private:
    QPointer<DocumentModel> m_model;
    QVector<int>       m_positions;
    QVector<int>       m_sortedPositions;
    QVector<int>       m_filteredPositions;
    LotList            m_lots;
    Type               m_type;
};

class ChangeCmd : public QUndoCommand
{
public:
    ChangeCmd(DocumentModel *model, const std::vector<std::pair<Lot *, Lot>> &changes,
              DocumentModel::Field hint = DocumentModel::FieldCount);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    void updateText();

    DocumentModel *m_model;
    uint m_loopCount;
    DocumentModel::Field m_hint;
    std::vector<std::pair<Lot *, Lot>> m_changes;

    static QTimer *s_eventLoopCounter;
};

class CurrencyCmd : public QUndoCommand
{
public:
    CurrencyCmd(DocumentModel *model, const QString &ccode, double crate);
    ~CurrencyCmd() override;

    int id() const override;

    void redo() override;
    void undo() override;

private:
    DocumentModel * m_model;
    QString    m_ccode;
    double     m_crate;
    double *   m_prices; // m_items.count() * 5 (price, origPrice, tierPrice * 3)
};

class ResetDifferenceModeCmd : public QUndoCommand
{
public:
    ResetDifferenceModeCmd(DocumentModel *model, const LotList &lots);
    int id() const override;

    void redo() override;
    void undo() override;

private:
    DocumentModel *m_model;
    QHash<const Lot *, Lot> m_differenceBase;
};


class SortCmd : public QUndoCommand
{
public:
    SortCmd(DocumentModel *model, const QVector<QPair<int, Qt::SortOrder>> &columns);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    DocumentModel *m_model;
    QDateTime m_created;
    QVector<QPair<int, Qt::SortOrder>> m_columns;
    bool m_isSorted = false;

    QVector<Lot *> m_unsorted;
};

class FilterCmd : public QUndoCommand
{
public:
    FilterCmd(DocumentModel *model, const QVector<Filter> &filterList);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    DocumentModel *m_model;
    QDateTime m_created;
    QVector<Filter> m_filterList;
    bool m_isFiltered = false;

    QVector<Lot *> m_unfiltered;
};
