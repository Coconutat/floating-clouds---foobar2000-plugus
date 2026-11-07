#include "stdafx.h"
#include "itunes.h"
#include "config.h"
#include <SDK/http_client.h> // http_client / http_request / file::ptr
#include <future>            // std::async for the parallel storefront scan
#include <mutex>             // ordering of the finished scans

// ============================================================================
// Apple Music / iTunes Lookup API client implementation
// ============================================================================

namespace {

// Extract the longest contiguous run of digits in `s` — for Apple Music URLs
// that is the album ID (the trailing numeric path segment). Used as a fallback
// when the canonical URL structure could not be parsed.
bool extract_trailing_id(const char* s, int64_t& out_id)
{
    const char* best_start = nullptr;
    t_size best_len = 0;
    const char* p = s;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            const char* start = p;
            while (*p >= '0' && *p <= '9') p++;
            const t_size len = (t_size)(p - start);
            if (len > best_len) { best_start = start; best_len = len; }
        } else {
            p++;
        }
    }
    if (!best_start || best_len < 5) return false; // album IDs are >= 5 digits
    pfc::string8 idstr;
    idstr.set_string(best_start, best_len);
    out_id = strtoll(idstr.get_ptr(), nullptr, 10); // 64-bit: IDs exceed INT_MAX
    return out_id > 0;
}

} // namespace

namespace {

// Printable preview of the first bytes of a response body, for console
// diagnostics (keeps a binary/gzip body readable instead of dumping junk).
pfc::string8 printable_head(const pfc::string8& body, t_size max = 160)
{
    pfc::string8 out;
    const t_size n = (std::min)(body.length(), max);
    for (t_size i = 0; i < n; i++) {
        const unsigned char c = (unsigned char)body[i];
        out.add_byte((c >= 0x20 && c < 0x7F) ? (char)c : '.');
    }
    return out;
}

// Index of a storefront by its iTunes country code (DEFAULT_REGION if unknown).
int region_index_for_code(const char* code)
{
    if (code) {
        for (int i = 0; i < kRegionCount; i++) {
            if (pfc::stricmp_ascii(kRegions[i].code, code) == 0) return i;
        }
    }
    return DEFAULT_REGION;
}

// How far a storefront is from what the user selected: the selection itself,
// then the same metadata language, then the other Chinese script, then English,
// then anything else. Lower tiers are tried first.
int storefront_tier(const RegionInfo& selected, const RegionInfo& other)
{
    if (strcmp(selected.code, other.code) == 0) return 0;
    if (strcmp(selected.lang, other.lang) == 0) return 1;
    const bool selected_zh = (selected.lang[0] == 'z' && selected.lang[1] == 'h');
    const bool other_zh = (other.lang[0] == 'z' && other.lang[1] == 'h');
    if (selected_zh && other_zh) return 2;
    if (strcmp(other.lang, "en") == 0) return 3;
    return 4;
}

// Map Apple's web locale ("zh-Hans-CN", "en-US", ...) onto a metadata language
// family from kRegions. A bare "zh" means Simplified on the Apple web site.
pfc::string8 locale_family(const char* locale)
{
    if (!locale || !*locale) return pfc::string8();
    for (int i = 0; i < kRegionCount; i++) {
        const char* family = kRegions[i].lang;
        if (_strnicmp(locale, family, strlen(family)) == 0) return pfc::string8(family);
    }
    if (_strnicmp(locale, "zh", 2) == 0) return pfc::string8("zh-Hans");
    return pfc::string8();
}

// A music.apple.com link carries both a storefront path segment and a `?l=`
// display language. The path only says where the link was copied from; `l=` is
// the language the page actually shows. Honor `l=` when choosing the first
// storefront to read, otherwise the pasted link would be ignored.
void apply_locale_preference(const char* url, pfc::string8& region)
{
    const char* p = strstr(url, "?l=");
    if (!p) p = strstr(url, "&l=");
    if (!p) {
        p = strstr(url, "?lang=");
        if (!p) p = strstr(url, "&lang=");
    }
    if (!p) return;
    const char* value = strchr(p, '=') + 1;
    const char* end = value;
    while (*end && *end != '&' && *end != '#') end++;
    pfc::string8 locale;
    locale.set_string(value, (t_size)(end - value));
    const int idx = region_index_for_language(locale_family(locale.get_ptr()).get_ptr());
    if (idx >= 0) region = kRegions[idx].code;
}

} // namespace

bool parse_album_url(const wchar_t* url, pfc::string8& out_region, int64_t& out_album_id)
{
    out_region.reset();
    out_album_id = 0;
    if (!url || !*url) return false;

    pfc::stringcvt::string_utf8_from_wide u(url);
    const char* s = u.get_ptr();

    const char* host = strstr(s, "music.apple.com/");
    if (host) {
        // Advance PAST "music.apple.com/" (16 chars: "music.apple.com" is 15, + '/').
        const char* p = host + 16;
        const char* region_end = strchr(p, '/');
        if (region_end && region_end != p) {
            out_region.set_string(p, (t_size)(region_end - p));
            const char* q = strstr(region_end, "/album/");
            if (q) {
                q += 7;
                const char* slug_end = strchr(q, '/');
                if (slug_end) {
                    const char* id_start = slug_end + 1;
                    const char* id_end = id_start;
                    while (*id_end >= '0' && *id_end <= '9') id_end++;
                    if (id_end != id_start) {
                        pfc::string8 idstr;
                        idstr.set_string(id_start, (t_size)(id_end - id_start));
                        out_album_id = strtoll(idstr.get_ptr(), nullptr, 10);
                        if (out_album_id > 0) {
                            apply_locale_preference(s, out_region);
                            return true;
                        }
                    }
                }
            }
        }
        // Fall through to the numeric extraction below if the URL structure
        // didn't match the expected shape.
    }

    // Bare numeric album ID (strip ASCII whitespace on both ends).
    pfc::string8 t(s);
    size_t b = 0, e = t.length();
    while (b < e && (t[b] == ' ' || t[b] == '\t' || t[b] == '\r' || t[b] == '\n')) b++;
    while (e > b && (t[e - 1] == ' ' || t[e - 1] == '\t' || t[e - 1] == '\r' || t[e - 1] == '\n')) e--;
    bool all_digits = (e > b);
    for (size_t i = b; i < e; i++) {
        if (t[i] < '0' || t[i] > '9') { all_digits = false; break; }
    }
    if (all_digits) {
        pfc::string8 idstr;
        idstr.set_string(t.get_ptr() + b, (t_size)(e - b));
        out_album_id = strtoll(idstr.get_ptr(), nullptr, 10);
        return out_album_id > 0;
    }

    // Final fallback: grab the album ID digits from anywhere in the string.
    if (!extract_trailing_id(s, out_album_id)) return false;
    apply_locale_preference(s, out_region);
    return true;
}

void fetch_album(int64_t album_id, const char* region, abort_callback& abort, AppleAlbum& out,
                 const char* lang)
{
    out = AppleAlbum();

    pfc::string_formatter url;
    url << "https://itunes.apple.com/lookup?id=" << (long long)album_id
        << "&entity=song&country=" << region << "&limit=200";
    // The API only localises a few fields (genre and the like) and rejects
    // zh-Hans locales, but "zh_CN" is accepted and yields Simplified genre names.
    if (lang && *lang) url << "&l=" << lang;

    service_ptr_t<http_request> req = http_client::get()->create_request("GET");
    // Apple gzips when asked; the host's decompression support is not something a
    // component can rely on, so ask for plain bytes. (If the body still comes back
    // binary, parse_failed_below logs the first bytes so it is diagnosable.)
    req->add_header("Accept-Encoding", "identity");
    file::ptr f = req->run(url, abort); // throws on network failure / non-2xx

    pfc::string8 body;
    char chunk[4096];
    for (;;) {
        const t_size rd = f->read(chunk, sizeof(chunk), abort);
        if (rd == 0) break;
        body.add_string(chunk, rd);
    }

    // Diagnostics: a silent parse/no-collection failure here looks exactly like
    // "album not available" in the dialog, which is not actionable.
    FB2K_console_formatter() << "Apple Music Tags: GET " << url.get_ptr() << " -> " << body.length() << " bytes";

    json::Value root;
    if (!json::parse(body.get_ptr(), body.length(), root)) {
        FB2K_console_formatter() << "Apple Music Tags: JSON parse failed, head=" << printable_head(body);
        out.error = "Failed to parse the Apple Music response.";
        return;
    }

    const json::Value* results = root.get("results");
    if (!results || results->type != json::Value::Array) {
        out.error = "Unexpected response from Apple Music.";
        return;
    }

    bool found_album = false;
    for (t_size i = 0; i < results->arr.get_size(); i++) {
        const json::Value& r = results->arr[i];
        pfc::string8 wt = r.get_string("wrapperType");
        if (wt == "collection") {
            if (!found_album) {
                out.album_name = r.get_string("collectionName");
                out.album_artist = r.get_string("collectionArtistName");
                // Single-artist albums have no collectionArtistName: fall back to
                // artistName so ALBUM ARTIST can still be written.
                if (out.album_artist.is_empty()) out.album_artist = r.get_string("artistName");
                out.genre = r.get_string("primaryGenreName");
                out.release_date = r.get_string("releaseDate");
                out.track_count = (int)r.get_number("trackCount");
                out.copyright = r.get_string("copyright");
                out.album_id = album_id;
                out.region = region;
                found_album = true;
            }
        } else if (wt == "track") {
            AppleTrack t;
            t.title = r.get_string("trackName");
            t.artist = r.get_string("artistName");
            t.album = r.get_string("collectionName");
            t.album_artist = r.get_string("collectionArtistName");
            if (t.album_artist.is_empty()) t.album_artist = t.artist; // same fallback
            t.genre = r.get_string("primaryGenreName");
            t.composer = r.get_string("composer");
            t.release_date = r.get_string("releaseDate");
            t.track_number = (int)r.get_number("trackNumber");
            t.disc_number = (int)r.get_number("discNumber");
            if (t.disc_number <= 0) t.disc_number = 1;
            const int td = (int)r.get_number("discCount");
            if (td > out.disc_count) out.disc_count = td; // album disc count = max
            pfc::string8 exp = r.get_string("trackExplicitness");
            t.explicit_flag = (exp == "explicit");
            out.tracks.add_item(t);
        }
    }

    if (!found_album) {
        FB2K_console_formatter() << "Apple Music Tags: " << region << " has no collection (tracks="
            << (int)out.tracks.get_size() << ")";
        out.not_available = true;
        out.error = "This album is not available on the selected storefront (region).";
        return;
    }
    FB2K_console_formatter() << "Apple Music Tags: " << region << " ok, tracks=" << (int)out.tracks.get_size();
    out.ok = true;
}

// ---------------------------------------------------------------------------
// Traditional -> Simplified Chinese conversion (independent T2S capability)
// ---------------------------------------------------------------------------

// Convert one UTF-8 string from Traditional to Simplified Chinese using the
// built-in Windows NLS mapping (no bundled table needed).
pfc::string8 to_simplified_str(const char* utf8)
{
    pfc::string8 in(utf8 ? utf8 : "");
    if (in.is_empty()) return in;
    pfc::stringcvt::string_wide_from_utf8 w(in);
    const int needed = LCMapStringEx(L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, w, -1, nullptr, 0, nullptr, nullptr, 0);
    if (needed <= 1) return in; // nothing to convert
    pfc::array_t<wchar_t> buf;
    buf.set_size((t_size)needed);
    LCMapStringEx(L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, w, -1, buf.get_ptr(), needed, nullptr, nullptr, 0);
    pfc::stringcvt::string_utf8_from_wide conv(buf.get_ptr());
    return pfc::string8(conv);
}

namespace {
bool changed_by_t2s(const char* s)
{
    return s && *s && strcmp(to_simplified_str(s).get_ptr(), s) != 0;
}
} // namespace

void to_simplified(AppleAlbum& a)
{
    a.album_name = to_simplified_str(a.album_name.get_ptr());
    a.album_artist = to_simplified_str(a.album_artist.get_ptr());
    a.genre = to_simplified_str(a.genre.get_ptr());
    a.copyright = to_simplified_str(a.copyright.get_ptr());
    for (t_size i = 0; i < a.tracks.get_size(); i++) {
        AppleTrack& t = a.tracks[i];
        t.title = to_simplified_str(t.title.get_ptr());
        t.artist = to_simplified_str(t.artist.get_ptr());
        t.album = to_simplified_str(t.album.get_ptr());
        t.album_artist = to_simplified_str(t.album_artist.get_ptr());
        t.genre = to_simplified_str(t.genre.get_ptr());
        t.composer = to_simplified_str(t.composer.get_ptr());
    }
}

bool has_traditional(const AppleAlbum& a)
{
    if (changed_by_t2s(a.album_name.get_ptr())) return true;
    if (changed_by_t2s(a.album_artist.get_ptr())) return true;
    if (changed_by_t2s(a.genre.get_ptr())) return true;
    if (changed_by_t2s(a.copyright.get_ptr())) return true;
    for (t_size i = 0; i < a.tracks.get_size(); i++) {
        const AppleTrack& t = a.tracks[i];
        if (changed_by_t2s(t.title.get_ptr())) return true;
        if (changed_by_t2s(t.artist.get_ptr())) return true;
        if (changed_by_t2s(t.album.get_ptr())) return true;
        if (changed_by_t2s(t.album_artist.get_ptr())) return true;
        if (changed_by_t2s(t.genre.get_ptr())) return true;
        if (changed_by_t2s(t.composer.get_ptr())) return true;
    }
    return false;
}

namespace {

// Character classes that matter when deciding whether a storefront's answer is
// "Chinese metadata": CJK ideographs count, Japanese kana disqualify (kanji are
// in the same Unicode range, so a Japanese title would otherwise score as
// Chinese and win the merge).
struct ScriptStats {
    unsigned cjk = 0;
    bool kana = false;
};

ScriptStats script_stats(const char* utf8)
{
    ScriptStats st;
    if (!utf8) return st;
    const unsigned char* p = (const unsigned char*)utf8;
    while (*p) {
        unsigned cp = 0;
        int extra = 0;
        if (*p < 0x80) { p++; continue; }
        if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
        else { p++; continue; }
        p++;
        bool valid = true;
        for (int i = 0; i < extra; i++, p++) {
            if ((*p & 0xC0) != 0x80) { valid = false; break; }
            cp = (cp << 6) | (*p & 0x3F);
        }
        if (!valid) continue;
        if (cp >= 0x4E00 && cp <= 0x9FFF) st.cjk++;
        else if ((cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0x31F0 && cp <= 0x31FF)) st.kana = true;
    }
    return st;
}

unsigned count_cjk(const char* utf8) { return script_stats(utf8).cjk; }

unsigned album_cjk(const AppleAlbum& a)
{
    unsigned n = count_cjk(a.album_name.get_ptr());
    n += count_cjk(a.album_artist.get_ptr());
    for (t_size i = 0; i < a.tracks.get_size(); i++) {
        n += count_cjk(a.tracks[i].title.get_ptr());
        n += count_cjk(a.tracks[i].artist.get_ptr());
    }
    return n;
}

// 2 = Chinese and already Simplified (official localisation, keep as-is),
// 1 = Chinese but Traditional (usable, would need conversion),
// 0 = not Chinese at all (romanised / English / Japanese).
int text_score(const pfc::string8& s)
{
    if (s.is_empty()) return -1;
    const ScriptStats st = script_stats(s.get_ptr());
    if (st.kana || st.cjk == 0) return 0;
    const bool needs_conversion = strcmp(to_simplified_str(s.get_ptr()).get_ptr(), s.get_ptr()) != 0;
    return needs_conversion ? 1 : 2;
}

// Take `candidate` only when it is a *better* script match than what we have, so
// an already-Simplified value always beats one that would need conversion.
void prefer_text(pfc::string8& target, const pfc::string8& candidate, bool& merged)
{
    if (candidate.is_empty()) return;
    if (text_score(candidate) > text_score(target)) {
        target = candidate;
        merged = true;
    }
}

const AppleTrack* find_track(const AppleAlbum& album, int disc, int track)
{
    for (t_size i = 0; i < album.tracks.get_size(); i++) {
        if (album.tracks[i].disc_number == disc && album.tracks[i].track_number == track)
            return &album.tracks[i];
    }
    return nullptr;
}

// Single-artist albums: when a track's Traditional artist converts to exactly the
// official Simplified album artist we already have, use the official spelling
// instead of deriving it by conversion.
void normalize_track_artists(AppleAlbum& album)
{
    if (album.album_artist.is_empty()) return;
    for (t_size t = 0; t < album.tracks.get_size(); t++) {
        AppleTrack& track = album.tracks[t];
        if (track.artist == album.album_artist) continue;
        if (strcmp(to_simplified_str(track.artist.get_ptr()).get_ptr(), album.album_artist.get_ptr()) == 0)
            track.artist = album.album_artist;
    }
}

bool is_zh_hans(const RegionInfo& r) { return strcmp(r.lang, "zh-Hans") == 0; }

// Auto mode ("CN auto: scan regions for Chinese"): ask every storefront at once
// and keep the answer with the most Chinese text. Parallel because one lookup
// takes ~1s and there are ten storefronts; done sequentially the dialog would
// sit there for ten seconds.
// Ranking: an answer with track rows beats album-only, then more Chinese wins,
// then a Simplified storefront beats a Traditional one, and only then does
// whichever finished first win. Pure arrival order would happily pick SG/US,
// which carry a Chinese album title but a romanised artist name.
void scan_storefronts(int64_t album_id, const char* lang, abort_callback& abort, AppleAlbum& out)
{
    std::vector<AppleAlbum> albums(kRegionCount);
    std::vector<unsigned> cjk(kRegionCount, 0);
    std::vector<unsigned> order(kRegionCount, 0);
    std::mutex order_guard;
    unsigned finished = 0;

    std::vector<std::future<void>> jobs;
    for (int i = 0; i < kRegionCount; i++) {
        if (!kRegions[i].code[0] || kRegions[i].scan_all) continue; // skip the auto row
        jobs.push_back(std::async(std::launch::async, [&, i]() {
            try {
                fetch_album(album_id, kRegions[i].code, abort, albums[(size_t)i], lang);
            } catch (...) {
                // One slow or failing storefront must not fail the whole scan.
            }
            std::lock_guard<std::mutex> lock(order_guard);
            order[(size_t)i] = finished++;
        }));
    }
    for (auto& job : jobs) job.wait();

    if (abort.is_aborting()) throw exception_aborted();

    for (int i = 0; i < kRegionCount; i++) cjk[(size_t)i] = albums[(size_t)i].ok ? album_cjk(albums[(size_t)i]) : 0;

    auto better = [&](int a, int b) {
        const bool tracks_a = albums[(size_t)a].tracks.get_size() > 0;
        const bool tracks_b = albums[(size_t)b].tracks.get_size() > 0;
        if (tracks_a != tracks_b) return tracks_a;
        if (cjk[(size_t)a] != cjk[(size_t)b]) return cjk[(size_t)a] > cjk[(size_t)b];
        if (is_zh_hans(kRegions[a]) != is_zh_hans(kRegions[b])) return is_zh_hans(kRegions[a]);
        return order[(size_t)a] < order[(size_t)b];
    };

    int best = -1;
    for (int i = 0; i < kRegionCount; i++) {
        if (!albums[(size_t)i].ok || kRegions[i].scan_all) continue;
        if (cjk[(size_t)i] == 0) continue; // auto mode is looking for Chinese
        if (best < 0 || better(i, best)) best = i;
    }

    if (best < 0) {
        // Nothing Chinese anywhere: do not fail, fall back to the usual language
        // order so the user still gets usable (English) metadata.
        for (int i = 0; i < kRegionCount; i++) {
            if (!albums[(size_t)i].ok) continue;
            const bool tracks = albums[(size_t)i].tracks.get_size() > 0;
            if (best < 0 || (tracks && albums[(size_t)best].tracks.get_size() == 0)) best = i;
        }
        if (best < 0) {
            out.not_available = true;
            out.error = "This album was not found in any Apple Music storefront.";
            return;
        }
    }

    out = albums[(size_t)best];

    // Per-field merge, not per-storefront: Apple scatters the official Simplified
    // text across catalogs. On the reported single, SG returns a Simplified album
    // title and track title but a romanised artist, CN returns the official
    // Simplified artist and genre but no track rows, and HK/TW have the track rows
    // in Traditional. Taking the best script per field therefore yields fully
    // official Simplified metadata and needs no conversion at all.
    bool merged = false;
    for (int i = 0; i < kRegionCount; i++) {
        if (i == best || kRegions[i].scan_all || !albums[(size_t)i].ok) continue;
        const AppleAlbum& other = albums[(size_t)i];
        prefer_text(out.album_name, other.album_name, merged);
        prefer_text(out.album_artist, other.album_artist, merged);
        prefer_text(out.genre, other.genre, merged);
        prefer_text(out.copyright, other.copyright, merged);
        for (t_size t = 0; t < out.tracks.get_size(); t++) {
            AppleTrack& dst = out.tracks[t];
            const AppleTrack* src = find_track(other, dst.disc_number, dst.track_number);
            if (!src) continue;
            prefer_text(dst.title, src->title, merged);
            prefer_text(dst.artist, src->artist, merged);
            prefer_text(dst.album, src->album, merged);
            prefer_text(dst.album_artist, src->album_artist, merged);
            prefer_text(dst.composer, src->composer, merged);
        }
    }

    // Single-artist albums: official Simplified album artist beats a converted
    // track artist.
    normalize_track_artists(out);

    out.multi_source = merged;

    FB2K_console_formatter() << "Apple Music Tags: auto scan picked " << out.region
        << " (chinese chars=" << cjk[(size_t)best] << ", tracks=" << (int)out.tracks.get_size()
        << ", merged=" << (merged ? 1 : 0) << ")";
}

} // namespace

void fetch_album_auto(int64_t album_id, const char* region, abort_callback& abort, AppleAlbum& out)
{
    // The lookup API answers from exactly one storefront, and that storefront may
    // know an album only as metadata without its track rows, or not know it at
    // all (verified live: the same album is collection-only in CN and complete in
    // HK). So walk the storefronts ordered by how well their metadata language
    // fits the selection, and stop at the first complete answer.
    const int selected_index = region_index_for_code(region);
    const RegionInfo& selected = kRegions[selected_index];

    // Apple localises only a few fields via `l`, but for a Simplified target the
    // accepted "zh_CN" value at least turns genre names into Simplified.
    const char* api_lang = (pfc::stricmp_ascii(selected.lang, "zh-Hans") == 0) ? "zh_CN" : nullptr;

    // Auto mode: no single storefront is trusted, scan them all and keep the
    // answer that actually carries Chinese text.
    if (selected.scan_all) {
        scan_storefronts(album_id, api_lang, abort, out);
        if (out.ok && api_lang && has_traditional(out)) {
            to_simplified(out);
            out.t2s_applied = true;
        }
        return;
    }

    int order[kRegionCount];
    for (int i = 0; i < kRegionCount; i++) order[i] = i;
    for (int i = 1; i < kRegionCount; i++) { // stable insertion sort by tier
        const int value = order[i];
        const int value_tier = storefront_tier(selected, kRegions[value]);
        int j = i - 1;
        while (j >= 0 && storefront_tier(selected, kRegions[order[j]]) > value_tier) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = value;
    }

    out = AppleAlbum();
    bool have_album_only = false;
    AppleAlbum album_only;
    pfc::string8 first_error;
    bool have_error = false;

    for (int k = 0; k < kRegionCount; k++) {
        abort.check();
        AppleAlbum candidate;
        try {
            fetch_album(album_id, kRegions[order[k]].code, abort, candidate, api_lang);
        } catch (const exception_aborted&) {
            throw;
        } catch (const pfc::exception& e) {
            if (!have_error) { first_error = e.what(); have_error = true; }
            continue;
        }
        if (!candidate.ok) {
            // Keep the first concrete reason (parse failure etc.) for the dialog:
            // "not found anywhere" alone hides a broken HTTP response.
            if (!have_error && !candidate.error.is_empty()) { first_error = candidate.error; have_error = true; }
            continue;
        }
        if (candidate.tracks.get_size() > 0) { out = candidate; break; }
        if (!have_album_only) { album_only = candidate; have_album_only = true; }
    }

    if (!out.ok) {
        if (!have_album_only) {
            out.not_available = true;
            out.error = "This album was not found in any Apple Music storefront.";
            if (have_error) {
                out.error = pfc::string_formatter() << out.error << " Last error: " << first_error;
            }
            return;
        }
        out = album_only; // metadata only: no storefront had the track list
    } else if (have_album_only) {
        // The preferred storefront listed the album but returned no track rows
        // (CN does exactly that for albums it only partly carries). Merge its text
        // field by field so an official Simplified value wins over the borrowed
        // Traditional one - the same rule the auto scan uses. Same track count
        // keeps this from mixing up different editions.
        if (album_only.track_count == 0 || album_only.track_count == out.track_count) {
            bool merged = false;
            prefer_text(out.album_name, album_only.album_name, merged);
            prefer_text(out.album_artist, album_only.album_artist, merged);
            prefer_text(out.genre, album_only.genre, merged);
            prefer_text(out.copyright, album_only.copyright, merged);
            normalize_track_artists(out);
            FB2K_console_formatter() << "Apple Music Tags: album text considered from "
                << album_only.region << ", track rows from " << out.region;
            out.multi_source = out.multi_source || merged;
        }
    }

    // Fallback only: whatever could not be fetched in Simplified directly (HK/TW
    // track rows, or every field when no Chinese storefront carries the album) is
    // converted char-by-char. Idempotent, so fields already official Simplified
    // are left untouched.
    if (api_lang && has_traditional(out)) {
        to_simplified(out);
        out.t2s_applied = true;
    }
}
