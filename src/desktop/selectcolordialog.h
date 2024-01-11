// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)
class SelectColor;


class SelectColorDialog : public QDialog
{
    Q_OBJECT
public:
    SelectColorDialog(bool popupMode, QWidget *parent = nullptr);
    ~SelectColorDialog() override;

    void setColor(const BrickLink::Color *color);
    void setColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item);
    const BrickLink::Color *color() const;

    void setPopupPosition(const QRect &pos = QRect());

protected:
    void moveEvent(QMoveEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *e) override;
    QSize sizeHint() const override;

private slots:
    void checkColor(const BrickLink::Color *, bool);

private:
    void setPopupGeometryChanged(bool b);
    bool isPopupGeometryChanged() const;

    bool m_popupMode = false;
    // only relevant when in popupMode and execAtPosition was called:
    QRect m_popupPos;
    QString m_geometryConfigKey;
    QAction *m_resetGeometryAction = nullptr;

    SelectColor *w_sc;
    QDialogButtonBox *w_buttons;
};
