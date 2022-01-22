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

#include <QDialog>
#include "bricklink/global.h"
#include "ui_selectcolordialog.h"


class SelectColorDialog : public QDialog, private Ui::SelectColorDialog
{
    Q_OBJECT
public:
    SelectColorDialog(bool popupMode, QWidget *parent = nullptr);
    ~SelectColorDialog() override;

    void setColor(const BrickLink::Color *color);
    void setColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item);
    const BrickLink::Color *color() const;

    int execAtPosition(const QRect &pos = QRect());

protected:
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    void changeEvent(QEvent *e) override;
#elif defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)
    bool event(QEvent *e) override;
#endif
    void moveEvent(QMoveEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *e) override;
    QSize sizeHint() const override;

private slots:
    void checkColor(const BrickLink::Color *, bool);

private:
    bool m_popupMode = false;
    // only relevant when in popupMode and execAtPosition was called:
    QRect m_popupPos;
    bool m_geometryChanged = false;
    QString m_geometryConfigKey;
};
