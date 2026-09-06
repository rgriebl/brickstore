// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPushButton>

#include "common/config.h"
#include "renderimagedialog.h"

static const QString configBase = u"MainWindow/RenderImageDialog/"_qs;


RenderImageDialog::RenderImageDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_lockAspect->setProperty("iconScaling", true);
    w_lockAspect->setToolTip(tr("Lock the aspect ratio"));

    w_buttons->button(QDialogButtonBox::Ok)->setText(tr("Copy"));

    connect(w_buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {
        switch (w_buttons->standardButton(button)) {
        case QDialogButtonBox::Ok  : m_action = Action::CopyToClipboard; break;
        case QDialogButtonBox::Save: m_action = Action::SaveAs; break;
        default                    : break;
        }
    });

    w_width->setValue(Config::inst()->value(configBase + u"Width", 640).toInt());
    w_height->setValue(Config::inst()->value(configBase + u"Height", 480).toInt());
    w_lockAspect->setChecked(Config::inst()->value(configBase + u"LockAspect", true).toBool());
    w_border->setValue(Config::inst()->value(configBase + u"Border", 2).toInt());
    w_borderPercent->setValue(w_border->value());

    const int background = Config::inst()->value(configBase + u"Background",
                                                 int(Transparent)).toInt();
    w_background->setCurrentIndex((background >= 0) && (background < w_background->count())
                                  ? background : int(Transparent));

    m_aspect = qreal(w_width->value()) / w_height->value();

    auto setLock = [this](bool locked) {
        if (locked)
            m_aspect = qreal(w_width->value()) / w_height->value();
        w_lockAspect->setIcon(QIcon::fromTheme(locked ? u"folder-locked"_qs
                                                      : u"folder-unlocked"_qs));
    };

    connect(w_lockAspect, &QToolButton::toggled, this, setLock);
    setLock(w_lockAspect->isChecked());

    // the two spin boxes drive each other while locked, so the guard is not optional: the
    // ratio does not round-trip exactly for anything but a whole-number aspect
    connect(w_width, &QSpinBox::valueChanged, this, [this](int width) {
        if (!w_lockAspect->isChecked() || m_updatingSize)
            return;
        m_updatingSize = true;
        w_height->setValue(qRound(width / m_aspect));
        m_updatingSize = false;
    });
    connect(w_height, &QSpinBox::valueChanged, this, [this](int height) {
        if (!w_lockAspect->isChecked() || m_updatingSize)
            return;
        m_updatingSize = true;
        w_width->setValue(qRound(height * m_aspect));
        m_updatingSize = false;
    });

    connect(w_border, &QSlider::valueChanged, w_borderPercent, &QSpinBox::setValue);
    connect(w_borderPercent, &QSpinBox::valueChanged, w_border, &QSlider::setValue);

    resize(minimumSizeHint());
}

RenderImageDialog::~RenderImageDialog()
{
    Config::inst()->setValue(configBase + u"Width", w_width->value());
    Config::inst()->setValue(configBase + u"Height", w_height->value());
    Config::inst()->setValue(configBase + u"LockAspect", w_lockAspect->isChecked());
    Config::inst()->setValue(configBase + u"Border", w_border->value());
    Config::inst()->setValue(configBase + u"Background", w_background->currentIndex());
}

QSize RenderImageDialog::imageSize() const
{
    return { w_width->value(), w_height->value() };
}

QColor RenderImageDialog::background() const
{
    switch (w_background->currentIndex()) {
    case White: return { Qt::white };
    case Black: return { Qt::black };
    default   : return { Qt::transparent };
    }
}

qreal RenderImageDialog::border() const
{
    return w_border->value();
}

RenderImageDialog::Action RenderImageDialog::action() const
{
    return m_action;
}

#include "moc_renderimagedialog.cpp"
