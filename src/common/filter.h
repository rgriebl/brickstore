// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>
#include <QMultiMap>
#include <QVector>
#include <QPair>
#include <QDateTime>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDebug>


class Filter
{
    Q_DECLARE_TR_FUNCTIONS(Filter)

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

    Filter() = default;
    
    bool operator==(const Filter &other) const
    {
        return (m_field == other.m_field)
                && (m_comparison == other.m_comparison)
                && (m_combination == other.m_combination)
                && (m_expression == other.m_expression);
    }

    inline int field() const                { return m_field; }
    inline QString expression() const       { return m_expression; }
    inline Comparison comparison() const    { return m_comparison; }
    inline Combination combination() const  { return m_combination; }
    
    void setField(int field);
    void setExpression(const QString &expr);
    void setComparison(Comparison cmp);
    void setCombination(Combination cmb);

    class Parser {
    public:
        Parser() = default;

        QVector<Filter> parse(const QString &str);

        void setFieldTokens(const QVector<QPair<int, QString>> &idToName);
        void setComparisonTokens(const QVector<QPair<Filter::Comparison, QString>> &comparisonToName);
        void setCombinationTokens(const QVector<QPair<Filter::Combination, QString>> &combinationToName);

        QVector<QPair<int, QString>> fieldTokens() const;
        QVector<QPair<Filter::Comparison, QString>> comparisonTokens() const;
        QVector<QPair<Filter::Combination, QString>> combinationTokens() const;

        void setStandardComparisonTokens(Filter::Comparisons);
        void setStandardCombinationTokens(Filter::Combinations);

        QString toolTip() const;

        QString toString(const QVector<Filter> &filter, bool preferSymbolic = false) const;

    private:
        bool eatWhiteSpace(int &pos, const QString &str);

        template<typename T> T matchTokens(int &pos, const QString &str,
                                           const QVector<QPair<T, QString>> &tokens,
                                           const T default_result, int *start_of_token = nullptr);
        QPair<QString, Filter::Combination> matchFilterAndCombination(int &pos, const QString &str);

        QVector<QPair<Filter::Comparison, QString>> standardComparisonTokens(Filter::Comparisons mask);
        QVector<QPair<Filter::Combination, QString>> standardCombinationTokens(Filter::Combinations mask);

        QVector<QPair<int,                 QString>> m_field_tokens;
        QVector<QPair<Filter::Comparison,  QString>> m_comparison_tokens;
        QVector<QPair<Filter::Combination, QString>> m_combination_tokens;
    };

    template<typename T> bool is() const
    {
        if constexpr (std::is_same_v<T, int>)
            return m_isInt;
        else if constexpr (std::is_same_v<T, double>)
            return m_isDouble;
        else if constexpr (std::is_same_v<T, QRegularExpression>)
            return m_isRegExp;
        else
            return false;
    }

    template<typename T> T as() const
    {
        if constexpr (std::is_same_v<T, int>)
            return m_asInt;
        else if constexpr (std::is_same_v<T, double>)
            return m_asDouble;
        else if constexpr (std::is_same_v<T, QRegularExpression>)
            return m_asRegExp;
        else if constexpr (std::is_same_v<T, QDateTime>)
            return m_asDateTime;
        else {
            Q_ASSERT(false);
        }
    }

private:
    QString     m_expression;
    int         m_field = -1;
    Comparison  m_comparison = Matches;
    Combination m_combination = And;
    bool        m_isInt = false;
    bool        m_isDouble = false;
    bool        m_isRegExp = false;
    int         m_asInt = 0;
    double      m_asDouble = 0;
    QDateTime   m_asDateTime;
    QRegularExpression m_asRegExp;
};

QDebug &operator<<(QDebug &dbg, const Filter &filter);

Q_DECLARE_METATYPE(Filter)
Q_DECLARE_OPERATORS_FOR_FLAGS(Filter::Comparisons)
Q_DECLARE_OPERATORS_FOR_FLAGS(Filter::Combinations)
