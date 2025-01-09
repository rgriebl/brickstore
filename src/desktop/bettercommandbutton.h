// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QPushButton>
#include <QStaticText>


// Based on QCommandLinkButton, but this one scales with font size, supports richtext and can be
// associated with a QAction

class BetterCommandButton : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY(BetterCommandButton)

public:
    explicit BetterCommandButton(QAction *action, QWidget *parent = nullptr);
    explicit BetterCommandButton(QWidget *parent = nullptr);
    explicit BetterCommandButton(const QString &text, QWidget *parent = nullptr);
    explicit BetterCommandButton(const QString &text, const QString &description, QWidget *parent = nullptr);

    QString description() const;
    void setDescription(const QString &desc);

    QAction *action() const;
    void setAction(QAction *action);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;

protected:
    void changeEvent(QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *) override;

private:
    void resetTitleFont();
    void updateDescriptionText();
    void updateAction();

    int textOffset() const;
    int descriptionOffset() const;
    QRect descriptionRect() const;
    QRect titleRect() const;
    int descriptionHeight(int widgetWidth) const;

    QFont m_titleFont;
    QString m_descriptionText;
    QStaticText m_description;
    QAction *m_action = nullptr;
    int m_margin = 10;
};

