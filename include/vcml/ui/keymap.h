/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_KEYMAP_H
#define VCML_UI_KEYMAP_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"
#include "vcml/ui/codes.h"

namespace vcml {
namespace ui {

enum keysym : u32 {
    KEYSYM_NONE = 0,

    KEYSYM_1 = '1',
    KEYSYM_2 = '2',
    KEYSYM_3 = '3',
    KEYSYM_4 = '4',
    KEYSYM_5 = '5',
    KEYSYM_6 = '6',
    KEYSYM_7 = '7',
    KEYSYM_8 = '8',
    KEYSYM_9 = '9',
    KEYSYM_0 = '0',

    KEYSYM_A = 'A',
    KEYSYM_B = 'B',
    KEYSYM_C = 'C',
    KEYSYM_D = 'D',
    KEYSYM_E = 'E',
    KEYSYM_F = 'F',
    KEYSYM_G = 'G',
    KEYSYM_H = 'H',
    KEYSYM_I = 'I',
    KEYSYM_J = 'J',
    KEYSYM_K = 'K',
    KEYSYM_L = 'L',
    KEYSYM_M = 'M',
    KEYSYM_N = 'N',
    KEYSYM_O = 'O',
    KEYSYM_P = 'P',
    KEYSYM_Q = 'Q',
    KEYSYM_R = 'R',
    KEYSYM_S = 'S',
    KEYSYM_T = 'T',
    KEYSYM_U = 'U',
    KEYSYM_V = 'V',
    KEYSYM_W = 'W',
    KEYSYM_X = 'X',
    KEYSYM_Y = 'Y',
    KEYSYM_Z = 'Z',
    KEYSYM_a = 'a', // NOLINT(readability-identifier-naming)
    KEYSYM_b = 'b', // NOLINT(readability-identifier-naming)
    KEYSYM_c = 'c', // NOLINT(readability-identifier-naming)
    KEYSYM_d = 'd', // NOLINT(readability-identifier-naming)
    KEYSYM_e = 'e', // NOLINT(readability-identifier-naming)
    KEYSYM_f = 'f', // NOLINT(readability-identifier-naming)
    KEYSYM_g = 'g', // NOLINT(readability-identifier-naming)
    KEYSYM_h = 'h', // NOLINT(readability-identifier-naming)
    KEYSYM_i = 'i', // NOLINT(readability-identifier-naming)
    KEYSYM_j = 'j', // NOLINT(readability-identifier-naming)
    KEYSYM_k = 'k', // NOLINT(readability-identifier-naming)
    KEYSYM_l = 'l', // NOLINT(readability-identifier-naming)
    KEYSYM_m = 'm', // NOLINT(readability-identifier-naming)
    KEYSYM_n = 'n', // NOLINT(readability-identifier-naming)
    KEYSYM_o = 'o', // NOLINT(readability-identifier-naming)
    KEYSYM_p = 'p', // NOLINT(readability-identifier-naming)
    KEYSYM_q = 'q', // NOLINT(readability-identifier-naming)
    KEYSYM_r = 'r', // NOLINT(readability-identifier-naming)
    KEYSYM_s = 's', // NOLINT(readability-identifier-naming)
    KEYSYM_t = 't', // NOLINT(readability-identifier-naming)
    KEYSYM_u = 'u', // NOLINT(readability-identifier-naming)
    KEYSYM_v = 'v', // NOLINT(readability-identifier-naming)
    KEYSYM_w = 'w', // NOLINT(readability-identifier-naming)
    KEYSYM_x = 'x', // NOLINT(readability-identifier-naming)
    KEYSYM_y = 'y', // NOLINT(readability-identifier-naming)
    KEYSYM_z = 'z', // NOLINT(readability-identifier-naming)

    KEYSYM_EXCLAIM = '!',
    KEYSYM_DBLQUOTE = '"',
    KEYSYM_HASH = '#',
    KEYSYM_DOLLAR = '$',
    KEYSYM_PERCENT = '%',
    KEYSYM_AMPERSAND = '&',
    KEYSYM_QUOTE = '\'',
    KEYSYM_LEFTPAR = '(',
    KEYSYM_RIGHTPAR = ')',
    KEYSYM_ASTERISK = '*',
    KEYSYM_PLUS = '+',
    KEYSYM_COMMA = ',',
    KEYSYM_MINUS = '-',
    KEYSYM_DOT = '.',
    KEYSYM_SLASH = '/',
    KEYSYM_COLON = ':',
    KEYSYM_SEMICOLON = ';',
    KEYSYM_LESS = '<',
    KEYSYM_EQUAL = '=',
    KEYSYM_GREATER = '>',
    KEYSYM_QUESTION = '?',
    KEYSYM_AT = '@',
    KEYSYM_LEFTBRACKET = '[',
    KEYSYM_BACKSLASH = '\\',
    KEYSYM_RIGHTBRACKET = ']',
    KEYSYM_CARET = '^',
    KEYSYM_UNDERSCORE = '_',
    KEYSYM_BACKQUOTE = '`',
    KEYSYM_LEFTBRACE = '{',
    KEYSYM_PIPE = '|',
    KEYSYM_RIGHTBRACE = '}',
    KEYSYM_TILDE = '~',

    KEYSYM_SPECIAL = 1u << 31,

    KEYSYM_ESC,
    KEYSYM_ENTER,
    KEYSYM_BACKSPACE,
    KEYSYM_SPACE,
    KEYSYM_TAB,
    KEYSYM_LEFTSHIFT,
    KEYSYM_RIGHTSHIFT,
    KEYSYM_LEFTCTRL,
    KEYSYM_RIGHTCTRL,
    KEYSYM_LEFTALT,
    KEYSYM_RIGHTALT,
    KEYSYM_LEFTMETA,
    KEYSYM_RIGHTMETA,
    KEYSYM_MENU,
    KEYSYM_CAPSLOCK,

    KEYSYM_F1,
    KEYSYM_F2,
    KEYSYM_F3,
    KEYSYM_F4,
    KEYSYM_F5,
    KEYSYM_F6,
    KEYSYM_F7,
    KEYSYM_F8,
    KEYSYM_F9,
    KEYSYM_F10,
    KEYSYM_F11,
    KEYSYM_F12,

    KEYSYM_PRINT,
    KEYSYM_SCROLLOCK,
    KEYSYM_PAUSE,

    KEYSYM_INSERT,
    KEYSYM_DELETE,
    KEYSYM_HOME,
    KEYSYM_END,
    KEYSYM_PAGEUP,
    KEYSYM_PAGEDOWN,

    KEYSYM_LEFT,
    KEYSYM_RIGHT,
    KEYSYM_UP,
    KEYSYM_DOWN,

    KEYSYM_NUMLOCK,
    KEYSYM_KP0,
    KEYSYM_KP1,
    KEYSYM_KP2,
    KEYSYM_KP3,
    KEYSYM_KP4,
    KEYSYM_KP5,
    KEYSYM_KP6,
    KEYSYM_KP7,
    KEYSYM_KP8,
    KEYSYM_KP9,
    KEYSYM_KPENTER,
    KEYSYM_KPPLUS,
    KEYSYM_KPMINUS,
    KEYSYM_KPMUL,
    KEYSYM_KPDIV,
    KEYSYM_KPDOT,
    KEYSYM_KPUP,
    KEYSYM_KPDOWN,
    KEYSYM_KPLEFT,
    KEYSYM_KPRIGHT,
    KEYSYM_KPHOME,
    KEYSYM_KPEND,
    KEYSYM_KPPAGEUP,
    KEYSYM_KPPAGEDOWN,
    KEYSYM_KPINSERT,
    KEYSYM_KPDELETE,
};

struct syminfo {
    u32 keysym; // vcml key symbol (check above)
    u32 code;   // linux key code (depends on keyboard layout)
    bool shift; // additionally needs shift to produce key symbol
    bool l_alt; // additionally needs alt to produce key symbol
    bool r_alt; // additionally needs altgr to produce key symbol

    bool is_special() const { return keysym > KEYSYM_SPECIAL; }
};

class keymap
{
private:
    static unordered_map<string, keymap> maps;

    keymap(const vector<syminfo>& layout);

public:
    const vector<syminfo>& layout;

    keymap() = delete;
    keymap(const keymap&) = default;
    keymap(keymap&&) = default;

    const syminfo* lookup_symbol(u32 symbol) const;

    static const keymap& lookup(const string& name);
    static void register_keymap(const string& name,
                                const vector<syminfo>& layout);
};

} // namespace ui
} // namespace vcml

#endif
