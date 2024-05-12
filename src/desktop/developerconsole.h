// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>

#include <QDialog>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QLabel)


class DeveloperConsole : public QDialog
{
    Q_OBJECT

public:
    DeveloperConsole(const QString &prompt, const std::function<std::tuple<QString, bool>(QString)> &executeFunction,
                     QWidget *parent = nullptr);
    ~DeveloperConsole() override;

    void setPrompt(const QString &prompt);

    void appendLogMessage(QtMsgType type, const QString &category, const QString &file,
                          int line, const QString &msg);

protected:
    void changeEvent(QEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

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
