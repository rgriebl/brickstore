#ifndef __FILTERPARSER_H__
#define __FILTERPARSER_H__

#include <QString>
#include <QMultiMap>
#include <QList>
#include <QPair>

class Filter {
public:
    enum Comparison {
        Matches          = 0x0001,
        DoesNotMatch     = 0x0002,
        Is               = 0x0004,
        IsNot            = 0x0008,
        Less             = 0x0010,
        GreaterEqual     = 0x0020,
        Greater          = 0x0040,
        LessEqual        = 0x0080,
        StartsWith       = 0x0100,
        DoesNotStartWith = 0x0200,
        EndsWith         = 0x0400,
        DoesNotEndWith   = 0x0800,        
    };
    
    Q_DECLARE_FLAGS(Comparisons, Comparison)
    
    enum Combination {
        And  = 0x01,
        Or   = 0x02,
    };
    
    Q_DECLARE_FLAGS(Combinations, Combination)

    Filter();
    
    inline int field() const                { return m_field; }
    inline QString expression() const       { return m_expression; }
    inline Comparison comparison() const    { return m_comparison; }
    inline Combination combination() const  { return m_combination; }
    
    void setField(int field);
    void setExpression(const QString &expr);
    void setComparison(Comparison cmp);
    void setCombination(Combination cmb);

    bool matches(const QVariant &v) const;
    
private:
    int         m_field;
    Comparison  m_comparison;
    Combination m_combination;
    QString     m_expression;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Filter::Comparisons)
Q_DECLARE_OPERATORS_FOR_FLAGS(Filter::Combinations)

class FilterParser {
public:
    FilterParser();

    QList<Filter> parse(const QString &str);

    void setFieldTokens(const QMultiMap<int, QString> &idToName);
    void setComparisonTokens(const QMultiMap<Filter::Comparison, QString> &comparisonToName);
    void setCombinationTokens(const QMultiMap<Filter::Combination, QString> &combinationToName);

    void setStandardComparisonTokens(Filter::Comparisons);
    void setStandardCombinationTokens(Filter::Combinations);

private:
    bool eatWhiteSpace(int &pos, const QString &str);

    template<typename T> T matchTokens(int &pos, const QString &str, const QMultiMap<T, QString> &tokens, const T default_result, int *start_of_token = 0);
    QPair<QString, Filter::Combination> matchFilterAndCombination(int &pos, const QString &str);

    QMultiMap<Filter::Comparison, QString> standardComparisonTokens(Filter::Comparisons mask);
    QMultiMap<Filter::Combination, QString> standardCombinationTokens(Filter::Combinations mask);
    
    QMultiMap<int,                 QString> m_field_tokens;
    QMultiMap<Filter::Comparison,  QString> m_comparison_tokens;
    QMultiMap<Filter::Combination, QString> m_combination_tokens;
};

#endif
