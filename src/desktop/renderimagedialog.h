// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QColor>
#include <QDialog>

#include "ui_renderimagedialog.h"


class RenderImageDialog : public QDialog, private Ui::RenderImageDialog
{
    Q_OBJECT

public:
    enum class Action { CopyToClipboard, SaveAs };

    RenderImageDialog(QWidget *parent = nullptr);
    ~RenderImageDialog() override;

    QSize imageSize() const;
    QColor background() const;
    qreal border() const; // percent of each edge to keep free
    Action action() const;

private:
    enum Background { Transparent, White, Black }; // same order as in the ui file

    Action m_action = Action::CopyToClipboard;
    bool m_updatingSize = false;
    qreal m_aspect = 1;
};
