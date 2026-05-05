// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)
class SelectItem;

class SelectItemDialog : public QDialog
{
    Q_OBJECT
public:
    SelectItemDialog(bool popupMode, QWidget *parent = nullptr);
    ~SelectItemDialog() override;

    void setItemType(const BrickLink::ItemType *);
    void setItem(const BrickLink::Item *);
    const BrickLink::Item *item() const;

    void setPopupPosition(const QRect &pos = QRect());

protected:
    void moveEvent(QMoveEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *e) override;
    QSize sizeHint() const override;

private slots:
    void checkItem(const BrickLink::Item *, bool);

private:
    void setPopupGeometryChanged(bool b);
    bool isPopupGeometryChanged() const;

    bool m_popupMode = false;
    // only relevant when in popupMode and execAtPosition was called:
    QRect m_popupPos;
    QString m_geometryConfigKey;
    QAction *m_resetGeometryAction = nullptr;

    QDialogButtonBox *w_buttons;
    SelectItem *w_si;
};
