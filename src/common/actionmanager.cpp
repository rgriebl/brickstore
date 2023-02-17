// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QAction>
#include <QActionGroup>
#if defined(BS_MOBILE)
#  if defined(Q_CC_MSVC)
#    pragma warning(push)
#    pragma warning(disable: 4458)
#    pragma warning(disable: 4201)
#  endif
#  include <QtQuickTemplates2/private/qquickaction_p.h>
#  if defined(Q_CC_MSVC)
#    pragma warning(pop)
#  endif
#  include <QJSValue>
#  include <QJSValueIterator>
#  include <QQmlEngine>
#elif defined(BS_DESKTOP)
#  include <QApplication>
#  include <QPalette>
QT_BEGIN_NAMESPACE
class QQuickAction : public QObject { }; // clazy:exclude=missing-qobject-macro
QT_END_NAMESPACE
#endif
#include <QDebug>

#include "utility/utility.h"
#include "actionmanager.h"
#include "config.h"
#include "document.h"
#include "onlinestate.h"


ActionManager::Action::Action(const char *name, const char *text, Needs needs, Flags flags,
                              QKeySequence::StandardKey key, const char *shortcut)
    : m_name(name)
    , m_text(text)
    , m_shortcut(shortcut)
    , m_needs(needs)
    , m_flags(int(flags))
{
    if (text)
        m_transText = QString::fromLatin1(text);
    if (shortcut)
        m_transShortcut = QString::fromLatin1(shortcut);
    if (key != QKeySequence::UnknownKey)
        m_defaultShortcuts = QKeySequence::keyBindings(key);
    else if (shortcut)
        m_defaultShortcuts = QKeySequence::listFromString(QString::fromLatin1(shortcut));
    m_shortcuts = m_defaultShortcuts;

#if defined(BS_DESKTOP)
    Q_UNUSED(m_qquickaction)
#endif

}

ActionManager::Action::Action(const char *name, const char *text, const char *shortcut, Needs needs,
                              Flags flags)
    : Action(name, text, needs, flags, QKeySequence::UnknownKey, shortcut)
{ }

ActionManager::Action::Action(const char *name, const char *text, QKeySequence::StandardKey key,
                              Needs needs, Flags flags)
    : Action(name, text, needs, flags, key, nullptr)
{ }

ActionManager::Action::Action(const char *name, const char *text, Needs needs, Flags flags)
    : Action(name, text, needs, flags, QKeySequence::UnknownKey, nullptr)
{ }

QString ActionManager::Action::iconName() const
{
    auto iconName = QString::fromLatin1(m_iconName ? m_iconName : m_name);
    return iconName.replace(ushort('_'), ushort('-'));
}

const QList<QKeySequence> ActionManager::Action::shortcuts() const
{
    if (!m_customShortcut.isEmpty())
        return { m_customShortcut };
    else
        return m_shortcuts;
}



ActionManager *ActionManager::s_inst = nullptr;

ActionManager *ActionManager::inst()
{
    if (!s_inst) {
        s_inst = new ActionManager();
        s_inst->initialize();
    }
    return s_inst;
}

ActionManager *ActionManager::create(QQmlEngine *, QJSEngine *)
{
    auto am = inst();
    QQmlEngine::setObjectOwnership(am, QQmlEngine::CppOwnership);
    return am;
}

Document *ActionManager::activeDocument() const
{
    return m_document;
}

void ActionManager::setActiveDocument(Document *document)
{
    if (m_document == document)
        return;

    if (m_document) {
        m_document->setActive(false);

        disconnect(m_document->model(), &DocumentModel::modificationChanged,
                   this, &ActionManager::updateActionsModificationChanged);
        disconnect(m_document, &Document::blockingOperationActiveChanged,
                   this, &ActionManager::updateActionsBlockingChanged);
    }
    m_document = document;
    if (m_document) {
        connect(m_document, &Document::blockingOperationActiveChanged,
                this, &ActionManager::updateActionsBlockingChanged);
        connect(m_document->model(), &DocumentModel::modificationChanged,
                this, &ActionManager::updateActionsModificationChanged);

        m_document->setActive(true);
    }
    setSelection(document ? document->selectedLots() : LotList { });
    updateActions(DocumentChanged);

    emit activeDocumentChanged(m_document);
}

ActionManager::ActionManager()
{
    connect(OnlineState::inst(), &OnlineState::onlineStateChanged,
            this, [this]() {
        updateActions(OnlineChanged);
    });
    connect(Config::inst(), &Config::shortcutsChanged,
            this, &ActionManager::loadCustomShortcuts);
}

void ActionManager::setSelection(const LotList &selection)
{
    m_selection = selection;
    updateActions(SelectionChanged);
}

bool ActionManager::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        return true;
    }
    return QObject::event(e);
}

void ActionManager::updateActions(int updateReason)
{
    if (updateReason & SelectionChanged) {
        const auto firstLot = !m_selection.isEmpty() ? m_selection.constFirst() : nullptr;

        int status     = firstLot ? int(firstLot->status())       : -1;
        int condition  = firstLot ? int(firstLot->condition())    : -1;
        int scondition = firstLot ? int(firstLot->subCondition()) : -1;
        int retain     = firstLot ? (firstLot->retain() ? 1 : 0)  : -1;
        int stockroom  = firstLot ? int(firstLot->stockroom())    : -1;

        if (firstLot) {
            for (const Lot *item : std::as_const(m_selection)) {
                if ((status >= 0) && (status != int(item->status())))
                    status = -1;
                if ((condition >= 0) && (condition != int(item->condition())))
                    condition = -1;
                if ((scondition >= 0) && (scondition != int(item->subCondition())))
                    scondition = -1;
                if ((retain >= 0) && (retain != (item->retain() ? 1 : 0)))
                    retain = -1;
                if ((stockroom >= 0) && (stockroom != int(item->stockroom())))
                    stockroom = -1;
            }
        }
        setChecked("edit_status_include", status == int(BrickLink::Status::Include));
        setChecked("edit_status_exclude", status == int(BrickLink::Status::Exclude));
        setChecked("edit_status_extra", status == int(BrickLink::Status::Extra));

        setChecked("edit_cond_new", condition == int(BrickLink::Condition::New));
        setChecked("edit_cond_used", condition == int(BrickLink::Condition::Used));

        setChecked("edit_subcond_none", scondition == int(BrickLink::SubCondition::None));
        setChecked("edit_subcond_sealed", scondition == int(BrickLink::SubCondition::Sealed));
        setChecked("edit_subcond_complete", scondition == int(BrickLink::SubCondition::Complete));
        setChecked("edit_subcond_incomplete", scondition == int(BrickLink::SubCondition::Incomplete));

        setChecked("edit_retain_yes", retain == 1);
        setChecked("edit_retain_no", retain == 0);

        setChecked("edit_stockroom_no", stockroom == int(BrickLink::Stockroom::None));
        setChecked("edit_stockroom_a", stockroom == int(BrickLink::Stockroom::A));
        setChecked("edit_stockroom_b", stockroom == int(BrickLink::Stockroom::B));
        setChecked("edit_stockroom_c", stockroom == int(BrickLink::Stockroom::C));
    }

    bool isOnline = OnlineState::inst()->isOnline();
    int docItemCount = m_document ? int(m_document->model()->lotCount()) : 0;

    for (auto &a : m_actions) {
        if (!a.m_needs)
            continue;

        bool b = true;

        if (m_document && m_document->isBlockingOperationActive()
                && (a.m_needs & (NeedDocument | NeedLots | NeedItemMask))) {
            b = false;
        }

        if (a.m_needs & NeedNetwork)
            b = b && isOnline;

        if (a.m_needs & NeedDocument) {
            b = b && m_document;

            if (b) {
                if (a.m_needs & NeedLots)
                    b = b && (docItemCount > 0);

                quint8 minSelection = (a.m_needs >> 24) & 0xff;
                quint8 maxSelection = (a.m_needs >> 16) & 0xff;

                if (minSelection)
                    b = b && (m_selection.count() >= minSelection);
                if (maxSelection)
                    b = b && (m_selection.count() <= maxSelection);
            }
        }

        if (a.m_needs & NeedItemMask) {
            for (const Lot *item : std::as_const(m_selection)) {
                if (a.m_needs & NeedLotId)
                    b = b && (item->lotId() != 0);
                if (a.m_needs & NeedInventory)
                    b = b && (item->item() && item->item()->hasInventory());
                if (a.m_needs & NeedQuantity)
                    b = b && (item->quantity() != 0);
                if (a.m_needs & NeedSubCondition)
                    b = b && (item->itemType() && item->itemType()->hasSubConditions());

                if (!b)
                    break;
            }
        }
        if (a.m_qaction) {
            a.m_qaction->setEnabled(b);
        }
    }

    bool canBeSaved = m_document ? m_document->model()->canBeSaved() : false;
    bool blocked = m_document ? m_document->isBlockingOperationActive() : false;
    bool hasNoFileName = m_document && m_document->filePath().isEmpty();

    setEnabled("document_save", (canBeSaved || hasNoFileName) && !blocked);
}

void ActionManager::updateActionsBlockingChanged()
{
    updateActions(BlockingChanged);
}

void ActionManager::updateActionsModificationChanged()
{
    updateActions(ModificationChanged);
}

void ActionManager::setEnabled(const char *name, bool b)
{
    if (auto qa = qAction(name))
        qa->setEnabled(b);
}

void ActionManager::setChecked(const char *name, bool b)
{
    if (auto qa = qAction(name))
        qa->setChecked(b);
}

void ActionManager::updateShortcuts(Action *aa)
{
#if defined(BS_DESKTOP)
    if (aa->m_qaction) {
        aa->m_qaction->setShortcuts(aa->m_shortcuts);
        if (!aa->m_shortcuts.isEmpty())
            aa->m_qaction->setToolTip(toolTipLabel(aa->m_transText, aa->m_shortcuts));
        else
            aa->m_qaction->setToolTip(aa->m_transText);
    }
#else
    Q_UNUSED(aa)
#endif
}

void ActionManager::loadCustomShortcuts()
{
    const auto savedShortcuts = Config::inst()->shortcuts();
    for (auto it = savedShortcuts.cbegin(); it != savedShortcuts.cend(); ++it) {
        QByteArray name = it.key().toLatin1();
        if (auto *a = const_cast<Action *>(action(name.constData()))) {
            QKeySequence customShortcut = it.value().value<QKeySequence>();
            a->m_customShortcut = customShortcut;
            a->m_shortcuts = { customShortcut };
            updateShortcuts(a);
        }
    }
}


// QT_TR_NOOP doesn't support the disambiguate parameter, which we need (e.g. New), but
// lupdate actually accepts the second comment/disambiguate parameter...
#undef QT_TR_NOOP
#define QT_TR_NOOP(x, ...) x
#define A(x, ...)  a = &m_actions.emplace_back(x, __VA_ARGS__)

void ActionManager::initialize()
{
    Action *a = nullptr;

    A("go_home",                        QT_TR_NOOP("Go to the Quickstart page"), QT_TR_NOOP("HomePage", "Go to the homepage"), NoNeed, FlagCheckable);

    A("menu_file",                      QT_TR_NOOP("&File"), NoNeed, FlagMenu);
    A("document_new",                   QT_TR_NOOP("New", "File|New"), QKeySequence::New);
    A("document_open",                  QT_TR_NOOP("Open..."),         QKeySequence::Open);
    A("document_open_recent",           QT_TR_NOOP("Open Recent"), NoNeed, FlagMenu | FlagRecentFiles);
    A("document_save",                  QT_TR_NOOP("Save"),            QKeySequence::Save,   NeedDocument);
    A("document_save_as",               QT_TR_NOOP("Save As..."),      QKeySequence::SaveAs, NeedDocument);
    A("document_save_all",              QT_TR_NOOP("Save All"));
    A("document_print",                 QT_TR_NOOP("Print..."),        QKeySequence::Print,  NeedDocument);
    A("document_print_pdf",             QT_TR_NOOP("Print to PDF..."),                       NeedDocument);
    A("document_close",                 QT_TR_NOOP("Close"),           QKeySequence::Close,  NeedDocument);
    A("application_exit",               QT_TR_NOOP("Exit"),            QKeySequence::Quit, NoNeed, FlagRole(QAction::QuitRole));
    A("document_import",                QT_TR_NOOP("Import"), NoNeed, FlagMenu);
    A("document_import_bl_inv",         QT_TR_NOOP("BrickLink Set Inventory..."),                 QT_TR_NOOP("Ctrl+I,Ctrl+I", "File|Import BrickLink Set Inventory"));
    a->m_iconName = "brick-1x1";
    A("document_import_bl_xml",         QT_TR_NOOP("BrickLink XML..."),                           QT_TR_NOOP("Ctrl+I,Ctrl+X", "File|Import BrickLink XML"));
    a->m_iconName = "dialog-xml-editor";
    A("document_import_bl_order",       QT_TR_NOOP("BrickLink Order..."),                         QT_TR_NOOP("Ctrl+I,Ctrl+O", "File|Import BrickLink Order"),           NeedNetwork);
    a->m_iconName = "view-financial-list";
    A("document_import_bl_store_inv",   QT_TR_NOOP("BrickLink Store Inventory..."),               QT_TR_NOOP("Ctrl+I,Ctrl+S", "File|Import BrickLink Store Inventory"), NeedNetwork);
    a->m_iconName = "bricklink-store";
    A("document_import_bl_cart",        QT_TR_NOOP("BrickLink Shopping Cart..."),                 QT_TR_NOOP("Ctrl+I,Ctrl+C", "File|Import BrickLink Shopping Cart"),   NeedNetwork);
    a->m_iconName = "bricklink-cart";
    A("document_import_bl_wanted",      QT_TR_NOOP("BrickLink Wanted List..."),                   QT_TR_NOOP("Ctrl+I,Ctrl+W", "File|Import BrickLink Wanted List"),     NeedNetwork);
    a->m_iconName = "love-amarok";
    A("document_import_ldraw_model",    QT_TR_NOOP("LDraw or Studio Model..."),                   QT_TR_NOOP("Ctrl+I,Ctrl+L", "File|Import LDraw Model"));
    a->m_iconName = "bricklink-studio";
    A("document_export",                QT_TR_NOOP("Export"), NoNeed, FlagMenu);
    A("document_export_bl_xml",         QT_TR_NOOP("BrickLink XML..."),                           QT_TR_NOOP("Ctrl+E,Ctrl+X", "File|Import BrickLink XML"),             NeedDocument | NeedLots);
    A("document_export_bl_xml_clip",    QT_TR_NOOP("BrickLink Mass-Upload XML to Clipboard"),     QT_TR_NOOP("Ctrl+E,Ctrl+U", "File|Import BrickLink Mass-Upload"),     NeedDocument | NeedLots);
    A("document_export_bl_update_clip", QT_TR_NOOP("BrickLink Mass-Update XML to Clipboard"),     QT_TR_NOOP("Ctrl+E,Ctrl+P", "File|Import BrickLink Mass-Update"),     NeedDocument | NeedLots);
    A("document_export_bl_invreq_clip", QT_TR_NOOP("BrickLink Set Inventory XML to Clipboard"),   QT_TR_NOOP("Ctrl+E,Ctrl+I", "File|Import BrickLink Set Inventory"),   NeedDocument | NeedLots);
    A("document_export_bl_wantedlist_clip", QT_TR_NOOP("BrickLink Wanted List XML to Clipboard"), QT_TR_NOOP("Ctrl+E,Ctrl+W", "File|Import BrickLink Wanted List"),     NeedDocument | NeedLots);

    A("menu_edit",                      QT_TR_NOOP("&Edit"),                                                                 NoNeed, FlagMenu);
    A("edit_undo",                      nullptr,                            QKeySequence::Undo,                              NoNeed, FlagUndo);
    A("edit_redo",                      nullptr,                            QKeySequence::Redo,                              NoNeed, FlagRedo);
    A("edit_cut",                       QT_TR_NOOP("Cut"),                  QKeySequence::Cut,                               NeedSelection(1));
    A("edit_copy",                      QT_TR_NOOP("Copy"),                 QKeySequence::Copy,                              NeedSelection(1));
    A("edit_paste",                     QT_TR_NOOP("Paste"),                QKeySequence::Paste,                             NeedDocument);
    A("edit_paste_silent",              QT_TR_NOOP("Paste Silent"),         QT_TR_NOOP("Ctrl+Shift+V", "Edit|Paste Silent"), NeedDocument);
    A("edit_duplicate",                 QT_TR_NOOP("Duplicate"),            QT_TR_NOOP("Ctrl+D", "Edit|Duplicate"),          NeedSelection(1));
    A("edit_delete",                    QT_TR_NOOP("Delete"),               QT_TR_NOOP("Del", "Edit|Delete"),                NeedSelection(1));
    // ^^ QKeySequence::Delete has Ctrl+D as secondary on macOS and Linux and this clashes with Duplicate
    A("edit_additems",                  QT_TR_NOOP("Add Items..."),         QT_TR_NOOP("Insert", "Edit|AddItems"),           NeedDocument);
    A("edit_subtractitems",             QT_TR_NOOP("Subtract Items..."),                                                     NeedDocument);
    A("edit_mergeitems",                QT_TR_NOOP("Consolidate Items..."), QT_TR_NOOP("Ctrl+L", "Edit|Consolidate Items"),  NeedSelection(2));
    A("edit_partoutitems",              QT_TR_NOOP("Part out Item..."),                                                      NeedInventory | NeedSelection(1) | NeedQuantity);
    A("edit_copy_fields",               QT_TR_NOOP("Copy Values from Document..."),                                          NeedDocument | NeedLots);
    A("edit_select_all",                QT_TR_NOOP("Select All"),           QKeySequence::SelectAll,                         NeedDocument | NeedLots);
    A("edit_select_none",               QT_TR_NOOP("Select None"),          QT_TR_NOOP("Ctrl+Shift+A", "Edit|Select None"),  NeedDocument | NeedLots);
    // ^^ QKeySequence::Deselect is only mapped on Linux
    A("edit_filter_from_selection",     QT_TR_NOOP("Create a Filter from the Selection"), NeedSelection(1, 1));

    A("menu_view",                      QT_TR_NOOP("&View"), NoNeed, FlagMenu);
    A("view_toolbar",                   QT_TR_NOOP("View Toolbar"),                                                                   NoNeed, FlagCheckable);
    A("view_docks",                     QT_TR_NOOP("View Info Docks"), NoNeed, FlagMenu);
    A("view_fullscreen",                QT_TR_NOOP("Full Screen"),                       QKeySequence::FullScreen,                    NoNeed, FlagCheckable);
    A("view_row_height_inc",            QT_TR_NOOP("Increase row height"),                                                            NeedDocument);
    A("view_row_height_dec",            QT_TR_NOOP("Decrease row height"),                                                            NeedDocument);
    A("view_row_height_reset",          QT_TR_NOOP("Reset row height"),                                                               NeedDocument);
    A("view_show_input_errors",         QT_TR_NOOP("Show Input Errors"),                                                              NoNeed, FlagCheckable);
    A("view_goto_next_input_error",     QT_TR_NOOP("Go to the Next Error"),              QT_TR_NOOP("F6", "View|Go Next Error"),      NeedDocument | NeedLots);
    a->m_iconName = "emblem-warning";
    A("view_show_diff_indicators",      QT_TR_NOOP("Show Difference Mode Indicators"),                                                NoNeed, FlagCheckable);
    A("view_reset_diff_mode",           QT_TR_NOOP("Reset Difference Mode Base Values"),                                              NeedSelection(1));
    A("view_goto_next_diff",            QT_TR_NOOP("Go to the Next Difference"),         QT_TR_NOOP("F5", "View|Go Next Difference"), NeedDocument | NeedLots);
    a->m_iconName = "vcs-locally-modified";
    A("view_filter",                    QT_TR_NOOP("Filter the Item List"),              QKeySequence::Find,                          NeedDocument);
    A("view_column_layout_save",        QT_TR_NOOP("Save Column Layout..."),                                                          NeedDocument);
    A("view_column_layout_manage",      QT_TR_NOOP("Manage Column Layouts..."));
    A("view_column_layout_load",        QT_TR_NOOP("Load Column Layout"), NeedDocument, FlagMenu);
    a->m_iconName = "object-columns";

    A("menu_extras",              QT_TR_NOOP("E&xtras"), NoNeed, FlagMenu);
    A("update_database",          QT_TR_NOOP("Update Database"),                 NeedNetwork);
    a->m_iconName = "view_refresh";
    A("configure",                QT_TR_NOOP("Settings..."),                     QT_TR_NOOP("Ctrl+,", "ExQT_TR_NOOPas|Settings"), NoNeed, FlagRole(QAction::PreferencesRole));
    A("reload_scripts",           QT_TR_NOOP("Reload User Scripts"));
    A("menu_window",              QT_TR_NOOP("&Windows"), NoNeed, FlagMenu);

    A("menu_help",                QT_TR_NOOP("&Help"), NoNeed, FlagMenu);
    A("help_about",               QT_TR_NOOP("About..."), NoNeed, FlagRole(QAction::AboutRole));
    A("help_systeminfo",          QT_TR_NOOP("System Information..."));
    A("help_announcements",       QT_TR_NOOP("Important Announcements..."));
    A("help_releasenotes",        QT_TR_NOOP("Release notes..."));
    A("help_reportbug",           QT_TR_NOOP("Report a bug..."));
    A("help_extensions",          QT_TR_NOOP("Extensions Interface Documentation..."));
    A("check_for_updates",        QT_TR_NOOP("Check for Program Updates..."),    NeedNetwork, FlagRole(QAction::ApplicationSpecificRole));
    a->m_iconName = "update_none";

    A("edit_status",              QT_TR_NOOP("Status"),                          NeedSelection(1), FlagMenu);
    A("edit_status_include",      QT_TR_NOOP("Set status to Include"),           NeedSelection(1), FlagCheckable | FlagGroup('S'));
    a->m_iconName = "vcs-normal";
    A("edit_status_exclude",      QT_TR_NOOP("Set status to Exclude"),           NeedSelection(1), FlagCheckable | FlagGroup('S'));
    a->m_iconName = "vcs-removed";
    A("edit_status_extra",        QT_TR_NOOP("Set status to Extra"),             NeedSelection(1), FlagCheckable | FlagGroup('S'));
    a->m_iconName = "vcs-added";
    A("edit_status_toggle",       QT_TR_NOOP("Toggle status between Include and Exclude"), NeedSelection(1));
    A("edit_cond",                QT_TR_NOOP("Condition"),                       NeedSelection(1), FlagMenu);
    A("edit_cond_new",            QT_TR_NOOP("Set condition to New"),            NeedSelection(1), FlagCheckable | FlagGroup('C'));
    A("edit_cond_used",           QT_TR_NOOP("Set condition to Used"),           NeedSelection(1), FlagCheckable | FlagGroup('C'));
    A("edit_subcond_none",        QT_TR_NOOP("Set sub-condition to None"),       NeedSelection(1) | NeedSubCondition, FlagCheckable | FlagGroup('U'));
    A("edit_subcond_sealed",      QT_TR_NOOP("Set sub-condition to Sealed"),     NeedSelection(1) | NeedSubCondition, FlagCheckable | FlagGroup('U'));
    A("edit_subcond_complete",    QT_TR_NOOP("Set sub-condition to Complete"),   NeedSelection(1) | NeedSubCondition, FlagCheckable | FlagGroup('U'));
    A("edit_subcond_incomplete",  QT_TR_NOOP("Set sub-condition to Incomplete"), NeedSelection(1) | NeedSubCondition, FlagCheckable | FlagGroup('U'));
    A("edit_item",                QT_TR_NOOP("Set item..."),                     NeedSelection(1));
    A("edit_color",               QT_TR_NOOP("Set color..."),                    NeedSelection(1));
    a->m_iconName = "color_management";
    A("edit_qty",                 QT_TR_NOOP("Quantity"),                        NeedSelection(1), FlagMenu);
    A("edit_qty_set",             QT_TR_NOOP("Set quantity..."));
    A("edit_qty_multiply",        QT_TR_NOOP("Multiply quantity..."),            QT_TR_NOOP("Ctrl+*", "Edit|Quantity|Multiply"),       NeedSelection(1));
    A("edit_qty_divide",          QT_TR_NOOP("Divide quantity..."),              QT_TR_NOOP("Ctrl+/", "Edit|Quantity|Divide"),         NeedSelection(1));
    A("edit_price",               QT_TR_NOOP("Price"),                           NeedSelection(1), FlagMenu);
    A("edit_price_round",         QT_TR_NOOP("Round price to 2 decimals"),       NeedSelection(1));
    A("edit_price_set",           QT_TR_NOOP("Set price..."),                    NeedSelection(1));
    A("edit_price_to_priceguide", QT_TR_NOOP("Set price to guide..."),           QT_TR_NOOP("Ctrl+G", "Edit|Price|Set to PriceGuide"), NeedSelection(1));
    A("edit_price_inc_dec",       QT_TR_NOOP("Adjust price..."),                 QT_TR_NOOP("Ctrl++", "Edit|Price|Inc/Dec"),           NeedSelection(1));
    A("edit_cost",                QT_TR_NOOP("Cost"),                            NeedSelection(1), FlagMenu);
    A("edit_cost_round",          QT_TR_NOOP("Round cost to 2 decimals"),        NeedSelection(1));
    A("edit_cost_set",            QT_TR_NOOP("Set cost..."),                     NeedSelection(1));
    A("edit_cost_inc_dec",        QT_TR_NOOP("Adjust cost..."),                  NeedSelection(1));
    A("edit_cost_spread_price",   QT_TR_NOOP("Spread cost relative to price..."),  NeedSelection(2));
    A("edit_cost_spread_weight",  QT_TR_NOOP("Spread cost relative to weight..."), NeedSelection(2));
    A("edit_bulk",                QT_TR_NOOP("Set bulk quantity..."),            NeedSelection(1));
    A("edit_sale",                QT_TR_NOOP("Set sale..."),                     QT_TR_NOOP("Ctrl+%", "Edit|Sale"),                    NeedSelection(1));
    A("edit_comment",             QT_TR_NOOP("Comment"),                         NeedSelection(1), FlagMenu);
    A("edit_comment_set",         QT_TR_NOOP("Set comment..."),                  NeedSelection(1));
    A("edit_comment_add",         QT_TR_NOOP("Add to comment..."),               NeedSelection(1));
    A("edit_comment_rem",         QT_TR_NOOP("Remove from comment..."),          NeedSelection(1));
    A("edit_comment_clear",       QT_TR_NOOP("Clear comment"),                   NeedSelection(1));
    A("edit_remark",              QT_TR_NOOP("Remark"),                          NeedSelection(1), FlagMenu);
    A("edit_remark_set",          QT_TR_NOOP("Set remark..."),                   NeedSelection(1));
    A("edit_remark_add",          QT_TR_NOOP("Add to remark..."),                NeedSelection(1));
    A("edit_remark_rem",          QT_TR_NOOP("Remove from remark..."),           NeedSelection(1));
    A("edit_remark_clear",        QT_TR_NOOP("Clear remark"),                    NeedSelection(1));
    A("edit_retain",              QT_TR_NOOP("Retain in Inventory"),             NeedSelection(1), FlagMenu);
    A("edit_retain_yes",          QT_TR_NOOP("Set retain flag"),                 NeedSelection(1), FlagCheckable | FlagGroup('R'));
    A("edit_retain_no",           QT_TR_NOOP("Unset retain flag"),               NeedSelection(1), FlagCheckable | FlagGroup('R'));
    A("edit_retain_toggle",       QT_TR_NOOP("Toggle retain flag"),              NeedSelection(1));
    A("edit_stockroom",           QT_TR_NOOP("Stockroom Item"),                  NeedSelection(1), FlagMenu);
    A("edit_stockroom_no",        QT_TR_NOOP("Set stockroom to None"),           NeedSelection(1), FlagCheckable | FlagGroup('T'));
    A("edit_stockroom_a",         QT_TR_NOOP("Set stockroom to A"),              NeedSelection(1), FlagCheckable | FlagGroup('T'));
    A("edit_stockroom_b",         QT_TR_NOOP("Set stockroom to B"),              NeedSelection(1), FlagCheckable | FlagGroup('T'));
    A("edit_stockroom_c",         QT_TR_NOOP("Set stockroom to C"),              NeedSelection(1), FlagCheckable | FlagGroup('T'));
    A("edit_reserved",            QT_TR_NOOP("Set reserved for..."),             NeedSelection(1));
    A("edit_marker",              QT_TR_NOOP("Marker"),                          NeedSelection(1), FlagMenu);
    A("edit_marker_text",         QT_TR_NOOP("Set marker text"),                 NeedSelection(1));
    A("edit_marker_color",        QT_TR_NOOP("Set marker color..."),             NeedSelection(1));
    A("edit_marker_clear",        QT_TR_NOOP("Clear marker"),                    NeedSelection(1));
    A("edit_lotid_copy",          QT_TR_NOOP("Copy lot id"),                     NeedSelection(1));
    A("edit_lotid_clear",         QT_TR_NOOP("Clear lot id"),                    NeedSelection(1));

    A("bricklink_catalog",        QT_TR_NOOP("Show BrickLink Catalog Info..."),     QT_TR_NOOP("Ctrl+B,Ctrl+C", "Edit|Show BL Catalog Info"),  NeedSelection(1, 1) | NeedNetwork);
    A("bricklink_priceguide",     QT_TR_NOOP("Show BrickLink Price Guide Info..."), QT_TR_NOOP("Ctrl+B,Ctrl+P", "Edit|Show BL Price Guide"),   NeedSelection(1, 1) | NeedNetwork);
    A("bricklink_lotsforsale",    QT_TR_NOOP("Show Lots for Sale on BrickLink..."), QT_TR_NOOP("Ctrl+B,Ctrl+L", "Edit|Show BL Lots for Sale"), NeedSelection(1, 1) | NeedNetwork);
    A("bricklink_myinventory",    QT_TR_NOOP("Show in my Store on BrickLink..."),   QT_TR_NOOP("Ctrl+B,Ctrl+I", "Edit|Show BL my Inventory"),  NeedSelection(1, 1) | NeedNetwork);

    std::sort(m_actions.begin(), m_actions.end());

    loadCustomShortcuts();
}

#undef A
#undef QT_TR_NOOP
#define QT_TR_NOOP(x) x


const ActionManager::Action *ActionManager::action(const char *name) const
{
    if (!name || !*name)
        return nullptr;

    auto it = std::lower_bound(m_actions.cbegin(), m_actions.cend(), name);
    if ((it != m_actions.cend()) && (*it == name))
        return &(*it);
    return nullptr;
}

QVector<const ActionManager::Action *> ActionManager::allActions() const
{
    QVector<const Action *> all;
    all.reserve(int(m_actions.size()));
    for (const auto &a : m_actions)
        all.append(&a);
    return all;
}

void ActionManager::setCustomShortcut(const char *name, const QKeySequence &shortcut)
{
    if (auto *a = const_cast<Action *>(action(name))) {
        a->m_customShortcut = shortcut;
        if ((a->m_shortcuts.size() != 1) || (a->m_shortcuts.constFirst() != shortcut)) {
            a->m_shortcuts = { shortcut };
            updateShortcuts(a);
        }
    }
}

void ActionManager::setCustomShortcuts(const QMap<QByteArray, QKeySequence> &shortcuts)
{
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it)
        setCustomShortcut(it.key().constData(), it.value());
}

void ActionManager::saveCustomShortcuts() const
{
    QVariantMap savedShortcuts;
    for (const auto &a : m_actions) {
        if (!a.m_customShortcut.isEmpty()) {
            savedShortcuts.insert(QString::fromLatin1(a.m_name),
                                  QVariant::fromValue(a.m_customShortcut));
        }
    }
    Config::inst()->setShortcuts(savedShortcuts);
}

void ActionManager::retranslate()
{
    for (auto &a : m_actions) {
        if (a.m_text) {
            const auto newText = tr(a.m_text);
            if (newText != a.m_transText) {
                a.m_transText = newText;
                if (a.m_qaction)
                    a.m_qaction->setText(newText);
            }
        }
        if (a.m_shortcut) {
            const auto newShortcut = tr(a.m_shortcut);
            if (newShortcut != a.m_transShortcut) {
                a.m_transShortcut = newShortcut;
                a.m_defaultShortcuts = QKeySequence::listFromString(a.m_transShortcut);

                if (a.m_customShortcut.isEmpty()) {
                    a.m_shortcuts = a.m_defaultShortcuts;                    
                    updateShortcuts(&a);
                }
            }
        }
    }
}

QAction *ActionManager::qAction(const char *name)
{
    auto aa = const_cast<Action *>(action(name));
    return aa ? aa->m_qaction : nullptr;
}

void ActionManager::setupQAction(Action &aa)
{
    Q_ASSERT(aa.m_qaction);
    QAction *a = aa.m_qaction;

    a->setObjectName(QLatin1String(aa.name()));
    a->setProperty("bsAction", true);

    if (QIcon::hasThemeIcon(aa.iconName()))
        a->setIcon(QIcon::fromTheme(aa.iconName()));
    if (aa.needs())
        a->setProperty("bsFlags", int(aa.needs()));
    const auto text = aa.text();
    auto shortcuts = aa.shortcuts();
    if (aa.hasText())
        a->setText(text);

#if defined(BS_DESKTOP)
    a->setShortcuts(shortcuts);
    if (!shortcuts.isEmpty())
        a->setToolTip(toolTipLabel(a->text(), shortcuts));
    else
        a->setToolTip(a->text());

    connect(a, &QAction::changed, this, [a, &aa]() {
        if (a->text() != aa.m_transText) {
            if (!a->shortcuts().isEmpty())
                a->setToolTip(toolTipLabel(a->text(), a->shortcuts()));
            else
                a->setToolTip(a->text());
        }
    });
#endif
    a->setCheckable(aa.isCheckable());

    QAction::MenuRole role = aa.menuRole();
    if (role != QAction::NoRole)
        a->setMenuRole(role);

    if (char group = aa.groupId()) {
        auto ag = m_qactionGroups.value(group);
        if (!ag) {
            ag = new QActionGroup(this);
            m_qactionGroups.insert(group, ag);
        }
        a->setActionGroup(ag);
    }
}

QQuickAction *ActionManager::quickAction(const QString &name)
{
#if defined(BS_MOBILE)
    if (auto a = const_cast<Action *>(action(name.toLatin1().constData()))) {
        if (QAction *qAction = a->qAction()) {
            auto quickAction = new QQuickAction(qAction);
            quickAction->setObjectName(qAction->objectName());
            quickAction->setText(qAction->text());
            quickAction->setCheckable(qAction->isCheckable());
            quickAction->setEnabled(qAction->isEnabled());

            QQuickIcon qi = quickAction->icon();
            bool hasIcon = !qAction->icon().isNull();

            if (!qAction->isCheckable() || hasIcon) {
                qi.setName(hasIcon ? a->iconName() : u"dummy"_qs);
                qi.setColor(Qt::transparent);
                quickAction->setIcon(qi);
            }

            connect(quickAction, &QQuickAction::triggered, qAction, &QAction::trigger);
            connect(quickAction, &QQuickAction::enabledChanged, qAction, &QAction::setEnabled);
            connect(qAction, &QAction::enabledChanged, quickAction, &QQuickAction::setEnabled);
            connect(quickAction, &QQuickAction::checkableChanged, qAction, &QAction::setCheckable);
            connect(qAction, &QAction::checkableChanged, quickAction, &QQuickAction::setCheckable);
            connect(quickAction, &QQuickAction::checkedChanged, qAction, &QAction::setChecked);
            connect(qAction, &QAction::toggled, quickAction, &QQuickAction::setChecked);
            connect(qAction, &QAction::changed, quickAction, [qAction, quickAction]() { quickAction->setText(qAction->text()); });

            a->m_qquickaction = quickAction;
        }
        return qobject_cast<QQuickAction *>(a->m_qquickaction);
    }
#else
    Q_UNUSED(name)
#endif
    return nullptr;
}

bool ActionManager::createAll(std::function<QAction*(const ActionManager::Action *)> creator)
{
    if (!creator)
        return false;

    for (auto &aa : m_actions) {
        QAction *qa = creator(&aa);
        if (qa) {
            delete aa.m_qaction;
            qa->setParent(this);
            aa.m_qaction = qa;
            setupQAction(aa);
        }
    }
    return true;
}

QObject *ActionManager::connectActionTable(const ActionTable &actionTable)
{
    QObject *contextObject = new QObject(this);

    for (auto &at : std::as_const(actionTable)) {
        if (QAction *a = qAction(at.first))
            QObject::connect(a, &QAction::triggered, contextObject, at.second);
    }
    return contextObject;
}

void ActionManager::disconnectActionTable(QObject *contextObject)
{
    delete contextObject;
}

QObject *ActionManager::connectQuickActionTable(const QJSValue &nameToCallable)
{
    QObject *contextObject = new QObject(this);

#if defined(BS_MOBILE)
    QJSValueIterator it(nameToCallable);
    while (it.hasNext()) {
        it.next();

        QJSValue target = it.value();
        if (!target.isCallable()) {
            qWarning() << "connectQuickActionTable: connect" << it.name()
                       << "tries to connect to non-callable";
            continue;
        }

        if (QAction *a = qAction(it.name().toLatin1().constData())) {
            QObject::connect(a, &QAction::triggered, contextObject, [=](bool b) {
                target.call({ b });
            });
        }
    }
    QQmlEngine::setObjectOwnership(contextObject, QQmlEngine::CppOwnership);
#else
    Q_UNUSED(nameToCallable)
#endif
    return contextObject;
}

void ActionManager::disconnectQuickActionTable(QObject *connectionContext)
{
    delete connectionContext;
}

QString ActionManager::toolTipLabel(const QString &label, QKeySequence shortcut, const QString &extended)
{
    return toolTipLabel(label, shortcut.isEmpty() ? QList<QKeySequence> { }
                                                  : QList<QKeySequence> { shortcut }, extended);
}

QString ActionManager::toolTipLabel(const QString &label, const QList<QKeySequence> &shortcuts, const QString &extended)
{
#if defined(BS_DESKTOP)

    static const auto fmt = uR"(<table><tr style="white-space: nowrap;"><td>%1</td><td align="right" valign="middle"><span style="color: %2; font-size: small;">&nbsp; &nbsp;%3</span></td></tr>%4</table>)"_qs;
    static const auto fmtExt = uR"(<tr><td colspan="2">%1</td></tr>)"_qs;

    QColor color = Utility::gradientColor(Utility::premultiplyAlpha(QApplication::palette("QLabel").color(QPalette::Inactive, QPalette::ToolTipBase)),
                                          Utility::premultiplyAlpha(QApplication::palette("QLabel").color(QPalette::Inactive, QPalette::ToolTipText)),
                                          0.8f);
    QString extendedTable;
    if (!extended.isEmpty())
        extendedTable = fmtExt.arg(extended);
    QString shortcutText;
    if (!shortcuts.isEmpty())
        shortcutText = QKeySequence::listToString(shortcuts, QKeySequence::NativeText);

    return fmt.arg(label, color.name(), shortcutText, extendedTable);
#else
    Q_UNUSED(label)
    Q_UNUSED(shortcuts)
    Q_UNUSED(extended)
    return { };
#endif
}

#include "moc_actionmanager.cpp"
