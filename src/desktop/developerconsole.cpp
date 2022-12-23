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
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QScrollBar>
#include <QGridLayout>
#include <QTextBlock>
#include <QKeyEvent>
#include <QApplication>

#include "common/eventfilter.h"
#include "developerconsole.h"


DeveloperConsole::DeveloperConsole(const QString &prompt,
                                   std::function<std::tuple<QString, bool>(QString)> executeFunction,
                                   QWidget *parent)
    : QFrame(parent)
    , m_log(new QPlainTextEdit(this))
    , m_cmd(new QLineEdit(this))
    , m_prompt(new QLabel(this))
    , m_executeFunction(executeFunction)
{
    setFrameStyle(QFrame::StyledPanel);

    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setFrameStyle(QFrame::NoFrame);

    m_cmd->setFrame(false);
    m_cmd->setPlaceholderText(u"JS console, type \"help\" for help, \u2191/\u2193 for history"_qs);

    m_prompt->setBackgroundRole(QPalette::Base);
    m_prompt->setAutoFillBackground(true);
    m_prompt->setIndent(int(m_log->document()->documentMargin()));
    setPrompt(prompt);

    new EventFilter(m_cmd, { QEvent::KeyPress }, [this](QObject *, QEvent *e) {
        auto *ke = static_cast<QKeyEvent *>(e);

        int hd = (ke->key() == Qt::Key_Up) ? -1 : ((ke->key() == Qt::Key_Down) ? 1 : 0);
        if (hd) {
            if (((m_historyIndex + hd) >= 0) && ((m_historyIndex + hd) <= m_history.size())) {
                if (m_historyIndex == m_history.size())
                    m_current = m_cmd->text();
                m_historyIndex += hd;
                m_cmd->setText((m_historyIndex == m_history.size())
                               ? m_current : m_history.at(m_historyIndex));
            }
            return EventFilter::StopEventProcessing;
        } else if (ke->key() == Qt::Key_Escape) {
            m_cmd->clear();
            return EventFilter::StopEventProcessing;
        } else {
            return EventFilter::ContinueEventProcessing;
        }
    });

    connect(m_cmd, &QLineEdit::returnPressed,
            this, [this]() {
        auto cmd = m_cmd->text();
        if (!cmd.isEmpty() && m_executeFunction) {
            m_log->appendPlainText(m_prompt->text() + cmd);
            auto [message, succeeded] = m_executeFunction(cmd);
            if (succeeded) {
                m_log->appendPlainText(message);
                m_history.removeAll(cmd);
                m_history.append(cmd);
            } else {
                m_log->appendPlainText(u"ERROR: " + message);
            }
            m_log->moveCursor(QTextCursor::End);
        }
        m_cmd->clear();
        m_historyIndex = int(m_history.size());
        m_log->ensureCursorVisible();
    });

    auto *layout = new QGridLayout(this);
    layout->setColumnStretch(1, 10);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setVerticalSpacing(0);
    layout->setHorizontalSpacing(0);
    layout->addWidget(m_log, 0, 0, 1, 2);
    layout->addWidget(m_prompt, 1, 0);
    layout->addWidget(m_cmd, 1, 1);

    fontChange();
}

void DeveloperConsole::setPrompt(const QString &prompt)
{
    if (m_prompt->text() != prompt)
        m_prompt->setText(prompt);
}

void DeveloperConsole::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange)
        fontChange();
    QWidget::changeEvent(e);
}

void DeveloperConsole::fontChange()
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSizeF(qApp->font().pointSizeF());
    m_log->document()->setDefaultFont(f);
    //m_cmd->setCursorWidth(QFontMetrics(f).averageCharWidth());
    m_cmd->setFont(f);
    m_prompt->setFont(f);
}

void DeveloperConsole::appendLogMessage(QtMsgType type, const QString &category,
                                        const QString &file, int line, const QString &msg)
{
    static const char *msgTypeNames[] =   { "DBG ",   "WARN",   "CRIT",   "FATL",   "INFO" };
    static const char *msgTypeColor[] =   { "000000", "000000", "ff0000", "ffffff", "ffffff" };
    static const char *msgTypeBgColor[] = { "00ff00", "ffff00", "000000", "ff0000", "0000ff" };
    static const char *fileColor = "800080";
    static const char *lineColor = "ff00ff";
    static const char *categoryColor[] = { "e81717", "e8e817", "17e817", "17e8e8", "1717e8", "e817e8" };

    type = std::clamp(QtDebugMsg, type, QtInfoMsg);
    QString filename;
    if (!file.isEmpty() && line > 1) {
        filename = file;
        int pos = -1;
#if defined(Q_OS_WINDOWS)
        pos = filename.lastIndexOf(u'\\');
#endif
        if (pos < 0)
            pos = int(filename.lastIndexOf(u'/'));
        filename = filename.mid(pos + 1);
    }

    int catIndex = (category.isEmpty() ? 0 : category.at(0).unicode()) % 6;

    QString str = u"<pre>"_qs;
    const auto lines = msg.split(u"\n"_qs);
    for (int i = 0; i < lines.count(); ++i) {
        str = str + uR"(<span style="color:#)" + QLatin1String(msgTypeColor[type])
                + uR"(;background-color:#)" + QLatin1String(msgTypeBgColor[type]) + uR"(;">)"
                + QLatin1String(msgTypeNames[type]) + uR"(</span>)"
                + uR"(&nbsp;<span style="color:#)"
                + QLatin1String(categoryColor[catIndex])
                + uR"(;font-weight:bold;">)" + category + uR"(</span>)" + u":&nbsp;"
                + lines.at(i).toHtmlEscaped();
        if (i == (lines.count() - 1)) {
            if ((type != QtInfoMsg) && !filename.isEmpty()) {
                str = str + uR"( at <span style="color:#)" + QLatin1String(fileColor)
                        + uR"(;font-weight:bold;">)" + filename
                        + uR"(</span>, line <span style="color:#)" + QLatin1String(lineColor)
                        + uR"(;font-weight:bold;">)" + QString::number(line) + uR"(</span></pre>)";
            } else {
                str = str + u"</pre>";
            }
        } else {
            str = str + u"<br>";
        }
    }

    // only scroll to bottom, if we were at the bottom to begin with
    auto *sb = m_log->verticalScrollBar();
    bool atBottom = (sb->value() == sb->maximum());

    m_log->appendHtml(str);

    if (atBottom)
        m_log->ensureCursorVisible();
}

#include "moc_developerconsole.cpp"
