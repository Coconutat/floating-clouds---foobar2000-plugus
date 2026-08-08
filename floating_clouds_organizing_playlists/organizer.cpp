#include "stdafx.h"
#include "organizer.h"
#include "config.h"

#include <string.h>

// ============================================================================
// Organizer implementation
// ============================================================================

namespace {

// --- Case-insensitive string helpers (ASCII) ---

bool contains_ci(const pfc::string8& haystack, const char* needle)
{
    if (haystack.is_empty() || !needle || !*needle) return false;
    const char* h = haystack.get_ptr();
    const size_t hlen = haystack.length();
    const size_t nlen = strlen(needle);
    if (nlen > hlen) return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char a = h[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

bool iequals(const pfc::string8& a, const char* b)
{
    if (!b || !*b) return a.is_empty();
    const char* pa = a.get_ptr();
    const size_t la = a.length();
    const size_t lb = strlen(b);
    if (la != lb) return false;
    for (size_t i = 0; i < la; i++) {
        char x = pa[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return false;
    }
    return true;
}

// Trim leading/trailing ASCII whitespace.
pfc::string8 trim_ws(const pfc::string8& s)
{
    const char* p = s.get_ptr();
    size_t begin = 0, end = s.length();
    while (begin < end && (p[begin] == ' ' || p[begin] == '\t')) begin++;
    while (end > begin && (p[end - 1] == ' ' || p[end - 1] == '\t')) end--;
    if (begin == 0 && end == s.length()) return s;
    return pfc::string8(p + begin, end - begin);
}

// First value of a metadata field ("" when missing).
pfc::string8 meta_value(const file_info* fi, const char* name)
{
    if (!fi) return pfc::string8();
    const char* v = fi->meta_get(name, 0);
    return v ? pfc::string8(v) : pfc::string8();
}

// Route one track into exactly one destination group (see organizer.h).
void route_track(metadb_handle_ptr track, std::map<pfc::string8, metadb_handle_list>& groups)
{
    metadb_info_container::ptr info;
    if (!track->get_info_ref(info) || !info.is_valid()) {
        groups[kUnknownArtistPlaylist].add_item(track);
        return;
    }
    const file_info& fi = info->info();

    // 1. Soundtrack: album name contains "soundtrack" (case-insensitive).
    if (contains_ci(meta_value(&fi, "album"), kSoundtrackKeyword)) {
        groups[kSoundtrackPlaylist].add_item(track);
        return;
    }
    // 2. Compilations: album artist == "Various Artists" (case-insensitive).
    pfc::string8 aa = trim_ws(meta_value(&fi, "album artist"));
    if (iequals(aa, kVariousArtistsPlaylist)) {
        groups[kVariousArtistsPlaylist].add_item(track);
        return;
    }
    // 3. Missing album artist -> fall back to track artist, then Unknown Artist.
    if (aa.is_empty()) aa = trim_ws(meta_value(&fi, "artist"));
    if (aa.is_empty()) aa = kUnknownArtistPlaylist;
    groups[aa].add_item(track);
}

// Find a playlist by exact name; pfc_infinite when not found.
t_size find_playlist_by_name(playlist_manager* pm, const char* name)
{
    const t_size n = pm->get_playlist_count();
    for (t_size i = 0; i < n; i++) {
        pfc::string8 nm;
        if (pm->playlist_get_name(i, nm) && nm == name) return i;
    }
    return pfc_infinite;
}

// Lock protecting the generated destination playlists from user edits, reorder
// or rename. The organizer itself unlocks before rebuilding and relocks after,
// so the generated playlists stay intact between runs.
class organizer_lock : public playlist_lock {
public:
    t_uint32 get_filter_mask() override {
        return filter_add | filter_remove | filter_reorder | filter_replace |
               filter_rename | filter_remove_playlist;
    }
    bool query_items_add(t_size, const pfc::list_base_const_t<metadb_handle_ptr>&, const bit_array&) override { return false; }
    bool query_items_reorder(const t_size*, t_size) override { return false; }
    bool query_items_remove(const bit_array&, bool) override { return false; }
    bool query_item_replace(t_size, const metadb_handle_ptr&, const metadb_handle_ptr&) override { return false; }
    bool query_playlist_rename(const char*, t_size) override { return false; }
    bool query_playlist_remove() override { return false; }
    bool execute_default_action(t_size) override { return false; } // keep default double-click = play
    void on_playlist_index_change(t_size) override {}
    void on_playlist_remove() override {}
    void get_lock_name(pfc::string_base& p_out) override { p_out = "Playlist Organizer"; }
    void show_ui() override {}
};

// Process-wide lock instance shared across runs so we can unlock/relock the same one.
service_ptr_t<playlist_lock> g_lock;
service_ptr_t<playlist_lock> get_lock()
{
    if (!g_lock.is_valid()) g_lock = fb2k::service_new<organizer_lock>();
    return g_lock;
}

} // namespace

namespace organizer {

bool organize_active_playlist(OrganizeResult& out, pfc::string8& error)
{
    auto pm = playlist_manager::get();
    const t_size src = pm->get_active_playlist();
    if (src == pfc_infinite) { error = "No active playlist."; return false; }

    metadb_handle_list items;
    pm->playlist_get_all_items(src, items);
    if (items.get_count() == 0) { error = "The active playlist is empty."; return false; }

    // Route every track (source order preserved) into exactly one group.
    std::map<pfc::string8, metadb_handle_list> raw;
    for (t_size i = 0; i < items.get_count(); i++) {
        route_track(items[i], raw);
    }

    // Sort destination groups A-Z (case-insensitive) so the generated playlists
    // are created in letter order regardless of encounter order.
    std::vector<std::pair<pfc::string8, metadb_handle_list>> groups(raw.begin(), raw.end());
    std::sort(groups.begin(), groups.end(),
        [](const auto& a, const auto& b) { return str_less_ci(a.first, b.first); });

    service_ptr_t<playlist_lock> lock = get_lock();

    // Rebuild each non-empty destination: find or create, unlock, clear, refill,
    // then re-lock so the A-Z order and contents stay protected between runs.
    for (auto& kv : groups) {
        metadb_handle_list& list = kv.second;
        if (list.get_count() == 0) continue; // skip empty groups

        t_size pid = find_playlist_by_name(pm.get_ptr(), kv.first);
        if (pid == pfc_infinite) {
            pid = pm->create_playlist(kv.first, pfc_infinite, pfc_infinite);
            if (pid == pfc_infinite) { error = "Failed to create a playlist."; return false; }
        }
        pm->playlist_lock_uninstall(pid, lock); // our own lock, if present
        pm->playlist_clear(pid);
        if (!pm->playlist_add_items(pid, list, bit_array_true())) {
            error = "Failed to add items (playlist locked?).";
            return false;
        }
        pm->playlist_lock_install(pid, lock); // re-lock (best effort)
        out.playlists_updated++;
        out.tracks_organized += list.get_count();
    }

    // Also sort the whole playlist list A-Z so existing (non-generated) playlists
    // stay alphabetized alongside the newly created artist playlists.
    sort_all_playlists(error);
    return true;
}

bool sort_all_playlists(pfc::string8& error)
{
    auto pm = playlist_manager::get();
    const t_size n = pm->get_playlist_count();
    if (n < 2) return true;

    std::vector<std::pair<pfc::string8, t_size>> entries;
    entries.reserve(n);
    for (t_size i = 0; i < n; i++) {
        pfc::string8 nm;
        pm->playlist_get_name(i, nm);
        entries.push_back(std::make_pair(nm, i));
    }
    std::stable_sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b) { return str_less_ci(a.first, b.first); });

    // Permutation: new position i takes old index entries[i].second.
    std::vector<t_size> order(n);
    for (t_size i = 0; i < n; i++) order[i] = entries[i].second;
    if (!pm->reorder(order.data(), n)) {
        error = "Failed to reorder the playlist list.";
        return false;
    }
    return true;
}

} // namespace organizer
