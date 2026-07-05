// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace CsvTokenizer {

// Rows keep their original field count (no padding). A quoted field is returned with its outer
// quotes stripped, "" collapsed to " and embedded CR/CRLF normalized to '\n'.
struct ParseResult
{
    QList<QStringList> rows;
    bool truncated = false;          // stopped early because maxRecords was reached
    bool unterminatedQuote = false;  // reached the end while still inside a quoted field
};

// Tokenizes already-decoded text into records.
// A record separator is a CR, LF or CRLF that occurs outside a quoted field; inside quotes
// those are literal content. Pass a 0 QChar() as quote to disable quote processin entirely.
// maxRecords < 0 means "all".
ParseResult tokenize(QStringView text, QChar delimiter = u',', QChar quote = u'"',
                     int maxRecords = -1);

} // namespace CsvTokenizer
