// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QStringList>
#include <QCoreApplication>
#include <QVariant>
#include <QRegularExpression>
#include <QDebug>
#include <QLocale>

#include "filter.h"


static QString quote(const QString &str)
{
    if (str.isEmpty() || str.contains(u' '))
        return u'"' + str + u'"';
    else
        return str;
}


void Filter::setField(int field)
{
    m_field = field;
}

void Filter::setExpression(const QString &expr)
{
    m_expression = expr;

    QLocale loc;
    bool isInt = false;
    m_asInt = loc.toInt(expr, &isInt);
    m_isInt = isInt;
    bool isDouble = false;
    m_asDouble = loc.toDouble(expr, &isDouble);
    m_isDouble = isDouble;

    if (expr.contains(u'?') || expr.contains(u'*') || expr.contains(u'[')) {
        m_isRegExp = true;
        m_asRegExp.setPattern(QRegularExpression::wildcardToRegularExpression(expr));
        m_asRegExp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }
    m_asDateTime = QDateTime { };

    int baseYear = std::min(2000, QDate::currentDate().year() - 90);
    for (auto fmt : { QLocale::LongFormat, QLocale::ShortFormat, QLocale::NarrowFormat }) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        Q_UNUSED(baseYear)
        m_asDateTime = loc.toDateTime(expr, fmt);
#else
        m_asDateTime = loc.toDateTime(expr, fmt, baseYear);
#endif

        if (m_asDateTime.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
            const QDate d = m_asDateTime.date();
            if (d.year() < 2000)
                m_asDateTime.setDate(d.addYears(100));
#endif
            break;
        }
    }
    if (!m_asDateTime.isValid()) {
        for (auto fmt : { QLocale::LongFormat, QLocale::ShortFormat, QLocale::NarrowFormat }) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
            QDate d = loc.toDate(expr, fmt);
#else
            QDate d = loc.toDate(expr, fmt, baseYear);
#endif
            if (d.isValid()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
                if (d.year() < 2000)
                    d = d.addYears(100);
#endif
                m_asDateTime.setDate(d);
                m_asDateTime.setTime(QTime(0, 0));
                m_asDateTime.setTimeSpec(Qt::UTC);
                m_isDate = true;
                break;
            }
        }
    }
}

void Filter::setComparison(Comparison cmp)
{
    m_comparison = cmp;
}

void Filter::setCombination(Combination cmb)
{
    m_combination = cmb;
}

QString Filter::Parser::toString(const QVector<Filter> &filter, bool preferSymbolic) const
{
    QString result;

    for (int i = 0; i < filter.size(); ++i) {
        const Filter &f = filter.at(i);

        if ((f.field() != -1) || (f.comparison() != Matches)) {
            for (const auto &ft : m_field_tokens) {
                if (ft.first == f.field()) {
                    result = result + quote(ft.second);
                    break;
                }
            }
            for (const auto &ct : m_comparison_tokens) {
                if (ct.first == f.comparison()) {
                    if (preferSymbolic && ct.second.at(0).isLetterOrNumber())
                        continue;
                    result = result + u' ' + quote(ct.second) + u' ';
                    break;
                }
            }
        }
        result = result + quote(f.expression());

        if (i < (filter.size() - 1)) {
            for (const auto &ct : m_combination_tokens) {
                if (ct.first == f.combination()) {
                    if (preferSymbolic && ct.second.at(0).isLetterOrNumber())
                        continue;
                    result = result + u' ' + quote(ct.second) + u' ';
                    break;
                }
            }
        }
    }
    if (result == uR"("")"_qs)
        result.clear();
    return result;
}


enum State {
    StateStart,
    StateCompare,
    StateExpression,
    StateCombination,
    StateInvalid
};


template <typename T>
static T findInTokens(const QString &word, const QVector<QPair<T, QString>> &tokens, T notFound = T { })
{
    for (const auto &token : tokens) {
        if (word.compare(token.second, Qt::CaseInsensitive) == 0)
            return token.first;
    }
    return notFound;
}

QVector<Filter> Filter::Parser::parse(const QString &str)
{
    // match words, which are either quoted with ["], quoted with ['] or unquoted
    static const QRegularExpression re(uR"-("([^"]*)"|'([^']*)'|([^ ]+))-"_qs);
    auto matches = re.globalMatch(str + u" &&"_qs);

    QVector<Filter> filters;
    Filter f;
    State state = StateStart;

    while (state != StateInvalid && matches.hasNext()) {
        auto match = matches.next();
        QString word = match.captured(1) + match.captured(2) + match.captured(3);

retryState:
        switch(state) {
        case StateStart: {
            int field = findInTokens(word, m_field_tokens, -2);

            if (field > -2) {
                f.setField(field);
                state = StateCompare;
            } else {
                state = StateExpression;
                goto retryState;
            }
            break;
        }
        case StateCompare: {
            auto comparison = findInTokens(word, m_comparison_tokens, Filter::Matches);
            f.setComparison(comparison);
            state = StateExpression;
            break;
        }
        case StateExpression: {
            f.setExpression(word);
            state = StateCombination;
            break;
        }
        case StateCombination: {
            auto notFound = Combination(-1);
            auto combination = findInTokens(word, m_combination_tokens, notFound);
            if (combination == notFound) {
                state = StateInvalid;
            } else {
                f.setCombination(combination);
                filters.append(f);
                f = Filter();
                state = StateStart;
            }
            break;
        }
        case StateInvalid:
            break;
        }
    }
    return filters;
}


template<typename T>
static QString toHtml(const QVector<QPair<T, QString>> &tokens, const QString &before, const QString &after,
                      const QString &key_before, const QString &key_after, const QString &key_separator,
                      const QString &value_before, const QString &value_after, const QString &value_separator)
{
    QVector<T> keys;
    for (const auto &p : tokens) {
        if (!keys.contains(p.first))
            keys.append(p.first);
    }

    QString res;
    bool first_key = true;
    for (const T &key : std::as_const(keys)) {
        if (first_key)
            first_key = false;
        else
            res = res + key_separator;
        res = res + key_before;
        bool first_value = true;
        for (const auto &p : tokens) {
            if (p.first != key)
                continue;

            if (first_value)
                first_value = false;
            else
                res = res + value_separator;
            res = res + value_before + quote(p.second).toHtmlEscaped() + value_after;
        }
        res = res + key_after;
    }
    if (!res.isEmpty())
        res = before + res + after;
    return res;
}

QString Filter::Parser::toolTip() const
{
    QString tt = Filter::tr("<p>Enter the filter expression in either (near) natural language or with logical operators.<br />"
       "A single expression looks like <b><i>FIELDNAME COMPARSION</i> TEXT</b>. <b><i>FIELDNAME</i></b> and "
       "<b><i>COMPARISON</i></b> are optional and default to <b>in any field</b> and <b>contains</b> respectively.</p>"
       "<p>Multiple expressions can be combined by separating them with a <b>COMBINATION</b> token.</p>"
       "<p>E.g. to search for anything resembling an brick in blue, you could use: <b>brick and color is blue</b></p>");

    QString block = u"<b><u>%1</u></b>%2"_qs;
    tt += block.arg(Filter::tr("Field names:"),
                    toHtml(m_field_tokens,
                           u"<ul><li>"_qs, u"</li></ul>"_qs,
                           { }, { }, u", "_qs,
                           u"<b>"_qs, u"</b>"_qs, u" / "_qs));
    tt += block.arg(Filter::tr("Comparisons:"),
                    toHtml(m_comparison_tokens,
                           u"<ul>"_qs, u"</ul>"_qs,
                           u"<li>"_qs, u"</li>"_qs, { },
                           u"<b>"_qs, u"</b>"_qs, u" / "_qs));
    tt += block.arg(Filter::tr("Combinations:"),
                    toHtml(m_combination_tokens,
                           u"<ul>"_qs, u"</ul>"_qs,
                           u"<li>"_qs, u"</li>"_qs, { },
                           u"<b>"_qs, u"</b>"_qs, u" / "_qs));
    return tt;
}

void Filter::Parser::setFieldTokens(const QVector<QPair<int, QString>> &tokens)
{
    m_field_tokens = tokens;
}

void Filter::Parser::setComparisonTokens(const QVector<QPair<Filter::Comparison, QString>> &tokens)
{
    m_comparison_tokens = tokens;
}

void Filter::Parser::setCombinationTokens(const QVector<QPair<Filter::Combination, QString>> &tokens)
{
    m_combination_tokens = tokens;
}

QVector<QPair<int, QString>> Filter::Parser::fieldTokens() const
{
    return m_field_tokens;
}

QVector<QPair<Filter::Comparison, QString>> Filter::Parser::comparisonTokens() const
{
    return m_comparison_tokens;
}

QVector<QPair<Filter::Combination, QString>> Filter::Parser::combinationTokens() const
{
    return m_combination_tokens;
}

void Filter::Parser::setStandardComparisonTokens(Filter::Comparisons mask)
{
    m_comparison_tokens = standardComparisonTokens(mask);
}

void Filter::Parser::setStandardCombinationTokens(Filter::Combinations mask)
{
    m_combination_tokens = standardCombinationTokens(mask);
}

QVector<QPair<Filter::Combination, QString>> Filter::Parser::standardCombinationTokens(Filter::Combinations mask)
{
    struct token_table { 
        Filter::Combination m_combination;
        const char *        m_symbols;
        const char *        m_words;
    } predefined[] = {
        { Filter::And, "&,&&", QT_TR_NOOP( "and" ) },
        { Filter::Or,  "|,||", QT_TR_NOOP( "or" ) },
        
        { Filter::And, nullptr, nullptr }
    };
    
    QVector<QPair<Filter::Combination, QString>> dct;
    
    for (token_table *tt = predefined; tt->m_symbols || tt->m_words; ++tt) {
        if (!(mask & tt->m_combination))
            continue;

        const auto words = Filter::tr(tt->m_words).split(u',');
        const auto symbols = QString::fromLatin1(tt->m_symbols).split(u',');
        for (const auto &word : words)
            dct.emplace_back(tt->m_combination, word);
        for (const auto &symbol : symbols)
            dct.emplace_back(tt->m_combination, symbol);
    }
    return dct;
}

QVector<QPair<Filter::Comparison, QString>> Filter::Parser::standardComparisonTokens(Filter::Comparisons mask)
{
    struct token_table { 
        Filter::Comparison m_comparison;
        const char *       m_symbols;
        const char *       m_words;
    } predefined[] = {
        { Filter::Is,               "=,==,===",         QT_TR_NOOP( "is,equals" ) },
        { Filter::IsNot,            "!=,=!,!==,==!,<>", QT_TR_NOOP( "is not,doesn't equal,does not equal" ) },
        { Filter::Less,             "<",                QT_TR_NOOP( "less than" ) },
        { Filter::LessEqual,        "<=,=<",            QT_TR_NOOP( "less equal than" ) },
        { Filter::Greater,          ">",                QT_TR_NOOP( "greater than" ) },
        { Filter::GreaterEqual,     ">=,=>",            QT_TR_NOOP( "greater equal than" ) },
        { Filter::Matches,          "~,~=,=~",          QT_TR_NOOP( "contains,matches" ) },
        { Filter::DoesNotMatch,     "!~,~!,!~=,!=~",    QT_TR_NOOP( "doesn't contain,does not contain,doesn't match,does not match" ) },
        { Filter::StartsWith,       "^,^=,=^",          QT_TR_NOOP( "starts with,begins with" ) },
        { Filter::DoesNotStartWith, "!^,^!=,!=^",       QT_TR_NOOP( "doesn't start with,does not start with,doesn't begin with,does not begin with" ) },
        { Filter::EndsWith,         "$,$=,=$",          QT_TR_NOOP( "ends with" ) },
        { Filter::DoesNotEndWith,   "!$,$!=,!=$",       QT_TR_NOOP( "doesn't end with,does not end with" ) },
        
        { Filter::Is, nullptr, nullptr }
    };
    
    QVector<QPair<Filter::Comparison, QString>> dct;
    
    for (token_table *tt = predefined; tt->m_symbols || tt->m_words; ++tt) {
        if (!(mask & tt->m_comparison))
            continue;

        const auto words = Filter::tr(tt->m_words).split(u',');
        const auto symbols = QString::fromLatin1(tt->m_symbols).split(u',');
        for (const auto &word : words)
            dct.emplace_back(tt->m_comparison, word);
        for (const auto &symbol : symbols)
            dct.emplace_back(tt->m_comparison, symbol);
    }
    return dct;
}

QDebug &operator<<(QDebug &dbg, const Filter &filter)
{
    dbg << "Filter { " << filter.field() << " " << filter.comparison() << " "  << filter.expression() << " "  << filter.combination() << " }";
    return dbg;
}
