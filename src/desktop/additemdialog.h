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
#include <QDateTime>

#include "ui_additemdialog.h"


QT_FORWARD_DECLARE_CLASS(QValidator)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QTimer)

class View;


class AddItemDialog : public QWidget, private Ui::AddItemDialog
{
    Q_OBJECT
public:
    AddItemDialog(QWidget *parent = nullptr);
    ~AddItemDialog() override;

    void attach(View *view);

    void goToItem(const BrickLink::Item *item, const BrickLink::Color *color = nullptr);

signals:
    void closed();

protected slots:
    void languageChange();

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void updateCaption();
    void updateCurrencyCode();
    void updateItemAndColor();
    bool checkAddPossible();
    void addClicked();
    void checkTieredPrices();
    void setTierType(int type);
    void setSellerMode(bool);

private:
    double tierPriceValue(int i);
    void updateAddHistoryText();
    static QString addhistoryTextFor(const QDateTime &when, const BrickLink::Lot &lot);

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

private:
    QPointer<View> m_view;

    QAction *m_invGoToAction;
    QPushButton *w_add;
    QSpinBox *w_tier_qty[3];
    QDoubleSpinBox *w_tier_price[3];

    QButtonGroup *m_tier_type;
    QButtonGroup *m_condition;

    QString m_caption_fmt;
    QString m_price_label_fmt;

    QString m_currency_code;

    QAction *m_toggles[3];
    QAction *m_sellerMode;

    QMenu *m_backMenu;
    QMenu *m_nextMenu;
    QMenu *m_historyMenu;

    QTimer *m_historyTimer;
    QVector<std::pair<QDateTime, BrickLink::Lot>> m_addHistory;

    struct BrowseHistoryEntry
    {
        QByteArray m_fullItemId;
        uint m_colorId;
        QDateTime m_lastVisited;
        QByteArray m_itemState;
        QByteArray m_colorState;
        QByteArray m_addState;
    };

    QVector<BrowseHistoryEntry> m_browseStack;
    QVector<BrowseHistoryEntry> m_browseHistory;

    int m_browseStackIndex = -1;
    bool m_currentlyRestoringBrowseHistory = false;

    static constexpr qsizetype MaxBrowseHistory = 100;
    static constexpr qsizetype MaxBrowseStack = 15;

    enum class BrowseMenuType { Back, Next, History };

    void recordBrowseEntry(bool onlyUpdateHistory = false);
    bool replayBrowseEntry(BrowseMenuType type, int pos);
    void buildBrowseMenu(BrowseMenuType type);
    void updateBrowseActions();


    QByteArray saveBrowseState() const;
    bool restoreBrowseState(const QByteArray &ba);
};
