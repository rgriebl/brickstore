
#include <QStringList>
#include <QCoreApplication>
#include <QVariant>
#include <QtDebug>

#include "filterparser.h"



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
    bool isint = false, isfloat = false;
    int i1, i2;
    double d1, d2;
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
        isfloat = true;
        break;
    }    
    default:
        s1 = m_expression;
        s2 = v.toString();
        break;
    }

    switch (comparison()) {
    case Is:
        return isint ? i1 == i2 : isfloat ? d1 == d2 : s2.compare(s1, Qt::CaseInsensitive) == 0;
    case IsNot:
        return isint ? i1 != i2 : isfloat ? d1 != d2 : s2.compare(s1, Qt::CaseInsensitive) != 0;
    case Less:
        return isint ? i1 < i2 : isfloat ? d1 < d2 : false;
    case LessEqual:
        return isint ? i1 <= i2 : isfloat ? d1 <= d2 : false;
    case Greater:
        return isint ? i1 > i2 : isfloat ? d1 > d2 : false;
    case GreaterEqual:
        return isint ? i1 >= i2 : isfloat ? d1 >= d2 : false;
    case StartsWith:
        return isint || isfloat ? false : s2.startsWith(s1, Qt::CaseInsensitive);
    case DoesNotStartWith:
        return isint || isfloat ? false : !s2.startsWith(s1, Qt::CaseInsensitive);
    case EndsWith:
        return isint || isfloat ? false : s2.endsWith(s1, Qt::CaseInsensitive);
    case DoesNotEndWith:
        return isint || isfloat ? false : !s2.endsWith(s1, Qt::CaseInsensitive);
    case Matches:
    case DoesNotMatch: {
        if (isint || isfloat) {
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

FilterParser::FilterParser()
{
}


QList<Filter> FilterParser::parse(const QString &str_)
{
    QList<Filter> filters;
    int pos = 0;
    Filter f;
    State state = StateStart;
    QString str = str_.simplified();

    qDebug() << "Parsing filter expr:" << str;
    
    while (state != StateInvalid && pos < str.length()) {
        
        if (!eatWhiteSpace(pos, str))
            break;

        switch(state) {
        case StateStart: {
            int field = matchTokens(pos, str, m_field_tokens);

            qDebug() << "Got field: " << field;

            if (field > -1) {
                f.setField(field);
                state = StateCompare;
            }
            else
                state = StateFilter;
            break;
        }
        case StateCompare: {
            f.setComparison(matchTokens(pos, str, m_comparison_tokens));
            
            qDebug() << "Got comparison: " << f.comparison();
            state = StateFilter;
            break;
        }
        case StateFilter: {
            QPair<QString, Filter::Combination> res = matchFilterAndCombination(pos, str);
            
            qDebug() << "Got expression: \"" << res.first << "\" with combination: " << res.second;
            
            f.setExpression(res.first);
            f.setCombination(res.second);
            filters.append(f);
            state = StateStart;
            break;
        }
        case StateInvalid:
            break;
        }
        qDebug() << "State: " << state << "  Position: " << pos;
    }
    qDebug() << "Parsed " << filters.count() << " subexpressions";
    return filters;
}

bool FilterParser::eatWhiteSpace(int &pos, const QString &str)
{
    int len = str.length();

    // eat ws
    while (pos < len && str[pos].isSpace())
        pos++;

    // empty or only ws
    return (pos != len);
}

template<typename T>
T FilterParser::matchTokens(int &pos, const QString &str, const QMultiMap<T, QString> &tokens, int *start_of_token)
{
    int len = str.length();

    T found_field = (T) -1;
    int found_len = -1;
    int found_pos = -1;
  
    qDebug() << "Matching token:" << str.mid(pos);
    qDebug() << "       against:" << tokens;  

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
            qDebug("=======================X");
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
    qDebug("**************** %d", found_field);
    if (found_field != (T) -1) {
//        if (start_of_token)
//            pos = found_pos;
        pos += found_len;
    }
    if (start_of_token)
        *start_of_token = found_pos;
    return found_field;
}

QPair<QString, Filter::Combination> FilterParser::matchFilterAndCombination(int &pos, const QString &str)
{
    QPair<QString, Filter::Combination> res;
    res.second = Filter::And;
    
    int len = str.length();
    QChar quote_char = str[0];
    bool quoted = false;
    
    if (quote_char == QLatin1Char('\'') || quote_char == QLatin1Char('"')) {
        quoted = true;
        pos++;
    }
    
    if (quoted) {
        int end = str.indexOf(quote_char, pos);
        
        if (end == -1) {
            // missing quote end: just take everything as filter expr
        
            res.first = str.mid(pos);
            pos = len;
        } else {
            res.first = str.mid(pos, end - pos);
            pos = end + 1;
            res.second = matchTokens(pos, str, m_combination_tokens);
        }
    } else {
        int start_of_token = -1;
        res.second = matchTokens(pos, str, m_combination_tokens, &start_of_token);
        
        if (start_of_token == -1) {
            res.first = str.mid(pos);
            pos += len;
            
            qDebug() << "Did not find token for:" << str << " - Pos:" << pos << "Len:" << len;
        } else {
            res.first = str.mid(pos, start_of_token - pos).trimmed();
        }
    }
    return res;
}

void FilterParser::setFieldTokens(const QMultiMap<int, QString> &tokens)
{
    m_field_tokens = tokens;
}

void FilterParser::setComparisonTokens(const QMultiMap<Filter::Comparison, QString> &tokens)
{
    m_comparison_tokens = tokens;
}

void FilterParser::setCombinationTokens(const QMultiMap<Filter::Combination, QString> &tokens)
{
    m_combination_tokens = tokens;
}

void FilterParser::setStandardComparisonTokens(Filter::Comparisons mask)
{
    m_comparison_tokens = standardComparisonTokens(mask);
}

void FilterParser::setStandardCombinationTokens(Filter::Combinations mask)
{
    m_combination_tokens = standardCombinationTokens(mask);
}

QMultiMap<Filter::Combination, QString> FilterParser::standardCombinationTokens(Filter::Combinations mask)
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
        if (!mask & tt->m_combination)
            continue;
    
        foreach (QString symbol, QString::fromLatin1(tt->m_symbols).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, symbol); 

        foreach (QString word, QString::fromLatin1(tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, word);

        foreach (QString word, qApp->translate("FilterParser", tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, word);
    }
    return dct;
}

QMultiMap<Filter::Comparison, QString> FilterParser::standardComparisonTokens(Filter::Comparisons mask)
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
        if (!mask & tt->m_comparison)
            continue;
    
        foreach (QString symbol, QString::fromLatin1(tt->m_symbols).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, symbol); 

        foreach (QString word, QString::fromLatin1(tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, word);

        foreach (QString word, qApp->translate("FilterParser", tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, word);
    }
    return dct;
}
