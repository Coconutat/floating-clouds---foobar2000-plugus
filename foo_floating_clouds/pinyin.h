#pragma once

#include "stdafx.h"

// ============================================================================
// Pinyin index + zh-CN collation helpers for the playlist picker.
//
// - index_key(name): 0-25 = A-Z, 26-35 = 0-9, 36 = '#'. Uses the FIRST
//   meaningful character (skips leading symbols/whitespace): Chinese maps to
//   its pinyin initial via the GB2312 level-1 boundary table (level-1 GB2312
//   chars are pinyin-ordered, so a ~23-entry table suffices — no big data).
// - compare_zh(a,b): zh-CN collation (pinyin order for Chinese), -1/0/1.
//
// UTF-8 in / UTF-8 out. Only the codepage-936 conversion happens for the
// table lookup (WideCharToMultiByte(936) is available on every Windows).
// ============================================================================

namespace pinyin {

namespace detail {

// Decode one UTF-8 char at *s; advances s past it; returns the code point.
inline unsigned utf8_next(const char*& s)
{
    const unsigned char c = (unsigned char)*s;
    if (c < 0x80) { s++; return c; }
    unsigned cp = 0;
    int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { s++; return 0xFFFD; }
    for (int i = 0; i < extra; i++) {
        s++;
        cp = (cp << 6) | ((unsigned char)*s & 0x3F);
    }
    return cp;
}

// GB2312 level-1 boundary table: level-1 chars (GBK B0A1..D7F9) are ordered by
// pinyin, so the last boundary <= the code gives the pinyin initial.
inline const struct { unsigned short code; unsigned char letter; } gb2312_boundary[] = {
    { 0xB0A1, 'A' }, { 0xB0C5, 'B' }, { 0xB2C1, 'C' }, { 0xB4EE, 'D' },
    { 0xB6EA, 'E' }, { 0xB7A2, 'F' }, { 0xB8C1, 'G' }, { 0xB9FE, 'H' },
    { 0xBBF7, 'J' }, { 0xBFA6, 'K' }, { 0xC0AC, 'L' }, { 0xC2E8, 'M' },
    { 0xC4C3, 'N' }, { 0xC5B6, 'O' }, { 0xC5BE, 'P' }, { 0xC6DA, 'Q' },
    { 0xC8BB, 'R' }, { 0xC8F6, 'S' }, { 0xCBFA, 'T' }, { 0xCDDA, 'W' },
    { 0xCEF4, 'X' }, { 0xD1B9, 'Y' }, { 0xD4D1, 'Z' },
};

// BMP code point -> GBK code (codepage 936). 0 when not representable.
inline int unicode_to_gbk(unsigned cp)
{
    if (cp > 0xFFFF) return 0;
    const wchar_t wc = (wchar_t)cp;
    char buf[2];
    const int n = WideCharToMultiByte(936, 0, &wc, 1, buf, 2, NULL, NULL);
    if (n != 2) return 0;
    return ((unsigned char)buf[0] << 8) | (unsigned char)buf[1];
}

inline bool is_cjk(unsigned cp)
{
    return (cp >= 0x3400 && cp <= 0x9FFF) || (cp >= 0xF900 && cp <= 0xFAFF);
}

} // namespace detail

// 0-25 = A-Z, 26-35 = 0-9, 36 = '#' (empty or unmappable also -> '#').
inline int index_key(const pfc::string8& name)
{
    const char* p = name.get_ptr();
    if (!p || !*p) return 36;
    while (*p) {
        const unsigned cp = detail::utf8_next(p);
        if (cp >= 'A' && cp <= 'Z') return (int)(cp - 'A');
        if (cp >= 'a' && cp <= 'z') return (int)(cp - 'a');
        if (cp >= '0' && cp <= '9') return 26 + (int)(cp - '0');
        if (detail::is_cjk(cp)) {
            const int gbk = detail::unicode_to_gbk(cp);
            if (gbk >= 0xB0A1 && gbk <= 0xD7F9) {
                const unsigned short g = (unsigned short)gbk;
                unsigned char letter = 'A';
                for (const auto& e : detail::gb2312_boundary) {
                    if (g >= e.code) letter = e.letter;
                    else break;
                }
                return (int)(letter - 'A');
            }
            return 36; // CJK outside GB2312 level-1
        }
        // symbol / space / punctuation -> skip to the next character
    }
    return 36;
}

// zh-CN collation compare (pinyin order for Chinese). Returns -1 / 0 / 1.
inline int compare_zh(const pfc::string8& a, const pfc::string8& b)
{
    pfc::stringcvt::string_wide_from_utf8 wa(a), wb(b);
    const int r = CompareStringEx(L"zh-CN", NORM_IGNORECASE, wa, -1, wb, -1, NULL, NULL, 0);
    if (r == 0) return 0;
    return r - 2; // CSTR_LESS_THAN=1, CSTR_EQUAL=2, CSTR_GREATER_THAN=3
}

} // namespace pinyin
