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

QT_FORWARD_DECLARE_CLASS(QSlider)
QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QAbstractButton)

namespace Ui {
class RenderSettingsDialog;
}

namespace LDraw {
class RenderSettings;
}

class RenderSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    static RenderSettingsDialog *inst();
    ~RenderSettingsDialog() override;

private:
    explicit RenderSettingsDialog();
    void connectToggleButton(QAbstractButton *checkBox, const QByteArray &propName);
    void connectSliderAndSpinBox(QSlider *slider, QDoubleSpinBox *spinBox, const QByteArray &propName,
                                 int factor = 100);

    Ui::RenderSettingsDialog *ui;
    static RenderSettingsDialog *s_inst;
};
