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

#include <QPushButton>
#include <QStaticText>


// Based on QCommandLinkButton, but this one scales with font size, supports richtext and can be
// associated with a QAction

class BetterCommandButton : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY(BetterCommandButton)
    Q_PROPERTY(QString description READ description WRITE setDescription)

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

