// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QComboBox>

QT_FORWARD_DECLARE_CLASS(QMenu)


class MenuComboBox : public QComboBox
{
    Q_OBJECT
public:
    MenuComboBox(QWidget *parent = nullptr);

    void showPopup() override;
    void hidePopup() override;

private:
    QMenu *m_menu = nullptr;
};

