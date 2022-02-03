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

#include <QWidget>
#include <QMutex>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QTimer)


class DeveloperConsole : public QWidget
{
    Q_OBJECT

public:
    DeveloperConsole(QWidget *parent = nullptr);

    void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);

signals:
    void execute(const QString &command, bool *successful);

protected:
    void changeEvent(QEvent *e) override;

private:
    void activateConsole(bool b);
    void languageChange();
    void fontChange();

    QPlainTextEdit *m_log;
    QLineEdit *m_cmd;
    QStringList m_history;
    QString m_consoleKey;
    int m_historyIndex = 0;
    bool m_consoleActive = false;
    QStringList m_messages;
    QMutex m_messagesMutex;
    QTimer *m_messagesTimer = nullptr;
};
