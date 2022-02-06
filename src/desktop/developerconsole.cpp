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
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QKeyEvent>

#include "utility/utility.h"
#include "developerconsole.h"


DeveloperConsole::DeveloperConsole(QWidget *parent)
    : QWidget(parent)
{
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->installEventFilter(this);

    m_cmd = new QLineEdit(this);
    m_cmd->hide();
    m_cmd->installEventFilter(this);

    connect(m_cmd, &QLineEdit::returnPressed,
            this, [this]() {
        auto s = m_cmd->text();
        if (!s.isEmpty()) {
            bool successful = false;
            emit execute(s, &successful);
            if (successful) {
                if (!m_history.endsWith(s))
                    m_history.append(s);
            }
        }
        m_cmd->clear();
        m_historyIndex = m_history.size();
    });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_log);
    layout->addWidget(m_cmd);

    languageChange();
    fontChange();
}

bool DeveloperConsole::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(e);

        if ((o == m_log) && (ke->text() == m_consoleKey)) {
            activateConsole(!m_consoleActive);
        } else if (o == m_cmd) {
            int hd = 0;
            if (ke->key() == Qt::Key_Up)
                hd = -1;
            else if (ke->key() == Qt::Key_Down)
                hd = 1;
            if (hd) {
                if (((m_historyIndex + hd) >= 0) && ((m_historyIndex + hd) < m_history.size())) {
                    m_historyIndex += hd;
                    m_cmd->setText(m_history.at(m_historyIndex));
                }
            } else if (ke->key() == Qt::Key_Escape) {
                m_cmd->clear();
            } else if ((ke->text() == m_consoleKey) && m_cmd->text().isEmpty()) {
                m_cmd->clear();
                activateConsole(!m_consoleActive);
                return true;
            }
        }
    }
    return QWidget::eventFilter(o, e);
}

void DeveloperConsole::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    else if (e->type() == QEvent::FontChange)
        fontChange();
    QWidget::changeEvent(e);
}

void DeveloperConsole::activateConsole(bool active)
{
    if (active == m_consoleActive)
        return;
    m_consoleActive = active;
    if (m_consoleActive) {
        m_cmd->show();
        m_cmd->setFocus();
    } else {
        m_cmd->hide();
        m_log->setFocus();
    }
    auto tc = m_log->textCursor();
    if (!tc.atEnd()) {
        tc.movePosition(QTextCursor::End);
        m_log->setTextCursor(tc);
    }
    m_log->ensureCursorVisible();
}

void DeveloperConsole::languageChange()
{
    m_consoleKey = tr("`", "Hotkey for DevConsole, key below ESC");

    m_log->setToolTip(tr("Press %1 to activate the developer console").arg(m_consoleKey));
}

void DeveloperConsole::fontChange()
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSizeF(font().pointSizeF());
    m_log->setFont(f);
}


void DeveloperConsole::messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    static const char *msgTypeNames[] =   { "DBG ",   "WARN",   "CRIT",   "FATL",   "INFO" };
    static const char *msgTypeColor[] =   { "000000", "000000", "ff0000", "ffffff", "ffffff" };
    static const char *msgTypeBgColor[] = { "00ff00", "ffff00", "000000", "ff0000", "0000ff" };
    static const char *fileColor = "800080";
    static const char *lineColor = "ff00ff";
    static const char *categoryColor[] = { "e81717", "e8e817", "17e817", "17e8e8", "1717e8", "e817e8" };

    type = qBound(QtDebugMsg, type, QtInfoMsg);
    QString filename;
    if (ctx.file && ctx.file[0] && ctx.line > 1) {
        filename = QString::fromLocal8Bit(ctx.file);
        int pos = -1;
#if defined(Q_OS_WINDOWS)
        pos = filename.lastIndexOf('\\'_l1);
#endif
        if (pos < 0)
            pos = int(filename.lastIndexOf('/'_l1));
        filename = filename.mid(pos + 1);
    }

    QString str = "<pre>"_l1;
    const auto lines = msg.split('\n'_l1);
    for (int i = 0; i < lines.count(); ++i) {
        str = str % R"(<span style="color:#)"_l1 % QLatin1String(msgTypeColor[type])
                % R"(;background-color:#)"_l1 % QLatin1String(msgTypeBgColor[type]) % R"(;">)"_l1
                % QLatin1String(msgTypeNames[type]) % R"(</span>)"_l1
                % R"(&nbsp;<span style="color:#)"_l1
                % QLatin1String(categoryColor[qHashBits(ctx.category, qstrlen(ctx.category), 1) % 6])
                % R"(;font-weight:bold;">)"_l1
                % QLatin1String(ctx.category) % R"(</span>)"_l1 % ":&nbsp;"_l1
                % lines.at(i).toHtmlEscaped();
        if (i == (lines.count() - 1)) {
            if ((type != QtInfoMsg) && !filename.isEmpty()) {
                str = str % R"( at <span style="color:#)"_l1 % QLatin1String(fileColor)
                        % R"(;font-weight:bold;">)"_l1 % filename
                        % R"(</span>, line <span style="color:#)"_l1 % QLatin1String(lineColor)
                        % R"(;font-weight:bold;">)"_l1 % QString::number(ctx.line) % R"(</span></pre>)"_l1;
            } else {
                str = str % "</pre>"_l1;
            }
        } else {
            str = str % "&#x21a9;<br>"_l1;
        }
    }
    QMetaObject::invokeMethod(m_log, "appendHtml", Qt::QueuedConnection, Q_ARG(QString, str));
}

#include "moc_developerconsole.cpp"
