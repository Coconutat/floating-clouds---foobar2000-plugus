#include "stdafx.h"
#include "config.h"
#include "localization.h"
#include "organizer.h"

// ============================================================================
// Menu entry points: a main-menu item and a playlist context-menu item.
// Both invoke the same core (organizer::organize_active_playlist).
// ============================================================================

namespace {

// --- Component GUIDs (unique to this component) ---
static const GUID guid_menu_group    = { 0x7c3d2b1a, 0x4e5f, 0x6071, { 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8, 0x21 } };
static const GUID guid_cmd_organize  = { 0x7c3d2b1a, 0x4e5f, 0x6071, { 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8, 0x22 } };
static const GUID guid_cm_organize   = { 0x7c3d2b1a, 0x4e5f, 0x6071, { 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8, 0x23 } };
static const GUID guid_cmd_sort      = { 0x7c3d2b1a, 0x4e5f, 0x6071, { 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8, 0x24 } };

// Run the organize operation and show a localized summary popup.
void run_organize()
{
    OrganizeResult result;
    pfc::string8 error;
    if (!organizer::organize_active_playlist(result, error)) {
        pfc::string_formatter msg;
        msg << tr8("Organize failed: ", "整理失败：") << error;
        popup_message::g_show(msg, tr8("Playlist Organizer", "歌单整理器"));
        return;
    }
    pfc::string_formatter msg;
    msg << tr8("Updated ", "更新了 ")
        << result.playlists_updated
        << tr8(" playlist(s), ", " 个歌单，共 ")
        << result.tracks_organized
        << tr8(" track(s).", " 首曲目。");
    popup_message::g_show(msg, tr8("Playlist Organizer", "歌单整理器"));
}

// Sort the whole playlist list A-Z (existing playlists included).
void run_sort_all()
{
    pfc::string8 error;
    if (!organizer::sort_all_playlists(error)) {
        pfc::string_formatter msg;
        msg << tr8("Sort failed: ", "排序失败：") << error;
        popup_message::g_show(msg, tr8("Playlist Organizer", "歌单整理器"));
        return;
    }
    popup_message::g_show(tr8("Playlists sorted A-Z.", "歌单已按 A-Z 排序。"),
                          tr8("Playlist Organizer", "歌单整理器"));
}

// ---------------------------------------------------------------------------
// Main menu item: File > Playlist > Playlist Organizer > Organize active playlist
// ---------------------------------------------------------------------------
static mainmenu_group_popup_factory g_mainmenu_group(
    guid_menu_group, mainmenu_groups::file_playlist,
    mainmenu_commands::sort_priority_dontcare, "Playlist Organizer");

class mainmenu_commands_impl : public mainmenu_commands {
public:
    enum { cmd_organize = 0, cmd_sort = 1 };
    t_uint32 get_command_count() override { return 2; }
    GUID get_command(t_uint32 p_index) override {
        switch (p_index) {
            case cmd_organize: return guid_cmd_organize;
            case cmd_sort: return guid_cmd_sort;
            default: uBugCheck();
        }
    }
    void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
        switch (p_index) {
            case cmd_organize: p_out = tr8("Organize Active Playlist by Album Artist", "按专辑艺人整理活动歌单"); break;
            case cmd_sort: p_out = tr8("Sort all playlists A-Z", "所有歌单按 A-Z 排序"); break;
            default: uBugCheck();
        }
    }
    bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
        switch (p_index) {
            case cmd_organize:
                p_out = tr8("Organizes the active playlist into per-album-artist playlists, with Soundtrack / Various Artists / Unknown Artist handling.",
                            "把活动歌单按专辑艺人整理为多个歌单（含 Soundtrack / Various Artists / Unknown Artist 归类）。");
                return true;
            case cmd_sort:
                p_out = tr8("Sorts every playlist by name (A-Z).", "把所有歌单按名称 A-Z 排序。");
                return true;
            default: return false;
        }
    }
    GUID get_parent() override { return guid_menu_group; }
    void execute(t_uint32 p_index, service_ptr_t<service_base>) override {
        switch (p_index) {
            case cmd_organize: run_organize(); break;
            case cmd_sort: run_sort_all(); break;
            default: uBugCheck();
        }
    }
};
static mainmenu_commands_factory_t<mainmenu_commands_impl> g_mainmenu_factory;

// ---------------------------------------------------------------------------
// Playlist context-menu item: right-click a playlist tab or inside a playlist.
// contextmenu_item_simple only shows when track data is present, so we derive
// from contextmenu_item_v2 and render for the playlist callers with empty data.
// ---------------------------------------------------------------------------
class playlist_context_menu : public contextmenu_item_v2 {
public:
    unsigned get_num_items() override { return 1; }
    GUID get_item_guid(unsigned) override { return guid_cm_organize; }
    void get_item_name(unsigned, pfc::string_base& p_out) override {
        p_out = tr8("Organize this playlist by album artist", "按专辑艺人整理此歌单");
    }
    bool get_item_description(unsigned, pfc::string_base& p_out) override {
        p_out = tr8("Organizes the playlist into per-album-artist playlists.",
                    "把该歌单按专辑艺人整理为多个歌单。");
        return true;
    }
    t_enabled_state get_enabled_state(unsigned) override { return DEFAULT_ON; }

    contextmenu_item_node_root* instantiate_item(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) override {
        class node : public contextmenu_item_node_root_leaf {
        public:
            node(playlist_context_menu* owner, unsigned idx) : m_owner(owner), m_idx(idx) {}
            bool get_display_data(pfc::string_base& p_out, unsigned& p_flags, metadb_handle_list_cref, const GUID& p_caller) override {
                p_flags = 0;
                if (p_caller == contextmenu_item::caller_active_playlist ||
                    p_caller == contextmenu_item::caller_active_playlist_selection) {
                    m_owner->get_item_name(m_idx, p_out);
                    return true;
                }
                return false;
            }
            void execute(metadb_handle_list_cref, const GUID&) override { run_organize(); }
            bool get_description(pfc::string_base& p_out) override { return m_owner->get_item_description(m_idx, p_out); }
            GUID get_guid() override { return pfc::guid_null; }
            bool is_mappable_shortcut() override { return false; }
        private:
            service_ptr_t<playlist_context_menu> m_owner;
            unsigned m_idx;
        };
        (void)p_data; (void)p_caller;
        return new node(this, p_index);
    }

    void item_execute_simple(unsigned, const GUID&, metadb_handle_list_cref, const GUID&) override { run_organize(); }
};
static contextmenu_item_factory_t<playlist_context_menu> g_contextmenu_factory;

} // namespace
