/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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

#include <QStringList>
#include <QCoreApplication>
#include <QVariant>
#include <QtDebug>

#include "filter.h"



Filter::Filter()
    : m_field(-1), m_comparison(Matches), m_combination(And)
{ }

void Filter::setField(int field)
{
    m_field = field;
}
void Filter::setExpression(const QString &expr)
{
    m_expression = expr;
}

void Filter::setComparison(Comparison cmp)
{
    m_comparison = cmp;
}

void Filter::setCombination(Combination cmb)
{
    m_combination = cmb;
}

bool Filter::matches(const QVariant &v) const
{
    bool isint = false, isdbl = false;
    int i1 = 0, i2 = 0;
    double d1 = 0, d2 = 0;
    QString s1, s2;
    
    switch (v.type()) {
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong: {
        bool ok = false;        
        i1 = m_expression.toInt(&ok);
        if (!ok)
            return false; // data is int, but expression is not
        i2 = v.toInt();
        isint = true;
        break;
    }   
    case QVariant::Double: {
        bool ok = false;
        d1 = m_expression.toDouble(&ok);
        if (!ok)
            return false;
        d2 = v.toDouble();
        isdbl = true;
        break;
    }    
    default:
        s1 = m_expression;
        s2 = v.toString();
        break;
    }

    switch (comparison()) {
    case Is:
        return isint ? i2 == i1 : isdbl ? d2 == d1 : s2.compare(s1, Qt::CaseInsensitive) == 0;
    case IsNot:
        return isint ? i2 != i1 : isdbl ? d2 != d1 : s2.compare(s1, Qt::CaseInsensitive) != 0;
    case Less:
        return isint ? i2 < i1 : isdbl ? d2 < d1 : false;
    case LessEqual:
        return isint ? i2 <= i1 : isdbl ? d2 <= d1 : false;
    case Greater:
        return isint ? i2 > i1 : isdbl ? d2 > d1 : false;
    case GreaterEqual:
        return isint ? i2 >= i1 : isdbl ? d2 >= d1 : false;
    case StartsWith:
        return isint || isdbl ? false : s2.startsWith(s1, Qt::CaseInsensitive);
    case DoesNotStartWith:
        return isint || isdbl ? false : !s2.startsWith(s1, Qt::CaseInsensitive);
    case EndsWith:
        return isint || isdbl ? false : s2.endsWith(s1, Qt::CaseInsensitive);
    case DoesNotEndWith:
        return isint || isdbl ? false : !s2.endsWith(s1, Qt::CaseInsensitive);
    case Matches:
    case DoesNotMatch: {
        if (isint || isdbl) {
            s1 = m_expression;
            s2 = v.toString();
        }
        bool res = s2.contains(QRegExp(s1, Qt::CaseInsensitive));
    
        return (comparison() == Matches) ? res : !res;
    }
    }
    return false;
}


enum State {
    StateStart,
    StateCompare,
    StateFilter,
    StateInvalid
};

Filter::Parser::Parser()
{
}


QList<Filter> Filter::Parser::parse(const QString &str_)
{
    QList<Filter> filters;
    int pos = 0;
    Filter f;
    State state = StateStart;
    QString str = str_.simplified();

    while (state != StateInvalid && pos < str.length()) {
        
        if (!eatWhiteSpace(pos, str))
            break;

        switch(state) {
        case StateStart: {
            int field = matchTokens(pos, str, m_field_tokens, -1);

            if (field > -1) {
                f.setField(field);
                state = StateCompare;
            }
            else
                state = StateFilter;
            break;
        }
        case StateCompare: {
            f.setComparison(matchTokens(pos, str, m_comparison_tokens, Filter::Matches));
            
            state = StateFilter;
            break;
        }
        case StateFilter: {
            QPair<QString, Filter::Combination> res = matchFilterAndCombination(pos, str);
            
            f.setExpression(res.first);
            f.setCombination(res.second);
            filters.append(f);
            state = StateStart;
            f = Filter();
            break;
        }
        case StateInvalid:
            break;
        }
    }
    return filters;
}

bool Filter::Parser::eatWhiteSpace(int &pos, const QString &str)
{
    int len = str.length();

    // eat ws
    while (pos < len && str[pos].isSpace())
        pos++;

    // empty or only ws
    return (pos != len);
}

template<typename T>
T Filter::Parser::matchTokens(int &pos, const QString &str, const QMultiMap<T, QString> &tokens, const T defaultresult, int *start_of_token)
{
    int len = str.length();

    T found_field = defaultresult;
    int found_len = -1;
    int found_pos = -1;
  
    QMapIterator<T, QString> it(tokens);
    while (it.hasNext()) {
        it.next();

        int flen = it.value().length();
        
        if (len - pos >= flen) {    
            if (!start_of_token) {
                if (!it.value().compare(str.mid(pos, flen), Qt::CaseInsensitive) && found_len < flen) {
                    found_field = it.key();
                    found_len = flen;
                }
            }
            else {
                int fpos = str.indexOf(it.value(), pos, Qt::CaseInsensitive);
                
                if (fpos >= 0 && (found_pos == -1 || fpos <= found_pos) && found_len < flen &&
                    (fpos == 0 || str[fpos - 1].isSpace())) {
                    found_field = it.key();
                    found_len = flen;
                    found_pos = fpos;
                }
            }
        }  
    }
    
    // token not found:
    //   start_of_token = -1
    //   result = default
    //   pos (unchanged)
    
    // token found:
    //   start_of_token = found_pos (if start_of_token != 0)
    //   found_field = it.key()
    //   pos = (start_of_token ? found_pos + found_len : pos + found_len)
    
    if (found_len > 0) {
        if (start_of_token) {
            *start_of_token = found_pos;
            pos = found_pos + found_len;
        } else {
            pos += found_len;
        }
    } else {
        if (start_of_token)
            *start_of_token = -1;
    }
    return found_len > 0 ? found_field : defaultresult;
}

QPair<QString, Filter::Combination> Filter::Parser::matchFilterAndCombination(int &pos, const QString &str)
{
    QPair<QString, Filter::Combination> res;
    res.second = Filter::And;
    
    int len = str.length();
    QChar quote_char = str[pos];
    bool quoted = false;
    
    if (quote_char == QLatin1Char('\'') || quote_char == QLatin1Char('"')) {
        quoted = true;
        pos++;
    }
    
    if (quoted) {
        int end = str.indexOf(quote_char, pos+1);
        
        if (end == -1) {
            // missing quote end: just take everything as filter expr
        
            res.first = str.mid(pos);
            pos = len;
        } else {
            res.first = str.mid(pos, end - pos);
            pos = end + 1;
            res.second = matchTokens(pos, str, m_combination_tokens, Filter::And);
        }
    } else {
        int start_of_token = -1;
        int oldpos = pos;
        res.second = matchTokens(pos, str, m_combination_tokens, Filter::And, &start_of_token);
        
        if (start_of_token == -1) {
            res.first = str.mid(pos);
            pos += len;
        } else {
            res.first = str.mid(oldpos, start_of_token - oldpos).trimmed();
        }
    }
    return res;
}

template<typename T>
static QString toString(const QMultiMap<T, QString> &tokens, const QString &before, const QString &after,
                        const QString &key_before, const QString &key_after, const QString &key_separator,
                        const QString &value_before, const QString &value_after, const QString &value_separator)
{
    QString res;
    bool first_key = true;
    foreach (T key, tokens.uniqueKeys()) {
        if (first_key)
            first_key = false;
        else
            res += key_separator;
        res += key_before;
        bool first_value = true;
        foreach (const QString val, tokens.values(key)) {
            if (first_value)
                first_value = false;
            else
                res += value_separator;
            res = res + value_before + val + value_after;
        }
        res += key_after;
    }
    if (!res.isEmpty())
        res = before + res + after;
    return res;
}

QString Filter::Parser::toolTip() const
{
    QString tt = Filter::tr("<p>Enter the filter expression(s) in either (near) natural language or with logical operators.<br />"
       "A single expression looks like <b><i>FIELDNAME COMPARSION</i> TEXT</b>. <b><i>FIELDNAME</i></b> and "
       "<b><i>COMPARISON</i></b> are optional and default to <b>in any field</b> and <b>contains</b> respectively.</p>"
       "<p>Multiple filters can be combined by separating them with a <b>COMBINATION</b> token.</p>"
       "<p>E.g. to search for anything resembling an brick in blue, you could use: <b>brick and color is blue</b></p>");

    QString block = QLatin1String("<b><u>%1</u></b>%2");
    tt += block.arg(Filter::tr("Field names:")).arg(toString(m_field_tokens,
                                                    QLatin1String("<ul><li>"), QLatin1String("</li></ul>"),
                                                    QString(),  QString(), QLatin1String(", "),
                                                    QLatin1String("<b>"), QLatin1String("</b>"), QLatin1String(" / ")));
    tt += block.arg(Filter::tr("Comparisons:")).arg(toString(m_comparison_tokens,
                                                    QLatin1String("<ul>"), QLatin1String("</ul>"),
                                                    QLatin1String("<li>"), QLatin1String("</li>"), QString(),
                                                    QLatin1String("<b>"), QLatin1String("</b>"), QLatin1String(" / ")));
    tt += block.arg(Filter::tr("Combinations:")).arg(toString(m_combination_tokens,
                                                     QLatin1String("<ul>"), QLatin1String("</ul>"),
                                                     QLatin1String("<li>"), QLatin1String("</li>"), QString(),
                                                     QLatin1String("<b>"), QLatin1String("</b>"), QLatin1String(" / ")));
    return tt;
}

void Filter::Parser::setFieldTokens(const QMultiMap<int, QString> &tokens)
{
    m_field_tokens = tokens;
}

void Filter::Parser::setComparisonTokens(const QMultiMap<Filter::Comparison, QString> &tokens)
{
    m_comparison_tokens = tokens;
}

void Filter::Parser::setCombinationTokens(const QMultiMap<Filter::Combination, QString> &tokens)
{
    m_combination_tokens = tokens;
}

void Filter::Parser::setStandardComparisonTokens(Filter::Comparisons mask)
{
    m_comparison_tokens = standardComparisonTokens(mask);
}

void Filter::Parser::setStandardCombinationTokens(Filter::Combinations mask)
{
    m_combination_tokens = standardCombinationTokens(mask);
}

QMultiMap<Filter::Combination, QString> Filter::Parser::standardCombinationTokens(Filter::Combinations mask)
{
    struct token_table { 
        Filter::Combination m_combination;
        const char *        m_symbols;
        const char *        m_words;
    } predefined[] = {
        { Filter::And, "&,&&", QT_TR_NOOP( "and" ) },
        { Filter::Or,  "|,||", QT_TR_NOOP( "or" ) },
        
        { Filter::And, 0, 0 }
    };
    
    QMultiMap<Filter::Combination, QString> dct;
    
    for (token_table *tt = predefined; tt->m_symbols || tt->m_words; ++tt) {
        if (!(mask & tt->m_combination))
            continue;
    
        foreach (QString symbol, QString::fromLatin1(tt->m_symbols).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, symbol); 

//        foreach (QString word, QString::fromLatin1(tt->m_words).split(QLatin1Char(',')))
//            dct.insert(tt->m_combination, word);

        foreach (QString word, qApp->translate("Filter::Parser", tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, word);
    }
    return dct;
}

QMultiMap<Filter::Comparison, QString> Filter::Parser::standardComparisonTokens(Filter::Comparisons mask)
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
        
        { Filter::Is, 0, 0 }
    };
    
    QMultiMap<Filter::Comparison, QString> dct;
    
    for (token_table *tt = predefined; tt->m_symbols || tt->m_words; ++tt) {
        if (!(mask & tt->m_comparison))
            continue;
    
        foreach (QString symbol, QString::fromLatin1(tt->m_symbols).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, symbol); 

//        foreach (QString word, QString::fromLatin1(tt->m_words).split(QLatin1Char(',')))
//            dct.insert(tt->m_comparison, word);

        foreach (QString word, qApp->translate("Filter::Parser", tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, word);
    }
    return dct;
}
