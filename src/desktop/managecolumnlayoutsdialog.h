// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)


class ManageColumnLayoutsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ManageColumnLayoutsDialog(QWidget *parent);

    void retranslateUi();

    void accept() override;

protected:
    void changeEvent(QEvent *e) override;

private:
    QLabel *          m_label;
    QListWidget *     m_list;
    QDialogButtonBox *m_buttons;
};

