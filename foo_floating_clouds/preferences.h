#pragma once

#include "stdafx.h"
#include "config.h"
#include <string>

// Snapshot of every cfg value the Preferences page edits. The page loads it
// once, compares the controls against it, and writes each key once on apply.
struct PrefsState {
    // Default member initializers double as the "reset to defaults" values.
    int opacity = DEFAULT_OPACITY;
    bool auto_hide = DEFAULT_AUTO_HIDE;
    bool debug_log = false;
    bool attached_baseline = false;
    int style = DEFAULT_STYLE;
    int skin = DEFAULT_SKIN;
    int color_mode = DEFAULT_COLOR_MODE;
    int language = DEFAULT_LANGUAGE;
    std::string font; // UTF-8; empty = follow foobar2000

    // Hotkey slots: 0 = drag, 1 = show/hide, 2 = cycle style, 3 = cycle skin.
    uint32_t hk_mod[4] = { DEFAULT_HK_DRAG_MOD, DEFAULT_HK_VIS_MOD, DEFAULT_HK_STYLE_MOD, DEFAULT_HK_SKIN_MOD };
    uint32_t hk_vk[4]  = { DEFAULT_HK_DRAG_VK, DEFAULT_HK_VIS_VK, DEFAULT_HK_STYLE_VK, DEFAULT_HK_SKIN_VK };

    void load();                          // read every cfg key
    void apply() const;                   // write every cfg key
    bool equals(const PrefsState&) const; // compare the apply-time keys
};
