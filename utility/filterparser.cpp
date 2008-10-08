
#include <QStringList>
#include <QCoreApplication>

#include "filterparser.h"

enum State {
    StateStart,
    StateCompare,
    StateFilter,
    StateInvalid
};

FilterParser::FilterParser()
{
    m_comparison_tokens = defaultComparisonTokens();
    m_combination_tokens = defaultCombinationTokens();
}


QList<Filter> FilterParser::parse(const QString &str_)
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
            int field = matchTokens(pos, str, m_field_tokens);

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
            state = StateFilter;
            break;
        }
        case StateFilter: {
            QPair<QString, Filter::Combination> res = matchFilterAndCombination(pos, str);
            
            f.setExpression(res.first);
            f.setCombination(res.second);
            state = StateStart;
            break;
        }
        case StateInvalid:
            break;
        }
    }
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
  
    QMapIterator<T, QString> it(tokens);
    while (it.hasNext()) {
        it.next();

        int flen = it.value().length();
        
        if (len - pos >= flen) {
            if (!start_of_token) {
                QStringRef sr(&str, pos, flen);
                if (it.value() == sr && found_len < flen) {
                    found_field = it.key();
                    found_len = flen;
                }
            }
            else {
                int fpos = str.indexOf(it.value(), pos);
                
                if (fpos >= 0 && fpos <= found_pos && found_len < flen &&
                    (fpos == 0 || str[fpos - 1].isSpace())) {
                    found_field = it.key();
                    found_len = flen;
                    found_pos = fpos;
                }
            }
        }  
    }
    if (found_field != (T) -1)
        pos += found_len;
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
            pos = len;
        } else {
            res.first = str.mid(pos, start_of_token - pos).trimmed();
        }
    }
    return res;
}

void FilterParser::setComparisonTokens(const QMultiMap<Filter::Comparison, QString> &tokens)
{
    m_comparison_tokens = tokens;
}

void FilterParser::setCombinationTokens(const QMultiMap<Filter::Combination, QString> &tokens)
{
    m_combination_tokens = tokens;
}

QMultiMap<Filter::Combination, QString> FilterParser::defaultCombinationTokens()
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
    
    QMap<Filter::Combination, QString> dct;
    
    for (token_table *tt = predefined; tt->m_symbols || tt->m_words; ++tt) {
        foreach (QString symbol, QString::fromLatin1(tt->m_symbols).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, symbol); 

        foreach (QString word, QString::fromLatin1(tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, word);

        foreach (QString word, qApp->translate("FilterParser", tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_combination, word);
    }
    return dct;
}

QMultiMap<Filter::Comparison, QString> FilterParser::defaultComparisonTokens()
{
    struct token_table { 
        Filter::Comparison m_comparison;
        const char *       m_symbols;
        const char *       m_words;
    } predefined[] = {
        { Filter::Is,                           "=,==,===",         QT_TR_NOOP( "is,equals" ) },
        { Filter::Is | Filter::Negated,         "!=,=!,!==,==!,<>", QT_TR_NOOP( "is not,doesn't equal,does not equal" ) },
        { Filter::Less,                         "<",                QT_TR_NOOP( "less than" ) },
        { Filter::Greater | Filter::Negated,    "<=,=<",            QT_TR_NOOP( "less equal than" ) },
        { Filter::Greater,                      ">",                QT_TR_NOOP( "greater than" ) },
        { Filter::Less | Filter::Negated,       ">=,=>",            QT_TR_NOOP( "greater equal than" ) },
        { Filter::Matches,                      "~,~=,=~",          QT_TR_NOOP( "contains,matches" ) },
        { Filter::Matches | Filter::Negated,    "!~,~!,!~=,!=~",    QT_TR_NOOP( "doesn't contain,does not contain,doesn't match,does not match" ) },
        { Filter::StartsWith,                   "^,^=,=^",          QT_TR_NOOP( "starts with,begins with" ) },
        { Filter::StartsWith | Filter::Negated, "!^,^!=,!=^",       QT_TR_NOOP( "doesn't start with,does not start with,doesn't begin with,does not begin with" ) },
        { Filter::EndsWith,                     "$,$=,=$",          QT_TR_NOOP( "ends with" ) },
        { Filter::EndsWith | Filter::Negated,   "!$,$!=,!=$",       QT_TR_NOOP( "doesn't end with,does not end with" ) },
        
        { 0, 0, 0 }
    };
    
    QMap<Filter::Comparison, QString> dct;
    
    for (token_table *tt = predefined; tt->m_symbols || tt->m_words; ++tt) {
        foreach (QString symbol, QString::fromLatin1(tt->m_symbols).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, symbol); 

        foreach (QString word, QString::fromLatin1(tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, word);

        foreach (QString word, qApp->translate("FilterParser", tt->m_words).split(QLatin1Char(',')))
            dct.insert(tt->m_comparison, word);
    }
    return dct;
}
