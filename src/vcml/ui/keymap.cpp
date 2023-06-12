/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/keymap.h"

namespace vcml {
namespace ui {

vector<syminfo> keyboard_us_layout = {
    { KEYSYM_ESC, KEY_ESC, false, false, false },

    /* symbols on F1 - F12 key row */
    { KEYSYM_F1, KEY_F1, false, false, false },
    { KEYSYM_F2, KEY_F2, false, false, false },
    { KEYSYM_F3, KEY_F3, false, false, false },
    { KEYSYM_F4, KEY_F4, false, false, false },
    { KEYSYM_F5, KEY_F5, false, false, false },
    { KEYSYM_F6, KEY_F6, false, false, false },
    { KEYSYM_F7, KEY_F7, false, false, false },
    { KEYSYM_F8, KEY_F8, false, false, false },
    { KEYSYM_F9, KEY_F9, false, false, false },
    { KEYSYM_F10, KEY_F10, false, false, false },
    { KEYSYM_F11, KEY_F11, false, false, false },
    { KEYSYM_F12, KEY_F12, false, false, false },

    /* symbols on number key row */
    { KEYSYM_BACKQUOTE, KEY_GRAVE, false, false, false },
    { KEYSYM_1, KEY_1, false, false, false },
    { KEYSYM_2, KEY_2, false, false, false },
    { KEYSYM_3, KEY_3, false, false, false },
    { KEYSYM_4, KEY_4, false, false, false },
    { KEYSYM_5, KEY_5, false, false, false },
    { KEYSYM_6, KEY_6, false, false, false },
    { KEYSYM_7, KEY_7, false, false, false },
    { KEYSYM_8, KEY_8, false, false, false },
    { KEYSYM_9, KEY_9, false, false, false },
    { KEYSYM_0, KEY_0, false, false, false },
    { KEYSYM_MINUS, KEY_MINUS, false, false, false },
    { KEYSYM_EQUAL, KEY_EQUAL, false, false, false },
    { KEYSYM_BACKSPACE, KEY_BACKSPACE, false, false, false },

    /* symbols on number key row when shift is down */
    { KEYSYM_TILDE, KEY_GRAVE, true, false, false },
    { KEYSYM_EXCLAIM, KEY_1, true, false, false },
    { KEYSYM_AT, KEY_2, true, false, false },
    { KEYSYM_HASH, KEY_3, true, false, false },
    { KEYSYM_DOLLAR, KEY_4, true, false, false },
    { KEYSYM_PERCENT, KEY_5, true, false, false },
    { KEYSYM_CARET, KEY_6, true, false, false },
    { KEYSYM_AMPERSAND, KEY_7, true, false, false },
    { KEYSYM_ASTERISK, KEY_8, true, false, false },
    { KEYSYM_LEFTPAR, KEY_9, true, false, false },
    { KEYSYM_RIGHTPAR, KEY_0, true, false, false },
    { KEYSYM_UNDERSCORE, KEY_MINUS, true, false, false },
    { KEYSYM_PLUS, KEY_EQUAL, true, false, false },

    /* symbols on QWERTY row */
    { KEYSYM_TAB, KEY_TAB, false, false, false },
    { KEYSYM_q, KEY_Q, false, false, false },
    { KEYSYM_w, KEY_W, false, false, false },
    { KEYSYM_e, KEY_E, false, false, false },
    { KEYSYM_r, KEY_R, false, false, false },
    { KEYSYM_t, KEY_T, false, false, false },
    { KEYSYM_y, KEY_Y, false, false, false },
    { KEYSYM_u, KEY_U, false, false, false },
    { KEYSYM_i, KEY_I, false, false, false },
    { KEYSYM_o, KEY_O, false, false, false },
    { KEYSYM_p, KEY_P, false, false, false },
    { KEYSYM_LEFTBRACKET, KEY_LEFTBRACE, false, false, false },
    { KEYSYM_RIGHTBRACKET, KEY_RIGHTBRACE, false, false, false },
    { KEYSYM_BACKSLASH, KEY_BACKSLASH, false, false, false },

    /* symbols on QWERTY row when shift is down */
    { KEYSYM_Q, KEY_Q, true, false, false },
    { KEYSYM_W, KEY_W, true, false, false },
    { KEYSYM_E, KEY_E, true, false, false },
    { KEYSYM_R, KEY_R, true, false, false },
    { KEYSYM_T, KEY_T, true, false, false },
    { KEYSYM_Y, KEY_Y, true, false, false },
    { KEYSYM_U, KEY_U, true, false, false },
    { KEYSYM_I, KEY_I, true, false, false },
    { KEYSYM_O, KEY_O, true, false, false },
    { KEYSYM_P, KEY_P, true, false, false },
    { KEYSYM_LEFTBRACE, KEY_LEFTBRACE, true, false, false },
    { KEYSYM_RIGHTBRACE, KEY_RIGHTBRACE, true, false, false },
    { KEYSYM_PIPE, KEY_BACKSLASH, true, false, false },

    /* symbols on ASDFG row */
    { KEYSYM_CAPSLOCK, KEY_CAPSLOCK, false, false, false },
    { KEYSYM_a, KEY_A, false, false, false },
    { KEYSYM_s, KEY_S, false, false, false },
    { KEYSYM_d, KEY_D, false, false, false },
    { KEYSYM_f, KEY_F, false, false, false },
    { KEYSYM_g, KEY_G, false, false, false },
    { KEYSYM_h, KEY_H, false, false, false },
    { KEYSYM_j, KEY_J, false, false, false },
    { KEYSYM_k, KEY_K, false, false, false },
    { KEYSYM_l, KEY_L, false, false, false },
    { KEYSYM_SEMICOLON, KEY_SEMICOLON, false, false, false },
    { KEYSYM_QUOTE, KEY_APOSTROPHE, false, false, false },
    { KEYSYM_ENTER, KEY_ENTER, false, false, false },

    /* symbols on ASDFG row when shift is down */
    { KEYSYM_A, KEY_A, true, false, false },
    { KEYSYM_S, KEY_S, true, false, false },
    { KEYSYM_D, KEY_D, true, false, false },
    { KEYSYM_F, KEY_F, true, false, false },
    { KEYSYM_G, KEY_G, true, false, false },
    { KEYSYM_H, KEY_H, true, false, false },
    { KEYSYM_J, KEY_J, true, false, false },
    { KEYSYM_K, KEY_K, true, false, false },
    { KEYSYM_L, KEY_L, true, false, false },
    { KEYSYM_COLON, KEY_SEMICOLON, true, false, false },
    { KEYSYM_DBLQUOTE, KEY_APOSTROPHE, true, false, false },

    /* symbols on ZXCVB row */
    { KEYSYM_LEFTSHIFT, KEY_LEFTSHIFT, false, false, false },
    { KEYSYM_z, KEY_Z, false, false, false },
    { KEYSYM_x, KEY_X, false, false, false },
    { KEYSYM_c, KEY_C, false, false, false },
    { KEYSYM_v, KEY_V, false, false, false },
    { KEYSYM_b, KEY_B, false, false, false },
    { KEYSYM_n, KEY_N, false, false, false },
    { KEYSYM_m, KEY_M, false, false, false },
    { KEYSYM_COMMA, KEY_COMMA, false, false, false },
    { KEYSYM_DOT, KEY_DOT, false, false, false },
    { KEYSYM_SLASH, KEY_SLASH, false, false, false },
    { KEYSYM_RIGHTSHIFT, KEY_RIGHTSHIFT, false, false, false },

    /* symbols on ZXCVB row when shift is down */
    { KEYSYM_Z, KEY_Z, true, false, false },
    { KEYSYM_X, KEY_X, true, false, false },
    { KEYSYM_C, KEY_C, true, false, false },
    { KEYSYM_V, KEY_V, true, false, false },
    { KEYSYM_B, KEY_B, true, false, false },
    { KEYSYM_N, KEY_N, true, false, false },
    { KEYSYM_M, KEY_M, true, false, false },
    { KEYSYM_LESS, KEY_COMMA, true, false, false },
    { KEYSYM_GREATER, KEY_DOT, true, false, false },
    { KEYSYM_QUESTION, KEY_SLASH, true, false, false },

    /* symbols on bottom row */
    { KEYSYM_LEFTCTRL, KEY_LEFTCTRL, false, false, false },
    { KEYSYM_LEFTMETA, KEY_LEFTMETA, false, false, false },
    { KEYSYM_LEFTALT, KEY_LEFTALT, false, false, false },
    { KEYSYM_SPACE, KEY_SPACE, false, false, false },
    { KEYSYM_RIGHTALT, KEY_RIGHTALT, false, false, false },
    { KEYSYM_RIGHTMETA, KEY_RIGHTMETA, false, false, false },
    { KEYSYM_MENU, KEY_MENU, false, false, false },
    { KEYSYM_RIGHTCTRL, KEY_RIGHTCTRL, false, false, false },

    /* extended keys */
    { KEYSYM_PRINT, KEY_PRINT, false, false, false },
    { KEYSYM_SCROLLOCK, KEY_SCROLLLOCK, false, false, false },
    { KEYSYM_PAUSE, KEY_PAUSE, false, false, false },

    { KEYSYM_INSERT, KEY_INSERT, false, false, false },
    { KEYSYM_DELETE, KEY_DELETE, false, false, false },
    { KEYSYM_HOME, KEY_HOME, false, false, false },
    { KEYSYM_END, KEY_END, false, false, false },
    { KEYSYM_PAGEUP, KEY_PAGEUP, false, false, false },
    { KEYSYM_PAGEDOWN, KEY_PAGEDOWN, false, false, false },

    { KEYSYM_LEFT, KEY_LEFT, false, false, false },
    { KEYSYM_RIGHT, KEY_RIGHT, false, false, false },
    { KEYSYM_UP, KEY_UP, false, false, false },
    { KEYSYM_DOWN, KEY_DOWN, false, false, false },

    /* keypad */
    { KEYSYM_NUMLOCK, KEY_NUMLOCK, false, false, false },
    { KEYSYM_KPENTER, KEY_KPENTER, false, false, false },
    { KEYSYM_KPPLUS, KEY_KPPLUS, false, false, false },
    { KEYSYM_KPMUL, KEY_KPASTERISK, false, false, false },
    { KEYSYM_KPMINUS, KEY_KPMINUS, false, false, false },
    { KEYSYM_KPDOT, KEY_KPDOT, false, false, false },
    { KEYSYM_KPDIV, KEY_KPSLASH, false, false, false },
    { KEYSYM_KP0, KEY_KP0, false, false, false },
    { KEYSYM_KP1, KEY_KP1, false, false, false },
    { KEYSYM_KP2, KEY_KP2, false, false, false },
    { KEYSYM_KP3, KEY_KP3, false, false, false },
    { KEYSYM_KP4, KEY_KP4, false, false, false },
    { KEYSYM_KP5, KEY_KP5, false, false, false },
    { KEYSYM_KP6, KEY_KP6, false, false, false },
    { KEYSYM_KP7, KEY_KP7, false, false, false },
    { KEYSYM_KP8, KEY_KP8, false, false, false },
    { KEYSYM_KP9, KEY_KP9, false, false, false },

    /* keypad functions */
    { KEYSYM_KPDELETE, KEY_KPDOT, false, false, false },
    { KEYSYM_KPINSERT, KEY_KP0, false, false, false },
    { KEYSYM_KPEND, KEY_KP1, false, false, false },
    { KEYSYM_KPDOWN, KEY_KP2, false, false, false },
    { KEYSYM_KPPAGEDOWN, KEY_KP3, false, false, false },
    { KEYSYM_KPLEFT, KEY_KP4, false, false, false },
    { KEYSYM_KPRIGHT, KEY_KP6, false, false, false },
    { KEYSYM_KPHOME, KEY_KP7, false, false, false },
    { KEYSYM_KPUP, KEY_KP8, false, false, false },
    { KEYSYM_KPPAGEUP, KEY_KP9, false, false, false },
};

keymap::keymap(const vector<syminfo>& l): layout(l) {
    // sanity checking
    for (const auto info : layout) {
        if (info.code > KEY_MAX)
            VCML_ERROR("invalid key code 0x%x", info.code);
    }
}

const syminfo* keymap::lookup_symbol(u32 symbol) const {
    for (auto& info : layout)
        if (info.keysym == symbol)
            return &info;
    return nullptr;
}

std::unordered_map<string, keymap> keymap::maps(
    { { "us", keymap(keyboard_us_layout) } });

const keymap& keymap::lookup(const string& name) {
    VCML_ERROR_ON(name.empty(), "empty keyboard layout name");

    auto it = maps.find(to_lower(name));
    if (it != maps.end())
        return it->second;

    log_error("available keymaps:");
    for (auto& map : maps)
        log_error("  %s", map.first.c_str());

    VCML_ERROR("no such keymap: %s", name.c_str());
}

void keymap::register_keymap(const string& name,
                             const vector<syminfo>& layout) {
    if (stl_contains(maps, name))
        VCML_ERROR("keymap '%s' already registered", name.c_str());
    maps.insert({ name, keymap(layout) });
}

} // namespace ui
} // namespace vcml
