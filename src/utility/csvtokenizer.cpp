// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "csvtokenizer.h"

namespace CsvTokenizer {

ParseResult tokenize(QStringView text, QChar delimiter, QChar quote, int maxRecords)
{
    enum class State { FieldStart, Unquoted, Quoted, AfterQuote };

    ParseResult result;
    QStringList record;
    QString field;
    bool recordHasContent = false;
    State state = State::FieldStart;
    const qsizetype n = text.size();

    auto flushField = [&] {
        record.append(field);
        field.clear();
    };

    auto flushRecord = [&] {   // returns true when the record cap is hit
        flushField();
        if (recordHasContent)
            result.rows.append(record);
        record.clear();
        recordHasContent = false;
        state = State::FieldStart;
        return (maxRecords >= 0) && (result.rows.size() >= maxRecords);
    };

    qsizetype i = 0;
    auto flushRecordAtEol = [&](bool crSeen) {   // consumes the LF of a CRLF pair, then flushes
        if (crSeen && (i + 1 < n) && (text[i + 1] == u'\n'))
            ++i;
        return flushRecord();
    };

    for (; i < n; ++i) {
        const QChar c = text[i];
        const bool isCR = (c == u'\r');
        const bool isEol = isCR || (c == u'\n');

        switch (state) {
        case State::FieldStart:
            if (c == quote) {
                state = State::Quoted;
                recordHasContent = true;
            } else if (c == delimiter) {
                recordHasContent = true;
                flushField();
            } else if (isEol) {
                if (flushRecordAtEol(isCR)) {
                    result.truncated = (i + 1 < n);
                    return result;
                }
            } else {
                field.append(c);
                recordHasContent = true;
                state = State::Unquoted;
            }
            break;

        case State::Unquoted:
            if (c == delimiter) {
                flushField();
                state = State::FieldStart;
            } else if (isEol) {
                if (flushRecordAtEol(isCR)) {
                    result.truncated = (i + 1 < n);
                    return result;
                }
            } else {
                field.append(c);   // a stray quote is kept literally
            }
            break;

        case State::Quoted:
            if (c == quote) {
                state = State::AfterQuote;
            } else if (isCR) {
                field.append(u'\n');   // normalize embedded CR / CRLF to '\n'
                if ((i + 1 < n) && (text[i + 1] == u'\n'))
                    ++i;
            } else {
                field.append(c);   // literal delimiter or '\n'
            }
            break;

        case State::AfterQuote:
            if (c == quote) {
                field.append(quote);   // "" -> escaped quote
                state = State::Quoted;
            } else if (c == delimiter) {
                flushField();
                state = State::FieldStart;
            } else if (isEol) {
                if (flushRecordAtEol(isCR)) {
                    result.truncated = (i + 1 < n);
                    return result;
                }
            } else {
                field.append(c);   // junk after the closing quote
                state = State::Unquoted;
            }
            break;
        }
    }

    if (state == State::Quoted)
        result.unterminatedQuote = true;
    if (recordHasContent || !field.isEmpty() || !record.isEmpty())
        flushRecord();
    return result;
}

} // namespace CsvTokenizer
