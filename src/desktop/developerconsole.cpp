// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QScrollBar>
#include <QGridLayout>
#include <QTextBlock>
#include <QKeyEvent>
#include <QApplication>
#include <QSplitter>
#include <QTreeView>
#include <QTableView>
#include <QHeaderView>

#include "common/config.h"
#include "common/eventfilter.h"
#include "utility/appstatistics.h"
#include "developerconsole.h"


DeveloperConsole::DeveloperConsole(const QString &prompt,
                                   const std::function<std::tuple<QString, bool>(QString)> &executeFunction,
                                   QWidget *parent)
    : QDialog(parent)
    , m_log(new QPlainTextEdit)
    , m_cmd(new QLineEdit)
    , m_prompt(new QLabel)
    , m_executeFunction(executeFunction)
{
    m_history = Config::inst()->value(u"MainWindow/DeveloperConsole/History"_qs).toStringList();
    m_historyIndex = int(m_history.size());

    setWindowTitle(u"Developer Console"_qs);
    setSizeGripEnabled(true);

    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setFrameStyle(QFrame::NoFrame);
    m_log->setTextInteractionFlags(Qt::TextBrowserInteraction);

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
            m_log->setCurrentCharFormat({ });
            m_log->appendPlainText(m_prompt->text() + cmd);
            auto [message, succeeded] = m_executeFunction(cmd);
            if (succeeded) {
                m_log->appendPlainText(message);
            } else {
                m_log->appendHtml(uR"(<span style="color:#ff0000">ERROR</span>: <i>)"
                                  + message + uR"(</i>)");
            }
            m_history.removeAll(cmd);
            m_history.append(cmd);
            m_log->moveCursor(QTextCursor::End);
        }
        m_cmd->clear();
        m_historyIndex = int(m_history.size());
        m_log->ensureCursorVisible();
    });

    auto split = new QSplitter(Qt::Horizontal);
    auto *splitLayout = new QVBoxLayout(this);
    splitLayout->addWidget(split);
    splitLayout->setContentsMargins(0, 0, 0, 0);

    auto *console = new QFrame();
    console->setFrameStyle(QFrame::StyledPanel);

    auto *layout = new QGridLayout(console);
    layout->setColumnStretch(1, 10);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setVerticalSpacing(0);
    layout->setHorizontalSpacing(0);
    layout->addWidget(m_log, 0, 0, 1, 2);
    layout->addWidget(m_prompt, 1, 0);
    layout->addWidget(m_cmd, 1, 1);
    split->addWidget(console);

    auto *stats = new QTreeView();
    stats->header()->setSectionsMovable(false);
    stats->setRootIsDecorated(false);
    stats->setAlternatingRowColors(true);
    stats->setModel(AppStatistics::inst());

    stats->header()->setStretchLastSection(false);
    stats->header()->setMinimumSectionSize(20);
    stats->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    stats->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    split->addWidget(stats);
    split->setSizes({ 7000, 3000 });

    fontChange();

    m_cmd->setFocus();
    setFocusProxy(m_cmd);
}

DeveloperConsole::~DeveloperConsole()
{
    Config::inst()->setValue(u"MainWindow/DeveloperConsole/History"_qs, m_history);
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

void DeveloperConsole::showEvent(QShowEvent *e)
{
    auto ba = Config::inst()->value(u"MainWindow/DeveloperConsole/Geometry"_qs).toByteArray();
    if (!ba.isEmpty())
        restoreGeometry(ba);
    QDialog::showEvent(e);
}

void DeveloperConsole::closeEvent(QCloseEvent *e)
{
    Config::inst()->setValue(u"MainWindow/DeveloperConsole/Geometry"_qs, saveGeometry());
    QDialog::closeEvent(e);
}

void DeveloperConsole::fontChange()
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSizeF(qApp->font().pointSizeF());
    m_log->document()->setDefaultFont(f);
    m_log->setMinimumWidth(QFontMetrics(f).averageCharWidth() * 80 + 4);
    m_log->setMinimumHeight(QFontMetrics(f).height() * 10 + 4);
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

    type = std::clamp(type, QtDebugMsg, QtInfoMsg);
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
        str = str + uR"(<span style="color:#)" + QString::fromLatin1(msgTypeColor[type])
                + uR"(;background-color:#)" + QString::fromLatin1(msgTypeBgColor[type]) + uR"(;">)"
                + QString::fromLatin1(msgTypeNames[type]) + uR"(</span>)"
                + uR"(&nbsp;<span style="color:#)"
                + QString::fromLatin1(categoryColor[catIndex])
                + uR"(;font-weight:bold;">)" + category + uR"(</span>)" + u":&nbsp;"
                + lines.at(i).toHtmlEscaped();
        if (i == (lines.count() - 1)) {
            if ((type != QtInfoMsg) && !filename.isEmpty()) {
                str = str + uR"( at <span style="color:#)" + QString::fromLatin1(fileColor)
                        + uR"(;font-weight:bold;">)" + filename
                        + uR"(</span>, line <span style="color:#)" + QString::fromLatin1(lineColor)
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
