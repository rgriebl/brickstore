/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

#include <QWidget>
#include <QPointer>

QT_FORWARD_DECLARE_CLASS(QGroupBox);
QT_FORWARD_DECLARE_CLASS(QLabel);

class WelcomeButton;


class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    WelcomeWidget(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *e) override;

private:
    void updateLastDBUpdateDescription();

    void languageChange();

private:
    QGroupBox *m_recent_frame;
    QGroupBox *m_file_frame;
    QGroupBox *m_import_frame;
    QGroupBox *m_update_frame;
    WelcomeButton *m_db_update;
    WelcomeButton *m_bs_update;
    QPointer<QLabel> m_no_recent;
};


