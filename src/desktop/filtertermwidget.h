/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#pragma once

#include <QWidget>
#include <QPointer>

#include "common/filter.h"

QT_FORWARD_DECLARE_CLASS(QStackedLayout)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)
QT_FORWARD_DECLARE_CLASS(QTimer)
QT_FORWARD_DECLARE_CLASS(QToolButton)

class Document;
class HistoryLineEdit;
class FilterTermWidget;


class FilterWidget : public QWidget
{
    Q_OBJECT
public:
    FilterWidget(QWidget *parent = nullptr);

    void setIconSize(const QSize &s);
    void setDocument(Document *doc);

    QAction *action();

    QVector<Filter> filter() const;
    void setFilter(const QVector<Filter> &filter);

signals:
    void filterChanged(const QVector<Filter> &filter);
    void visibilityChanged(bool b);

protected:
    void changeEvent(QEvent *e) override;

private:
    enum class ChangedVia { Terms, Edit, Externally };

    void emitFilterChanged(const QVector<Filter> &filter, ChangedVia changedVia);
    QVector<Filter> filterFromTerms() const;
    void setFilterFromModel();
    void addFilterTerm(const Filter &filter = Filter { });
    void setFilterText(const QVector<Filter> &filter);
    void setFilterTerms(const QVector<Filter> &filter);

    void languageChange();

    Document *m_doc = nullptr;
    QObject *m_viewConnectionContext = nullptr;

    QAction *m_onOff;
    HistoryLineEdit *m_edit;
    QTimer *m_editDelay;
    QToolButton *m_refilter;
    QToolButton *m_menu;
    QAction *m_copy;
    QAction *m_paste;
    QAction *m_mode;

    QWidget *m_termsContainer;
    QLayout *m_termsFlow;

    QVector<Filter> m_filter;
};

class FilterTermWidget : public QWidget
{
    Q_OBJECT
public:
    FilterTermWidget(Document *doc, const Filter &filter, QWidget *parent = nullptr);

    void resetCombination();

    Filter::Combination combination() const;
    const Filter &filter() const;
    QString filterString() const;

signals:
    void combinationChanged(Filter::Combination combination);
    void deleteClicked();
    void filterChanged(const Filter &filter);

protected:
    void paintEvent(QPaintEvent *) override;
    void emitFilterChanged();
    void updateValueModel(int field);
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    Filter::Parser *m_parser;
    Document *m_doc;
    Filter m_filter;
    QTimer *m_valueDelay = nullptr;
    QComboBox *m_fields;
    QComboBox *m_comparisons;
    QComboBox *m_value;
    QButtonGroup *m_andOrGroup;
    bool m_nextMouseUpOpensPopup = false;
    QVector<int> m_visualColumnOrder;
};
