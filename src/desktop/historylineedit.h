// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QLineEdit>
#include <QStringListModel>
#include <QIcon>

QT_FORWARD_DECLARE_CLASS(QAction)
class HistoryViewDelegate;


class HistoryLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    HistoryLineEdit(QWidget *parent = nullptr);
    HistoryLineEdit(int maximumHistorySize, QWidget *parent = nullptr);

    bool isInFavoritesMode() const;
    void setToFavoritesMode(bool favoritesMode);

    void setModel(QStringListModel *model);

    QString instructionToolTip() const;

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

    QAction *reFilterAction();

protected:
    void keyPressEvent(QKeyEvent *ke) override;
    void changeEvent(QEvent *e) override;

private:
    void appendToModel();
    void showPopup();
    void setFilterPixmap();

    QStringListModel *m_filterModel;
    QIcon m_deleteIcon;
    QAction *m_popupAction;
    int m_maximumHistorySize;
    bool m_favoritesMode = false;
    QMetaObject::Connection m_connection;
    QAction *m_reFilterAction;

    friend class HistoryViewDelegate;
    friend class HistoryView;
};
