#include "stdafx.h"
#include "tagger.h"
#include "localization.h"
#include "config.h"
#include <SDK/metadb.h>          // metadb_io_v2
#include <SDK/file_info_filter.h> // file_info_filter
#include <SDK/completion_notify.h> // completion_notify

// ============================================================================
// Tag write pipeline implementation
// ============================================================================

namespace {

// Normalize an ISO releaseDate "2012-10-22T07:00:00Z" to "2012-10-22".
pfc::string8 normalize_date(const char* iso)
{
    pfc::string8 s(iso ? iso : "");
    const char* t = strchr(s.get_ptr(), 'T');
    if (t) s.truncate((t_size)(t - s.get_ptr()));
    return s;
}

// Shared counters between the worker-thread filter and the completion notify.
struct ApplyState {
    std::atomic<int> written{ 0 }; // tracks matched by (disc, track)
    std::atomic<int> skipped{ 0 }; // selected tracks that did not match
};

// Write one field if it is enabled and (overwrite or the field is empty).
void apply_field(file_info& info, uint32_t bit, const char* meta, const char* value,
                 const TagOptions& opt, bool& changed)
{
    if (!(opt.fields & bit)) return;
    if (!value || !*value) return;
    const char* cur = info.meta_get(meta, 0);
    if (opt.overwrite || !cur || !*cur) {
        info.meta_set(meta, value);
        changed = true;
    }
}

// file_info_filter that runs on a worker thread (only references copied data).
class TagFilter : public file_info_filter {
public:
    TagFilter(std::shared_ptr<ApplyState> state, AppleAlbum album,
              std::map<uint32_t, t_size> by_key,
              std::map<pfc::string8, t_size> path_to_index,
              TagOptions options)
        : m_state(state), m_album(std::move(album)), m_by_key(std::move(by_key)),
          m_path_to_index(std::move(path_to_index)), m_options(options) {}

    bool apply_filter(trackRef p_location, t_filestats, file_info& info) override
    {
        // Resolve which Apple track applies to this file.
        const AppleTrack* t = nullptr;
        if (m_options.force_order) {
            // Emergency mode: ignore track numbers; the Nth selected track gets
            // the Nth Apple track's tags (path -> selection index).
            auto it = m_path_to_index.find(p_location->get_path());
            if (it == m_path_to_index.end()) { m_state->skipped++; return false; }
            if (it->second >= m_album.tracks.get_size()) { m_state->skipped++; return false; }
            t = &m_album.tracks[it->second];
        } else {
            int disc = 1, trk = 0;
            const char* ds = info.meta_get("DISCNUMBER", 0);
            if (ds && *ds) disc = atoi(ds);
            const char* ts = info.meta_get("TRACKNUMBER", 0);
            if (ts && *ts) trk = atoi(ts);

            const uint32_t key = ((uint32_t)(disc & 0xFFFF) << 16) | (uint32_t)(trk & 0xFFFF);
            auto it = m_by_key.find(key);
            if (it == m_by_key.end()) {
                m_state->skipped++;
                return false; // no Apple track with this (disc, track): skip, never guess
            }
            t = &m_album.tracks[it->second];
        }

        // Force-order mode implies overwrite (it is an emergency override).
        TagOptions eff = m_options;
        if (m_options.force_order) eff.overwrite = true;

        const bool changed = apply_fields(info, *t, eff);
        m_state->written++;
        return changed;
    }

private:
    bool apply_fields(file_info& info, const AppleTrack& t, const TagOptions& opt)
    {
        bool changed = false;
        apply_field(info, FieldTitle,       "TITLE",         t.title.get_ptr(),        opt, changed);
        apply_field(info, FieldAlbum,       "ALBUM",         t.album.get_ptr(),        opt, changed);
        apply_field(info, FieldArtist,      "ARTIST",        t.artist.get_ptr(),       opt, changed);
        apply_field(info, FieldAlbumArtist, "ALBUM ARTIST",  t.album_artist.get_ptr(), opt, changed);
        apply_field(info, FieldGenre,       "GENRE",         t.genre.get_ptr(),        opt, changed);
        apply_field(info, FieldComposer,    "COMPOSER",      t.composer.get_ptr(),     opt, changed);

        pfc::string8 date = normalize_date(t.release_date.get_ptr());
        apply_field(info, FieldDate,        "DATE",          date.get_ptr(),            opt, changed);

        pfc::string8 trk_s = pfc::string8() << t.track_number;
        apply_field(info, FieldTrackNo,     "TRACKNUMBER",   trk_s.get_ptr(),           opt, changed);

        pfc::string8 disc_s = pfc::string8() << t.disc_number;
        apply_field(info, FieldDiscNo,      "DISCNUMBER",    disc_s.get_ptr(),          opt, changed);

        // Album-level fields: the same value applies to every track of the album.
        apply_field(info, FieldCopyright,   "COPYRIGHT",     m_album.copyright.get_ptr(), opt, changed);

        if (m_album.track_count > 0) {
            pfc::string8 total_s = pfc::string8() << m_album.track_count;
            apply_field(info, FieldTotalTracks, "TOTALTRACKS", total_s.get_ptr(), opt, changed);
        }
        if (m_album.disc_count > 0) {
            pfc::string8 discn_s = pfc::string8() << m_album.disc_count;
            apply_field(info, FieldTotalDiscs, "TOTALDISCS", discn_s.get_ptr(), opt, changed);
        }

        if (opt.fields & FieldExplicit) {
            if (t.explicit_flag) {
                info.meta_set("EXPLICIT", "1");
                changed = true;
            } else if (opt.overwrite && info.meta_exists("EXPLICIT")) {
                info.meta_remove_field("EXPLICIT");
                changed = true;
            }
        }
        return changed;
    }

    std::shared_ptr<ApplyState> m_state;
    AppleAlbum m_album;
    std::map<uint32_t, t_size> m_by_key;
    std::map<pfc::string8, t_size> m_path_to_index;
    TagOptions m_options;
};

// Shows the write summary when the async operation completes (main thread).
class ApplyNotify : public completion_notify {
public:
    ApplyNotify(std::shared_ptr<ApplyState> s) : m_state(s) {}
    void on_completion(unsigned) noexcept override
    {
        pfc::string_formatter msg;
        msg << tr8("Updated ", "更新了 ") << (int)m_state->written.load()
            << tr8(" track(s), skipped ", " 首，跳过 ") << (int)m_state->skipped.load()
            << tr8(".", " 首。");
        popup_message::g_show(msg, tr8("Apple Music Tags", "Apple Music 标签更新"));
    }
private:
    std::shared_ptr<ApplyState> m_state;
};

// --- Local Traditional -> Simplified conversion ---------------------------------

// Shared counters for the local T2S conversion.
struct T2SState {
    std::atomic<int> converted{ 0 }; // tracks whose tags actually changed
    std::atomic<int> unchanged{ 0 }; // tracks with no Traditional text
};

// file_info_filter that converts every text meta field (that changes) from
// Traditional Chinese to Simplified Chinese. Idempotent: numbers, empty values
// and already-Simplified text are left untouched.
class T2SFilter : public file_info_filter {
public:
    T2SFilter(std::shared_ptr<T2SState> state) : m_state(state) {}

    bool apply_filter(trackRef, t_filestats, file_info& info) override
    {
        bool any = false;
        const t_size field_count = info.meta_get_count();
        for (t_size f = 0; f < field_count; f++) {
            const char* name = info.meta_enum_name(f);
            const t_size vc = info.meta_enum_value_count(f);
            if (!name || !*name || vc == 0) continue;

            pfc::list_t<pfc::string8> vals;
            bool field_changed = false;
            for (t_size j = 0; j < vc; j++) {
                const char* val = info.meta_enum_value(f, j);
                const char* base = val ? val : "";
                pfc::string8 s = to_simplified_str(base);
                if (strcmp(s.get_ptr(), base) != 0) field_changed = true;
                vals.add_item(s);
            }
            if (!field_changed) continue;

            // Rebuild the field from the converted values (preserves order).
            info.meta_remove_field(name);
            for (t_size j = 0; j < vals.get_size(); j++) info.meta_add(name, vals[j].get_ptr());
            any = true;
        }

        if (any) m_state->converted++; else m_state->unchanged++;
        return any;
    }

private:
    std::shared_ptr<T2SState> m_state;
};

// Shows the local-conversion summary when the async operation completes.
class T2SNotify : public completion_notify {
public:
    T2SNotify(std::shared_ptr<T2SState> s) : m_state(s) {}
    void on_completion(unsigned) noexcept override
    {
        pfc::string_formatter msg;
        msg << tr8("Converted ", "转换了 ") << (int)m_state->converted.load()
            << tr8(" track(s), skipped ", " 首，跳过 ") << (int)m_state->unchanged.load()
            << tr8(" (no Traditional Chinese).", " 首（无繁体中文）。");
        popup_message::g_show(msg, tr8("Apple Music Tags", "Apple Music 标签更新"));
    }
private:
    std::shared_ptr<T2SState> m_state;
};

} // namespace

void apply_tags(const metadb_handle_list& selected, const AppleAlbum& album,
                const TagOptions& options, fb2k::hwnd_t parent)
{
    if (selected.get_count() == 0 || album.tracks.get_size() == 0) return;

    // Map (disc<<16 | track) -> AppleTrack index. Copying the album/map into the
    // filter keeps the worker thread safe (no references to the dialog's data).
    std::map<uint32_t, t_size> by_key;
    for (t_size i = 0; i < album.tracks.get_size(); i++) {
        const AppleTrack& t = album.tracks[i];
        const uint32_t key = ((uint32_t)(t.disc_number & 0xFFFF) << 16) | (uint32_t)(t.track_number & 0xFFFF);
        by_key[key] = i;
    }

    // For emergency force-order mode: path -> index in the selection list.
    std::map<pfc::string8, t_size> path_to_index;
    if (options.force_order) {
        for (t_size i = 0; i < selected.get_count(); i++) {
            path_to_index[selected[i]->get_path()] = i;
        }
    }

    auto state = std::make_shared<ApplyState>();
    service_ptr_t<file_info_filter> filter = new service_impl_t<TagFilter>(state, album, by_key, path_to_index, options);
    service_ptr_t<completion_notify> notify = new service_impl_t<ApplyNotify>(state);

    static_api_ptr_t<metadb_io_v2>()->update_info_async(
        selected, filter, parent,
        metadb_io_v2::op_flag_background | metadb_io_v2::op_flag_delay_ui, notify);
}

void convert_local_tags_to_simplified(const metadb_handle_list& selected, fb2k::hwnd_t parent)
{
    if (selected.get_count() == 0) return;

    auto state = std::make_shared<T2SState>();
    service_ptr_t<file_info_filter> filter = new service_impl_t<T2SFilter>(state);
    service_ptr_t<completion_notify> notify = new service_impl_t<T2SNotify>(state);

    static_api_ptr_t<metadb_io_v2>()->update_info_async(
        selected, filter, parent,
        metadb_io_v2::op_flag_background | metadb_io_v2::op_flag_delay_ui, notify);
}
