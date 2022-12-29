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
#include <QSignalMapper>

#include "ldraw/rendersettings.h"
#include "rendersettingsdialog.h"
#include "ui_rendersettingsdialog.h"


RenderSettingsDialog *RenderSettingsDialog::s_inst = nullptr;


RenderSettingsDialog::RenderSettingsDialog()
    : QDialog(nullptr, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint)
    , ui(new Ui::RenderSettingsDialog)
{
    ui->setupUi(this);

    connectSliderAndSpinBox(ui->animationAngleSlider, ui->animationAngle, "tumblingAnimationAngle");
    connectSliderAndSpinBox(ui->lineWidthSlider,      ui->lineWidth,      "lineThickness", 10);
    connectSliderAndSpinBox(ui->fieldOfViewSlider,    ui->fieldOfView,    "fieldOfView", 1);

    connectSliderAndSpinBox(ui->brightnessSlider, ui->brightness, "additionalLight");
    connectSliderAndSpinBox(ui->aoStrengthSlider, ui->aoStrength, "aoStrength");
    connectSliderAndSpinBox(ui->aoSoftnessSlider, ui->aoSoftness, "aoSoftness");
    connectSliderAndSpinBox(ui->aoDistanceSlider, ui->aoDistance, "aoDistance");

    connectSliderAndSpinBox(ui->plainMetalnessSlider,    ui->plainMetalness,    "plainMetalness");
    connectSliderAndSpinBox(ui->plainRoughnessSlider,    ui->plainRoughness,    "plainRoughness");
    connectSliderAndSpinBox(ui->chromeMetalnessSlider,   ui->chromeMetalness,   "chromeMetalness");
    connectSliderAndSpinBox(ui->chromeRoughnessSlider,   ui->chromeRoughness,   "chromeRoughness");
    connectSliderAndSpinBox(ui->metallicMetalnessSlider, ui->metallicMetalness, "metallicMetalness");
    connectSliderAndSpinBox(ui->metallicRoughnessSlider, ui->metallicRoughness, "metallicRoughness");
    connectSliderAndSpinBox(ui->pearlMetalnessSlider,    ui->pearlMetalness,    "pearlMetalness");
    connectSliderAndSpinBox(ui->pearlRoughnessSlider,    ui->pearlRoughness,    "pearlRoughness");

    connectToggleButton(ui->orthographicCamera,  "orthographicCamera");
    connectToggleButton(ui->showLines,           "renderLines");
    connectToggleButton(ui->showBoundingSpheres, "showBoundingSpheres");
    connectToggleButton(ui->enableLighting,      "lighting");

    connectComboBox(ui->antiAliasing,  "antiAliasing");

    connect(ui->buttonBox, &QDialogButtonBox::clicked,
            this, [this](QAbstractButton *button) {
        switch (ui->buttonBox->standardButton(button)) {
        case QDialogButtonBox::Save:
            LDraw::RenderSettings::inst()->save();
            break;
        case QDialogButtonBox::RestoreDefaults:
            LDraw::RenderSettings::inst()->resetToDefaults();
            break;
        default:
            break;
        }
    });

    resize(minimumSizeHint());
}

RenderSettingsDialog *RenderSettingsDialog::inst()
{
    if (!s_inst)
        s_inst = new RenderSettingsDialog();
    return s_inst;
}

RenderSettingsDialog::~RenderSettingsDialog()
{
    delete ui;
}

void RenderSettingsDialog::connectToggleButton(QAbstractButton *checkBox, const QByteArray &propName)
{
    auto rs = LDraw::RenderSettings::inst();

    connect(checkBox, &QCheckBox::toggled,
            rs, [=](bool b) {
        rs->setProperty(propName, b);
    });

    auto setter = [=]() {
        checkBox->setChecked(rs->property(propName).toBool());
    };

    auto dummyMapper = new QSignalMapper(checkBox);
    QByteArray chgSig = "2" + propName + "Changed(bool)";
    connect(rs, chgSig.constData(), dummyMapper, SLOT(map()));
    dummyMapper->setMapping(rs, 42);
    connect(dummyMapper, &QSignalMapper::mappedInt, this, setter);

    setter();
}

void RenderSettingsDialog::connectComboBox(QComboBox *comboBox, const QByteArray &propName)
{
    auto rs = LDraw::RenderSettings::inst();

    connect(comboBox, &QComboBox::currentIndexChanged,
            rs, [=](int i) {
        rs->setProperty(propName, i);
    });

    auto setter = [=]() {
        comboBox->setCurrentIndex(rs->property(propName).toInt());
    };

    auto dummyMapper = new QSignalMapper(comboBox);
    QByteArray chgSig = "2" + propName + "Changed(int)";
    connect(rs, chgSig.constData(), dummyMapper, SLOT(map()));
    dummyMapper->setMapping(rs, 42);
    connect(dummyMapper, &QSignalMapper::mappedInt, this, setter);

    setter();
}

void RenderSettingsDialog::connectSliderAndSpinBox(QSlider *slider, QDoubleSpinBox *spinBox,
                                                   const QByteArray &propName, int factor)
{
    auto rs = LDraw::RenderSettings::inst();

    connect(slider, &QSlider::valueChanged,
            spinBox, [=](int v) {
        double dv = double(v) / factor;
        if (!qFuzzyCompare(dv, spinBox->value()))
            spinBox->setValue(dv);
    });
    connect(spinBox, &QDoubleSpinBox::valueChanged,
            slider, [=](double v) {
        int iv = qRound(v * factor);
        if (iv != slider->value())
            slider->setValue(iv);

        rs->setProperty(propName, v);
    });

    auto setter = [=]() {
        spinBox->setValue(rs->property(propName).toDouble());
    };

    auto dummyMapper = new QSignalMapper(spinBox);
    QByteArray chgSig = "2" + propName + "Changed(float)";
    connect(rs, chgSig.constData(), dummyMapper, SLOT(map()));
    dummyMapper->setMapping(rs, 42);
    connect(dummyMapper, &QSignalMapper::mappedInt, this, setter);

    setter();
}
