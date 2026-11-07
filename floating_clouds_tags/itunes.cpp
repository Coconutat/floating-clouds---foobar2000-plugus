#include "stdafx.h"
#include "itunes.h"
#include "config.h"
#include <SDK/http_client.h> // http_client / http_request / file::ptr

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

void fetch_album(int64_t album_id, const char* region, abort_callback& abort, AppleAlbum& out)
{
    out = AppleAlbum();

    pfc::string_formatter url;
    url << "https://itunes.apple.com/lookup?id=" << (long long)album_id
        << "&entity=song&country=" << region << "&limit=200";

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

void fetch_album_auto(int64_t album_id, const char* region, abort_callback& abort, AppleAlbum& out)
{
    // The lookup API answers from exactly one storefront, and that storefront may
    // know an album only as metadata without its track rows, or not know it at
    // all (verified live: the same album is collection-only in CN and complete in
    // HK). So walk the storefronts ordered by how well their metadata language
    // fits the selection, and stop at the first complete answer.
    const int selected_index = region_index_for_code(region);
    const RegionInfo& selected = kRegions[selected_index];

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
            fetch_album(album_id, kRegions[order[k]].code, abort, candidate);
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
        // (CN does exactly that for albums it only partly carries). Its
        // album-level text is an official localisation, so use it directly and
        // borrow the track rows from the storefront that has them - then only
        // the borrowed text needs the Traditional->Simplified fallback below.
        // Same track count keeps this from mixing up different editions.
        if (album_only.track_count == 0 || album_only.track_count == out.track_count) {
            out.album_name = album_only.album_name;
            out.album_artist = album_only.album_artist;
            out.genre = album_only.genre;
            if (!album_only.copyright.is_empty()) out.copyright = album_only.copyright;
            if (!album_only.release_date.is_empty()) out.release_date = album_only.release_date;
            FB2K_console_formatter() << "Apple Music Tags: album text from " << album_only.region
                << ", track rows from " << out.region;
        }
    }

    // Fallback only: whatever could not be fetched in Simplified directly - HK/TW
    // track rows, or every field when no Chinese storefront carries the album -
    // is converted char-by-char. Idempotent, so the album-level text already
    // merged from an official Simplified storefront is left untouched.
    if (pfc::stricmp_ascii(selected.lang, "zh-Hans") == 0 && has_traditional(out)) {
        to_simplified(out);
        out.t2s_applied = true;
    }
}
