// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QWidget>
#include <QPointer>
#include <QDateTime>

#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QStringListModel)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QTimer)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QSplitter)
QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QButtonGroup)

class View;
class PriceGuideWidget;
class InventoryWidget;
class SelectItem;
class SelectColor;
class PictureWidget;
class Ui_AddItemWidget;

class AddItemDialog : public QWidget
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
    void changeEvent(QEvent *e) override;
    void showEvent(QShowEvent *) override;
    void closeEvent(QCloseEvent *e) override;
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
    QSpinBox *w_tier_qty[3];
    QDoubleSpinBox *w_tier_price[3];
    QSplitter *w_splitter_vertical;
    SelectItem *w_select_item;
    SelectColor *w_select_color;
    QSplitter *w_splitter_bottom;
    PictureWidget *w_picture;
    InventoryWidget *w_inventory;
    PriceGuideWidget *w_price_guide;
    QWidget *w_additem;
    Ui_AddItemWidget *w_ui_additem;
    QToolButton *w_go_back;
    QToolButton *w_go_next;
    QToolButton *w_menu;
    QLabel *w_last_added;
    QCheckBox *w_merge;
    QDialogButtonBox *w_buttons;
    QPushButton *w_add;

    QButtonGroup *m_tier_type;
    QButtonGroup *m_condition;

    QString m_price_label_fmt;

    QString m_currency_code;

    QAction *m_toggles[3];
    QAction *m_sellerMode;

    QStringListModel *m_favoriteFilters;

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
