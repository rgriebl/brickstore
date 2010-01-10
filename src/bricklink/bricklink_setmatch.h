#ifndef BRICKLINK_SETMATCH_H
#define BRICKLINK_SETMATCH_H

#include <QBitArray>

#include "bricklink.h"

namespace BrickLink {

class SetMatch : public QObject {
    Q_OBJECT

private:
    struct InvMatchItem {
        const Item *item;
        const Color *color;
        int qty;
    };

    class InvMatchList {
    public:
        InvMatchList();
        InvMatchList(const InvItemList &list);
        InvMatchList(const InvMatchList &copy);

        InvMatchList &operator=(const InvMatchList &copy);

        void add(const InvItemList &list);
        //bool subtract(const InvItemList &list);
        bool subtract(const InvMatchList &list);

        int count() const;
        bool isEmpty() const;

        friend class SetMatch;

    private:
        QList<InvMatchItem> m_list;
        int m_count;
    };


public:
    enum Algorithm {
        Greedy,
        Recursive
    };

    SetMatch(QObject *parent = 0);
    ~SetMatch();

    bool isActive() const;

    bool startAllPossibleSetMatch(const InvItemList &list);
    bool startMaximumPossibleSetMatch(const InvItemList &list, Algorithm a = Greedy);

    void setPartCountConstraint(int _min, int _max);
    void setYearReleasedConstraint(int _min, int _max);
    void setCategoryConstraint(const QList<const Category *> &list);
    void setItemTypeConstraint(const QList<const ItemType *> &list);

    enum GreedyPreference {
        PreferLargerSets  = 0,
        PreferSmallerSets = 1,
    };

    void setGreedyPreference(GreedyPreference p);

    void setRecursiveBound(float unmatched);

signals:
    void finished(QList<const Item *> &);
    void progress(int, int);

protected:
    QList<const Item *> allPossibleSetMatch(const InvItemList &list);
    QList<const Item *> maximumPossibleSetMatch(const InvItemList &list);

    friend class MatchThread;

private:
    QPair<int, QList<const Item *> > set_match_greedy(InvMatchList &parts);
    QPair<int, QList<const Item *> > set_match_recursive(QVector<QPair<const Item *, InvMatchList> >::const_iterator it, InvMatchList &parts);

    void clear_inventory_list();
    void create_inventory_list();

    bool compare_pairs(const QPair<const Item *, InvMatchList> &p1, const QPair<const Item *, InvMatchList> &p2);

    class GreedyComparePairs {
    public:
        GreedyComparePairs(bool prefer_small);
        bool operator()(const QPair<const Item *, InvMatchList> &p1, const QPair<const Item *, InvMatchList> &p2);

    private:
        bool m_prefer_small;
    };

private:
    Algorithm m_algorithm;

    // constraints
    int m_part_min;
    int m_part_max;
    int m_year_min;
    int m_year_max;
    QList<const Category *> m_categories;
    QList<const ItemType *> m_itemtypes;

    // greedy preferences
    GreedyPreference m_prefer;

    // recursive stop value
    float m_stopvalue;
    QBitArray m_doesnotfit;
    int m_startcount;

    // state
    int m_step;
    QVector<QPair<const Item *, InvMatchList> > m_inventories;
    bool m_active;
};

} //namespace BrickLink

#endif
