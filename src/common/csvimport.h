// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QLocale>
#include <QDateTime>
#include "bricklink/io.h"

// turn tokenized CSV rows (CSV::tokenize) + a per-column mapping into BrickLink lots.
// This is the same Incomplete -> resolveIncomplete -> addLot pipeline as IO::fromBrickLinkXML;
// the mapping just says which lot field each CSV column feeds. Unresolvable rows are flagged
// via ParseResult::invalidLotCount() (not silently dropped), exactly like the XML importer.
namespace CsvImport {

enum class Field {
    Ignore,
    ItemId,
    ItemType,
    ColorId,
    ColorName,
    Pcc,
    Condition,
    Quantity,
    Price,
    Cost,
    Bulk,
    Sale,
    Comments,
    Remarks,
};

struct Options
{
    QLocale locale = QLocale::c();                            // decimal/grouping for numeric cells
    bool firstRowIsHeader = false;                            // drop rows.first()
    QDateTime creationTime = QDateTime::currentDateTime();    // changelog cut-off
    char defaultItemType = 'P';                               // used when no ItemType column is mapped
};

// mapping[c] is the target for CSV column c; columns past mapping.size() are ignored.
BrickLink::IO::ParseResult toLots(const QList<QStringList> &rows,
                                  const QList<Field> &mapping,
                                  const Options &options = { });

// Shared, translatable UI vocabulary for the mapping (used by the import dialog).
QString displayName(Field field);
QList<Field> allFields();   // in a sensible order for a drop-down, Ignore first

} // namespace CsvImport
