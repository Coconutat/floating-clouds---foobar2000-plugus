#include "stdafx.h"
#include "config.h"
#include "localization.h"
#include "dialog.h"
#include <SDK/ui.h> // ui_selection_manager

// ============================================================================
// Entry points: a main-menu item and a track context-menu item. Both open the
// fetch/apply dialog with the currently selected tracks.
// ============================================================================

namespace {

// --- Component GUIDs (unique to this component) ---
static const GUID guid_menu_group = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x21 } };
static const GUID guid_cmd_tags   = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x22 } };
static const GUID guid_cm_tags    = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x23 } };

void run_tags_dialog(const metadb_handle_list& selected)
{
    if (selected.get_count() == 0) {
        popup_message::g_show(
            tr8("Select the tracks of an album first.", "请先选中一个专辑的曲目。"),
            tr8("Apple Music Tags", "Apple Music 标签更新"));
        return;
    }
    TagsDialog dlg(selected);
    dlg.DoModal(core_api::get_main_window());
}

// ---------------------------------------------------------------------------
// Main menu item: File > Playlist > Apple Music Tags > Update tags from Apple Music...
// ---------------------------------------------------------------------------
static mainmenu_group_popup_factory g_mainmenu_group(
    guid_menu_group, mainmenu_groups::file_playlist,
    mainmenu_commands::sort_priority_dontcare, "Apple Music Tags");

class mainmenu_commands_impl : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 1; }
    GUID get_command(t_uint32) override { return guid_cmd_tags; }
    void get_name(t_uint32, pfc::string_base& p_out) override {
        p_out = tr8("Update Tags from Apple Music...", "从 Apple Music 更新标签…");
    }
    bool get_description(t_uint32, pfc::string_base& p_out) override {
        p_out = tr8("Fetches the selected album's tags from Apple Music and writes them to the selected tracks.",
                    "获取所选专辑的 Apple Music 标签，并写入选中的曲目。");
        return true;
    }
    GUID get_parent() override { return guid_menu_group; }
    void execute(t_uint32, service_ptr_t<service_base>) override {
        metadb_handle_list selection;
        static_api_ptr_t<ui_selection_manager>()->get_selection(selection);
        run_tags_dialog(selection);
    }
};
static mainmenu_commands_factory_t<mainmenu_commands_impl> g_mainmenu_factory;

// ---------------------------------------------------------------------------
// Track context-menu item: right-click selected tracks.
// ---------------------------------------------------------------------------
class tags_context_menu : public contextmenu_item_v2 {
public:
    unsigned get_num_items() override { return 1; }
    GUID get_item_guid(unsigned) override { return guid_cm_tags; }
    void get_item_name(unsigned, pfc::string_base& p_out) override {
        p_out = tr8("Update Tags from Apple Music...", "从 Apple Music 更新标签…");
    }
    bool get_item_description(unsigned, pfc::string_base& p_out) override {
        p_out = tr8("Fetches the selected album's tags from Apple Music and writes them to the selected tracks.",
                    "获取所选专辑的 Apple Music 标签，并写入选中的曲目。");
        return true;
    }
    t_enabled_state get_enabled_state(unsigned) override { return DEFAULT_ON; }

    contextmenu_item_node_root* instantiate_item(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) override {
        class node : public contextmenu_item_node_root_leaf {
        public:
            node(tags_context_menu* owner, unsigned idx) : m_owner(owner), m_idx(idx) {}
            bool get_display_data(pfc::string_base& p_out, unsigned& p_flags, metadb_handle_list_cref p_data, const GUID&) override {
                p_flags = 0;
                if (p_data.get_count() > 0) {
                    m_owner->get_item_name(m_idx, p_out);
                    return true;
                }
                return false;
            }
            void execute(metadb_handle_list_cref p_data, const GUID&) override { run_tags_dialog(p_data); }
            bool get_description(pfc::string_base& p_out) override { return m_owner->get_item_description(m_idx, p_out); }
            GUID get_guid() override { return pfc::guid_null; }
            bool is_mappable_shortcut() override { return false; }
        private:
            service_ptr_t<tags_context_menu> m_owner;
            unsigned m_idx;
        };
        (void)p_caller;
        return new node(this, p_index);
    }

    void item_execute_simple(unsigned, const GUID&, metadb_handle_list_cref p_data, const GUID&) override { run_tags_dialog(p_data); }
};
static contextmenu_item_factory_t<tags_context_menu> g_contextmenu_factory;

} // namespace
