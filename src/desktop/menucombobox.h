// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QComboBox>
#include <QElapsedTimer>

QT_FORWARD_DECLARE_CLASS(QMenu)


class MenuComboBox : public QComboBox
{
    Q_OBJECT
public:
    MenuComboBox(QWidget *parent = nullptr);

    void showPopup() override;
    void hidePopup() override;

protected:
    void mousePressEvent(QMouseEvent *e) override;

    // Since 6.10.2, QMenu triggers its active action on a release that had no preceding
    // press on the menu (QTBUG-124920) - which is exactly the release that opened our
    // popup, as showPopup() puts the active action right under the cursor. Swallow that
    // one release, like QComboBox does for its own popup.
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    QMenu *m_menu = nullptr;
    QElapsedTimer m_popupTimer;
    QPoint m_pressPos;
    bool m_blockRelease = false;
};

