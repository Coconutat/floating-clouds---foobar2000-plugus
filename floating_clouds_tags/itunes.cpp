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
bool extract_trailing_id(const char* s, int& out_id)
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
    out_id = atoi(idstr.get_ptr());
    return out_id > 0;
}

} // namespace

bool parse_album_url(const wchar_t* url, pfc::string8& out_region, int& out_album_id)
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
                        out_album_id = atoi(idstr.get_ptr());
                        if (out_album_id > 0) return true;
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
        out_album_id = atoi(idstr.get_ptr());
        return out_album_id > 0;
    }

    // Final fallback: grab the album ID digits from anywhere in the string.
    return extract_trailing_id(s, out_album_id);
}

void fetch_album(int album_id, const char* region, abort_callback& abort, AppleAlbum& out)
{
    out = AppleAlbum();

    pfc::string_formatter url;
    url << "https://itunes.apple.com/lookup?id=" << album_id
        << "&entity=song&country=" << region << "&limit=200";

    service_ptr_t<http_request> req = http_client::get()->create_request("GET");
    file::ptr f = req->run(url, abort); // throws on network failure / non-2xx

    pfc::string8 body;
    char chunk[4096];
    for (;;) {
        const t_size rd = f->read(chunk, sizeof(chunk), abort);
        if (rd == 0) break;
        body.add_string(chunk, rd);
    }

    json::Value root;
    if (!json::parse(body.get_ptr(), body.length(), root)) {
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
        out.not_available = true;
        out.error = "This album is not available on the selected storefront (region).";
        return;
    }
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

void fetch_album_auto(int album_id, const char* region, abort_callback& abort, AppleAlbum& out)
{
    fetch_album(album_id, region, abort, out); // throws on network failure
    if (out.ok) return;

    // CN storefront often lacks an album that exists elsewhere. Fall back to HK
    // and convert its Traditional Chinese metadata to Simplified Chinese.
    if (out.not_available && pfc::stricmp_ascii(region, "cn") == 0) {
        AppleAlbum hk;
        fetch_album(album_id, "hk", abort, hk); // throws on network failure
        if (hk.ok) {
            to_simplified(hk);
            // hk.region stays "hk": it is the real source storefront. The
            // conversion is recorded separately (T2S is independent of source).
            hk.t2s_applied = true;
            out = hk;
        }
    }
}
