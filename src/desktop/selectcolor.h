// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QTreeView)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QToolButton)


class SelectColor : public QWidget
{
    Q_OBJECT
public:
    enum class Feature {
        ColorLock = 1,
    };

    explicit SelectColor(QWidget *parent = nullptr);
    explicit SelectColor(const QVector<Feature> &features, QWidget *parent = nullptr);

    void setWidthToContents(bool b);

    void setCurrentColor(const BrickLink::Color *color);
    void setCurrentColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item);
    const BrickLink::Color *currentColor() const;

    bool colorLock() const;
    void setColorLock(bool locked);

    void setShowInputError(bool show);

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);
    static QByteArray defaultState();

signals:
    void colorSelected(const BrickLink::Color *color, bool confirmed);
    void colorLockChanged(const BrickLink::Color *color);

protected slots:
    void colorConfirmed();
    void updateColorFilter(int filter);
    void languageChange();

protected:
    void changeEvent(QEvent *) override;
    void showEvent(QShowEvent *) override;

protected:
    QComboBox *w_filter;
    QTreeView *w_colors;
    QLineEdit *w_nocolor;
    QToolButton *w_lock;
    BrickLink::ColorModel *m_colorModel;
    const BrickLink::Item *m_item = nullptr;
    bool m_hasLock = false;
    bool m_locked = false;
};
