/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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

#include <vector>
#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QVector>
#include <QAction>

#include <common/document.h>

QT_FORWARD_DECLARE_CLASS(QActionGroup)
QT_FORWARD_DECLARE_CLASS(QQuickAction)


class ActionManager : public QObject
{
    Q_OBJECT

public:
    enum Need {
        NoNeed = 0,

        NeedLotId        = 0x0001,
        NeedInventory    = 0x0002,
        NeedSubCondition = 0x0004,
        NeedQuantity     = 0x0008,

        NeedItemMask     = 0x000f,

        NeedDocument     = 0x0010,
        NeedLots         = 0x0040,
        NeedNetwork      = 0x0100,

        // the upper 16 bits (0xffff0000) are reserved for NeedSelection()
    };
    Q_DECLARE_FLAGS(Needs, Need)

    static inline Needs NeedSelection(quint8 minSel, quint8 maxSel = 0)
    {
        return static_cast<Needs>(NeedDocument | (quint32(minSel) << 24) | (quint32(maxSel) << 16));
    }

    enum Flag {
        NoFlag = 0,

        FlagCheckable   = 0x0001,
        FlagMenu        = 0x0002,
        FlagRecentFiles = 0x0004,
        FlagUndo        = 0x0008,
        FlagRedo        = 0x0010,

        // bits 24-31 (0xff000000) are reserved for FlagGroup()
        // bits 16-23 (0x00ff0000) are reserved for FlagRole()
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    static inline Flags FlagGroup(char groupId)
    {
        return static_cast<Flags>(quint32(groupId) << 24);
    }
    static inline Flags FlagRole(QAction::MenuRole role)
    {
        return static_cast<Flags>(quint32(role) << 16);
    }

    class Action
    {
    public:
        Action(const char *name, const char *text, const char *shortcut, Needs needs = NoNeed,
               Flags flags = NoFlag);
        Action(const char *name, const char *text, QKeySequence::StandardKey key,
               Needs needs = NoNeed, Flags flags = NoFlag);
        Action(const char *name, const char *text, Needs needs = NoNeed, Flags flags = NoFlag);

        inline const char *name() const    { return m_name; }
        inline bool hasText() const        { return m_text; }
        inline QString text() const        { return m_transText; }
        QString iconName() const;
        const QList<QKeySequence> shortcuts() const;
        inline const QList<QKeySequence> defaultShortcuts() const  { return m_defaultShortcuts; }
        inline ActionManager::Needs needs() const  { return m_needs; }
        inline bool isCheckable() const    { return m_flags & FlagCheckable; }
        inline bool isRecentFiles() const  { return m_flags & FlagRecentFiles; }
        inline bool isMenu() const         { return m_flags & FlagMenu; }
        inline bool isUndo() const         { return m_flags & FlagUndo; }
        inline bool isRedo() const         { return m_flags & FlagRedo; }
        inline QAction::MenuRole menuRole() const  { return QAction::MenuRole(quint32(m_flags) >> 16 & 0x000000ff); }
        inline char groupId() const        { return char(quint32(m_flags) >> 24); }
        inline QAction *qAction() const    { return m_qaction; }

        static bool lessThan(const Action &action, const char *name);

    private:
        Action(const char *name, const char *text, Needs needs, Flags flags,
               QKeySequence::StandardKey key, const char *shortcut);

        const char *m_name;
        const char *m_text;
        const char *m_shortcut = nullptr;
        QKeySequence::StandardKey m_standardKey = QKeySequence::UnknownKey;
        QList<QKeySequence> m_defaultShortcuts;
        ActionManager::Needs m_needs = ActionManager::NoNeed;
        int m_flags = NoFlag;
        const char *m_iconName = nullptr;
        QString m_transText;
        QString m_transShortcut;
        QKeySequence m_customShortcut;
        QList<QKeySequence> m_shortcuts;

        QAction *m_qaction = nullptr;
        QObject *m_qquickaction = nullptr;

        friend class ActionManager;
    };

    const Action *action(const char *name) const;
    QVector<const Action *> allActions() const;

    void setCustomShortcut(const char *name, const QKeySequence &shortcut);
    void setCustomShortcuts(const QMap<QByteArray, QKeySequence> &shortcuts);
    void saveCustomShortcuts() const;

    void retranslate();

    typedef QVector<QPair<QByteArray, std::function<void(bool)>>> ActionTable;

    QAction *qAction(const char *name);

    // These are templates so that instantiation happens only in calling code, when
    // QQuickAction can be expected to be fully defined.
    Q_INVOKABLE QObject *quickAction(const QString &name);

    bool createAll(std::function<QAction *(const ActionManager::Action *)> creator);

    QObject *connectActionTable(const ActionTable &actionTable);
    void disconnectActionTable(QObject *contextObject);

    void setSelection(const BrickLink::LotList &selection);

    Document *activeDocument() const;
    void setActiveDocument(Document *document);

    static ActionManager *inst();

signals:
    void activeDocumentChanged(Document *doc);

protected:
    bool event(QEvent *e) override;

private:
    ActionManager();
    void initialize();
    enum UpdateReason {
        OnlineChanged = 1,
        DocumentChanged = 2,
        SelectionChanged = 4,
        BlockingChanged = 8,
        ModificationChanged = 16,
    };
    void updateActions(int updateReason);
    void updateActionsBlockingChanged(); // can't be a lambda, because we need to disconnect
    void updateActionsModificationChanged();
    void setEnabled(const char *name, bool b);
    void setChecked(const char *name, bool b);
    void updateShortcuts(Action *aa);
    void loadCustomShortcuts();
    void setupQAction(Action &aa);

    std::vector<Action> m_actions;
    QMap<char, QActionGroup *> m_qactionGroups;

    static ActionManager *s_inst;

    Document *m_document = nullptr;
    LotList m_selection;
};



Q_DECLARE_OPERATORS_FOR_FLAGS(ActionManager::Needs)
Q_DECLARE_OPERATORS_FOR_FLAGS(ActionManager::Flags)
