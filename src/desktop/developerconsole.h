// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
