#pragma once

#include "stdafx.h"

// ============================================================================
// Minimal JSON DOM parser (RFC 8259 subset) sufficient for the iTunes Lookup
// API response. Strings are kept as raw UTF-8 in pfc::string8. Surrogate pairs
// (\uDXXX\uDYYY) are decoded to UTF-8; bare BMP escapes are encoded as UTF-8.
// ============================================================================

namespace json {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0;
    pfc::string8 str;
    pfc::list_t<Value> arr;
    pfc::list_t< std::pair<pfc::string8, Value> > obj; // key/value pairs, order preserved

    // Object member by key; nullptr if absent or not an object.
    const Value* get(const char* key) const {
        if (type != Object) return nullptr;
        for (t_size i = 0; i < obj.get_size(); i++) {
            if (obj[i].first == key) return &obj[i].second;
        }
        return nullptr;
    }
    pfc::string8 get_string(const char* key) const {
        const Value* v = get(key);
        return (v && v->type == String) ? v->str : pfc::string8();
    }
    double get_number(const char* key) const {
        const Value* v = get(key);
        return (v && v->type == Number) ? v->num : 0;
    }
};

namespace detail {

inline void skip_ws(const char*& p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
}

inline bool parse_string(const char*& p, const char* end, pfc::string8& out)
{
    if (p >= end || *p != '"') return false;
    p++;
    pfc::string8 s;
    while (p < end) {
        char c = *p;
        if (c == '"') { p++; out = s; return true; }
        if (c == '\\') {
            p++;
            if (p >= end) return false;
            char e = *p;
            switch (e) {
                case '"': s.add_byte('"'); p++; break;
                case '\\': s.add_byte('\\'); p++; break;
                case '/': s.add_byte('/'); p++; break;
                case 'b': s.add_byte('\b'); p++; break;
                case 'f': s.add_byte('\f'); p++; break;
                case 'n': s.add_byte('\n'); p++; break;
                case 'r': s.add_byte('\r'); p++; break;
                case 't': s.add_byte('\t'); p++; break;
                case 'u': {
                    p++;
                    if (p + 4 > end) return false;
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = p[i]; cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                        else return false;
                    }
                    p += 4;
                    // Low surrogate: \uDXXX\uDYYY
                    if (cp >= 0xD800 && cp <= 0xDBFF && p + 1 < end && p[0] == '\\' && p[1] == 'u') {
                        const char* q = p + 2;
                        unsigned lo = 0;
                        bool ok = true;
                        for (int i = 0; i < 4; i++) {
                            char h = q[i]; lo <<= 4;
                            if (h >= '0' && h <= '9') lo |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') lo |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') lo |= (unsigned)(h - 'A' + 10);
                            else { ok = false; break; }
                        }
                        if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            p += 6;
                        }
                    }
                    // Encode UTF-8
                    if (cp < 0x80) s.add_byte((char)cp);
                    else if (cp < 0x800) {
                        s.add_byte((char)(0xC0 | (cp >> 6)));
                        s.add_byte((char)(0x80 | (cp & 0x3F)));
                    } else if (cp < 0x10000) {
                        s.add_byte((char)(0xE0 | (cp >> 12)));
                        s.add_byte((char)(0x80 | ((cp >> 6) & 0x3F)));
                        s.add_byte((char)(0x80 | (cp & 0x3F)));
                    } else {
                        s.add_byte((char)(0xF0 | (cp >> 18)));
                        s.add_byte((char)(0x80 | ((cp >> 12) & 0x3F)));
                        s.add_byte((char)(0x80 | ((cp >> 6) & 0x3F)));
                        s.add_byte((char)(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: return false;
            }
        } else {
            s.add_byte(c);
            p++;
        }
    }
    return false;
}

inline bool parse_number(const char*& p, const char* end, double& out)
{
    const char* start = p;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E')) p++;
    if (p == start) return false;
    pfc::string8 s;
    s.set_string(start, (t_size)(p - start));
    out = atof(s.get_ptr());
    return true;
}

inline bool parse_value(const char*& p, const char* end, Value& out);

inline bool parse_array(const char*& p, const char* end, Value& out)
{
    if (p >= end || *p != '[') return false;
    p++;
    out.type = Value::Array;
    skip_ws(p, end);
    if (p < end && *p == ']') { p++; return true; }
    while (p < end) {
        Value v;
        if (!parse_value(p, end, v)) return false;
        out.arr.add_item(v);
        skip_ws(p, end);
        if (p >= end) return false;
        if (*p == ',') { p++; skip_ws(p, end); continue; }
        if (*p == ']') { p++; return true; }
        return false;
    }
    return false;
}

inline bool parse_object(const char*& p, const char* end, Value& out)
{
    if (p >= end || *p != '{') return false;
    p++;
    out.type = Value::Object;
    skip_ws(p, end);
    if (p < end && *p == '}') { p++; return true; }
    while (p < end) {
        pfc::string8 key;
        if (!parse_string(p, end, key)) return false;
        skip_ws(p, end);
        if (p >= end || *p != ':') return false;
        p++;
        skip_ws(p, end);
        Value v;
        if (!parse_value(p, end, v)) return false;
        out.obj.add_item(std::make_pair(key, v));
        skip_ws(p, end);
        if (p >= end) return false;
        if (*p == ',') { p++; skip_ws(p, end); continue; }
        if (*p == '}') { p++; return true; }
        return false;
    }
    return false;
}

inline bool parse_value(const char*& p, const char* end, Value& out)
{
    skip_ws(p, end);
    if (p >= end) return false;
    char c = *p;
    if (c == '{') return parse_object(p, end, out);
    if (c == '[') return parse_array(p, end, out);
    if (c == '"') { out.type = Value::String; return parse_string(p, end, out.str); }
    if (c == 't' && end - p >= 4 && memcmp(p, "true", 4) == 0) { p += 4; out.type = Value::Bool; out.b = true; return true; }
    if (c == 'f' && end - p >= 5 && memcmp(p, "false", 5) == 0) { p += 5; out.type = Value::Bool; out.b = false; return true; }
    if (c == 'n' && end - p >= 4 && memcmp(p, "null", 4) == 0) { p += 4; out.type = Value::Null; return true; }
    out.type = Value::Number;
    return parse_number(p, end, out.num);
}

} // namespace detail

// Parse the whole document; returns true and fills root on success.
inline bool parse(const char* text, t_size len, Value& root)
{
    if (!text) return false;
    const char* p = text;
    const char* end = text + len;
    detail::skip_ws(p, end);
    if (!detail::parse_value(p, end, root)) return false;
    detail::skip_ws(p, end);
    return p == end;
}

} // namespace json
