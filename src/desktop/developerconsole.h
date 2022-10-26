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

#include <functional>

#include <QFrame>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QLabel)


class DeveloperConsole : public QFrame
{
    Q_OBJECT

public:
    DeveloperConsole(const QString &prompt, std::function<std::tuple<QString, bool>(QString)> executeFunction,
                     QWidget *parent = nullptr);

    void setPrompt(const QString &prompt);

    void appendLogMessage(QtMsgType type, const QString &category, const QString &file,
                          int line, const QString &msg);

protected:
    void changeEvent(QEvent *e) override;

private:
    void fontChange();

    QPlainTextEdit *m_log;
    QLineEdit *m_cmd;
    QLabel *m_prompt;

    QStringList m_history;
    int m_historyIndex = 0;
    QString m_current;

    std::function<std::tuple<QString, bool>(QString)> m_executeFunction;
};
