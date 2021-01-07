/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/keymap.h"

namespace vcml { namespace ui {

    vector<syminfo> keyboard_us_layout = {
        { 0xff1b, KEY_ESC,        false, false, false, "KEY_ESC"        },

        /* symbols on F1 - F12 key row */
        { 0xffbe, KEY_F1,         false, false, false, "KEY_F1"         },
        { 0xffbf, KEY_F2,         false, false, false, "KEY_F2"         },
        { 0xffc0, KEY_F3,         false, false, false, "KEY_F3"         },
        { 0xffc1, KEY_F4,         false, false, false, "KEY_F4"         },
        { 0xffc2, KEY_F5,         false, false, false, "KEY_F5"         },
        { 0xffc3, KEY_F6,         false, false, false, "KEY_F6"         },
        { 0xffc4, KEY_F7,         false, false, false, "KEY_F7"         },
        { 0xffc5, KEY_F8,         false, false, false, "KEY_F8"         },
        { 0xffc6, KEY_F9,         false, false, false, "KEY_F9"         },
        { 0xffc7, KEY_F10,        false, false, false, "KEY_F10"        },
        { 0xffc8, KEY_F11,        false, false, false, "KEY_F11"        },
        { 0xffc9, KEY_F12,        false, false, false, "KEY_F12"        },

        /* symbols on number key row */
        {    '`', KEY_GRAVE,      false, false, false, "KEY_GRAVE"      },
        { 0xfe50, KEY_GRAVE,      false, false, false, "KEY_GRAVE"      },
        {    '1', KEY_1,          false, false, false, "KEY_1"          },
        {    '2', KEY_2,          false, false, false, "KEY_2"          },
        {    '3', KEY_3,          false, false, false, "KEY_3"          },
        {    '4', KEY_4,          false, false, false, "KEY_4"          },
        {    '5', KEY_5,          false, false, false, "KEY_5"          },
        {    '6', KEY_6,          false, false, false, "KEY_6"          },
        {    '7', KEY_7,          false, false, false, "KEY_7"          },
        {    '8', KEY_8,          false, false, false, "KEY_8"          },
        {    '9', KEY_9,          false, false, false, "KEY_9"          },
        {    '0', KEY_0,          false, false, false, "KEY_0"          },
        {    '-', KEY_MINUS,      false, false, false, "KEY_MINUS"      },
        {    '=', KEY_EQUAL,      false, false, false, "KEY_EQUAL"      },
        { 0xff08, KEY_BACKSPACE,  false, false, false, "KEY_BACKSPACE"  },

        /* symbols on number key row when shift is down */
        {    '~', KEY_GRAVE,       true, false, false, "KEY_GRAVE"      },
        {    '!', KEY_1,           true, false, false, "KEY_1"          },
        {    '@', KEY_2,           true, false, false, "KEY_2"          },
        {    '#', KEY_3,           true, false, false, "KEY_3"          },
        {    '$', KEY_4,           true, false, false, "KEY_4"          },
        {    '%', KEY_5,           true, false, false, "KEY_5"          },
        {    '^', KEY_6,           true, false, false, "KEY_6"          },
        {    '&', KEY_7,           true, false, false, "KEY_7"          },
        {    '*', KEY_8,           true, false, false, "KEY_8"          },
        {    '(', KEY_9,           true, false, false, "KEY_9"          },
        {    ')', KEY_0,           true, false, false, "KEY_0"          },
        {    '_', KEY_MINUS,       true, false, false, "KEY_MINUS"      },
        {    '+', KEY_EQUAL,       true, false, false, "KEY_EQUAL"      },

        /* symbols on QWERTY row */
        { 0xff09, KEY_TAB,        false, false, false, "KEY_TAB"        },
        {    'q', KEY_Q,          false, false, false, "KEY_Q"          },
        {    'w', KEY_W,          false, false, false, "KEY_W"          },
        {    'e', KEY_E,          false, false, false, "KEY_E"          },
        {    'r', KEY_R,          false, false, false, "KEY_R"          },
        {    't', KEY_T,          false, false, false, "KEY_T"          },
        {    'y', KEY_Y,          false, false, false, "KEY_Y"          },
        {    'u', KEY_U,          false, false, false, "KEY_U"          },
        {    'i', KEY_I,          false, false, false, "KEY_I"          },
        {    'o', KEY_O,          false, false, false, "KEY_O"          },
        {    'p', KEY_P,          false, false, false, "KEY_P"          },
        {    '[', KEY_LEFTBRACE,  false, false, false, "KEY_LEFTBRACE"  },
        {    ']', KEY_RIGHTBRACE, false, false, false, "KEY_RIGHTBRACE" },
        {   '\\', KEY_BACKSLASH,  false, false, false, "KEY_BACKSLASH"  },

        /* symbols on QWERTY row when shift is down */
        {    'Q', KEY_Q,           true, false, false, "KEY_Q"          },
        {    'W', KEY_W,           true, false, false, "KEY_W"          },
        {    'E', KEY_E,           true, false, false, "KEY_E"          },
        {    'R', KEY_R,           true, false, false, "KEY_R"          },
        {    'T', KEY_T,           true, false, false, "KEY_T"          },
        {    'Y', KEY_Y,           true, false, false, "KEY_Y"          },
        {    'U', KEY_U,           true, false, false, "KEY_U"          },
        {    'I', KEY_I,           true, false, false, "KEY_I"          },
        {    'O', KEY_O,           true, false, false, "KEY_O"          },
        {    'P', KEY_P,           true, false, false, "KEY_P"          },
        {    '{', KEY_LEFTBRACE,   true, false, false, "KEY_LEFTBRACE"  },
        {    '}', KEY_RIGHTBRACE,  true, false, false, "KEY_RIGHTBRACE" },
        {    '|', KEY_BACKSLASH,   true, false, false, "KEY_BACKSLASH"  },

        /* symbols on ASDFG row */
        { 0xffe5, KEY_CAPSLOCK,   false, false, false, "KEY_CAPSLOCK"   },
        {    'a', KEY_A,          false, false, false, "KEY_A"          },
        {    's', KEY_S,          false, false, false, "KEY_S"          },
        {    'd', KEY_D,          false, false, false, "KEY_D"          },
        {    'f', KEY_F,          false, false, false, "KEY_F"          },
        {    'g', KEY_G,          false, false, false, "KEY_G"          },
        {    'h', KEY_H,          false, false, false, "KEY_H"          },
        {    'j', KEY_J,          false, false, false, "KEY_J"          },
        {    'k', KEY_K,          false, false, false, "KEY_K"          },
        {    'l', KEY_L,          false, false, false, "KEY_L"          },
        {    ';', KEY_SEMICOLON,  false, false, false, "KEY_SEMICOLON"  },
        {   '\'', KEY_APOSTROPHE, false, false, false, "KEY_APOSTROPHE" },
        { 0xff0d, KEY_ENTER,      false, false, false, "KEY_ENTER"      },

        /* symbols on ASDFG row when shift is down */
        {    'A', KEY_A,           true, false, false, "KEY_A"          },
        {    'S', KEY_S,           true, false, false, "KEY_S"          },
        {    'D', KEY_D,           true, false, false, "KEY_D"          },
        {    'F', KEY_F,           true, false, false, "KEY_F"          },
        {    'G', KEY_G,           true, false, false, "KEY_G"          },
        {    'H', KEY_H,           true, false, false, "KEY_H"          },
        {    'J', KEY_J,           true, false, false, "KEY_J"          },
        {    'K', KEY_K,           true, false, false, "KEY_K"          },
        {    'L', KEY_L,           true, false, false, "KEY_L"          },
        {    ':', KEY_SEMICOLON,   true, false, false, "KEY_SEMICOLON"  },
        {   '\"', KEY_APOSTROPHE,  true, false, false, "KEY_APOSTROPHE" },

        /* symbols on ZXCVB row */
        { 0xffe1, KEY_LEFTSHIFT,  false, false, false, "KEY_LEFTSHIFT"  },
        {    'z', KEY_Z,          false, false, false, "KEY_Z"          },
        {    'x', KEY_X,          false, false, false, "KEY_X"          },
        {    'c', KEY_C,          false, false, false, "KEY_C"          },
        {    'v', KEY_V,          false, false, false, "KEY_V"          },
        {    'b', KEY_B,          false, false, false, "KEY_B"          },
        {    'n', KEY_N,          false, false, false, "KEY_N"          },
        {    'm', KEY_M,          false, false, false, "KEY_M"          },
        {    ',', KEY_COMMA,      false, false, false, "KEY_COMMA"      },
        {    '.', KEY_DOT,        false, false, false, "KEY_DOT"        },
        {    '/', KEY_SLASH,      false, false, false, "KEY_SLASH"      },
        { 0xffe2, KEY_RIGHTSHIFT, false, false, false, "KEY_RIGHTSHIFT" },

        /* symbols on ZXCVB row when shift is down */
        {    'Z', KEY_Z,           true, false, false, "KEY_Z"          },
        {    'X', KEY_X,           true, false, false, "KEY_X"          },
        {    'C', KEY_C,           true, false, false, "KEY_C"          },
        {    'V', KEY_V,           true, false, false, "KEY_V"          },
        {    'B', KEY_B,           true, false, false, "KEY_B"          },
        {    'N', KEY_N,           true, false, false, "KEY_N"          },
        {    'M', KEY_M,           true, false, false, "KEY_M"          },
        {    '<', KEY_COMMA,       true, false, false, "KEY_COMMA"      },
        {    '>', KEY_DOT,         true, false, false, "KEY_DOT"        },
        {    '?', KEY_SLASH,       true, false, false, "KEY_SLASH"      },

        /* symbols on bottom row */
        { 0xffe3, KEY_LEFTCTRL,   false, false, false, "KEY_LEFTCTRL"   },
        { 0xffeb, KEY_LEFTMETA,   false, false, false, "KEY_LEFTMETA"   },
        { 0xffe9, KEY_LEFTALT,    false, false, false, "KEY_LEFTALT"    },
        {    ' ', KEY_SPACE,      false, false, false, "KEY_SPACE"      },
        { 0xffec, KEY_RIGHTMETA,  false, false, false, "KEY_RIGHTMETA"  },
        { 0xff67, KEY_MENU,       false, false, false, "KEY_MENU"       },
        { 0xffe4, KEY_RIGHTCTRL,  false, false, false, "KEY_RIGHTCTRL"  },

        /* extended keys */
        { 0xff61, KEY_PRINT,      false, false, false, "KEY_PRINT"      },
        { 0xff14, KEY_SCROLLLOCK, false, false, false, "KEY_SCROLLLOCK" },
        { 0xff13, KEY_PAUSE,      false, false, false, "KEY_PAUSE"      },

        { 0xff63, KEY_INSERT,     false, false, false, "KEY_INSERT"     },
        { 0xffff, KEY_DELETE,     false, false, false, "KEY_DELETE"     },
        { 0xff50, KEY_HOME,       false, false, false, "KEY_HOME"       },
        { 0xff57, KEY_END,        false, false, false, "KEY_END"        },
        { 0xff55, KEY_PAGEUP,     false, false, false, "KEY_PAGEUP"     },
        { 0xff56, KEY_PAGEDOWN,   false, false, false, "KEY_PAGEDOWN"   },

        { 0xff51, KEY_LEFT,       false, false, false, "KEY_LEFT"       },
        { 0xff52, KEY_UP,         false, false, false, "KEY_UP"         },
        { 0xff53, KEY_RIGHT,      false, false, false, "KEY_RIGHT"      },
        { 0xff54, KEY_DOWN,       false, false, false, "KEY_DOWN"       },

        /* keypad */
        { 0xff7f, KEY_NUMLOCK,    false, false, false, "KEY_NUMLOCK"    },
        { 0xff8d, KEY_KPENTER,    false, false, false, "KEY_KPENTER"    },
        { 0xffaa, KEY_KPASTERISK, false, false, false, "KEY_KPASTERISK" },
        { 0xffab, KEY_KPPLUS,     false, false, false, "KEY_KPPLUS"     },
        { 0xffac, KEY_KPDOT,      false, false, false, "KEY_KPDOT"      },
        { 0xffad, KEY_KPMINUS,    false, false, false, "KEY_KPMINUS"    },
        { 0xffaf, KEY_KPSLASH,    false, false, false, "KEY_KPSLASH"    },
        { 0xffb0, KEY_KP0,        false, false, false, "KEY_KP0"        },
        { 0xffb1, KEY_KP1,        false, false, false, "KEY_KP1"        },
        { 0xffb2, KEY_KP2,        false, false, false, "KEY_KP2"        },
        { 0xffb3, KEY_KP3,        false, false, false, "KEY_KP3"        },
        { 0xffb4, KEY_KP4,        false, false, false, "KEY_KP4"        },
        { 0xffb5, KEY_KP5,        false, false, false, "KEY_KP5"        },
        { 0xffb6, KEY_KP6,        false, false, false, "KEY_KP6"        },
        { 0xffb7, KEY_KP7,        false, false, false, "KEY_KP7"        },
        { 0xffb8, KEY_KP8,        false, false, false, "KEY_KP8"        },
        { 0xffb9, KEY_KP9,        false, false, false, "KEY_KP9"        },

        /* keypad functions */
        { 0xff9f, KEY_KPDOT,      false, false, false, "KEY_KPDOT"      },
        { 0xff9e, KEY_KP0,        false, false, false, "KEY_KP0"        },
        { 0xff9c, KEY_KP1,        false, false, false, "KEY_KP1"        },
        { 0xff99, KEY_KP2,        false, false, false, "KEY_KP2"        },
        { 0xff9b, KEY_KP3,        false, false, false, "KEY_KP3"        },
        { 0xff96, KEY_KP4,        false, false, false, "KEY_KP4"        },
        { 0xff9d, KEY_KP5,        false, false, false, "KEY_KP5"        },
        { 0xff98, KEY_KP6,        false, false, false, "KEY_KP6"        },
        { 0xff95, KEY_KP7,        false, false, false, "KEY_KP7"        },
        { 0xff97, KEY_KP8,        false, false, false, "KEY_KP8"        },
        { 0xff9a, KEY_KP9,        false, false, false, "KEY_KP9"        },
    };

    keymap::keymap(const vector<syminfo>& l):
        layout(l) {
        // sanity checking
        for (const auto info : layout) {
            if (info.code > KEY_MAX)
                VCML_ERROR("invalid key code 0x%x", info.code);
        }
    }

    const syminfo* keymap::lookup_symbol(u32 symbol) const {
        for (auto& info : layout)
            if (info.sym == symbol)
                return &info;
        return nullptr;
    }

    static bool is_reserved(u16 code) {
        switch (code) {
        case KEY_CAPSLOCK:
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            return true;

        default:
            return false;
        }
    }

    vector<u16> keymap::translate_symbol(u32 symbol) const {
        vector<u16> keys;
        const syminfo* info = lookup_symbol(symbol);
        if (info == nullptr || is_reserved(info->code))
            return keys;

        if (info->shift)
            keys.push_back(KEY_LEFTSHIFT);
        if (info->l_alt)
            keys.push_back(KEY_LEFTALT);
        if (info->r_alt)
            keys.push_back(KEY_RIGHTALT);

        keys.push_back(info->code);
        return keys;
    }

    const keymap& keymap::lookup(const string& name) {
        VCML_ERROR_ON(name.empty(), "empty keyboard layout name");

        static std::unordered_map<string, keymap> maps({
            { "us", keymap(keyboard_us_layout) }
        });

        auto it = maps.find(to_lower(name));
        if (it != maps.end())
            return it->second;

        log_error("available keymaps:");
        for (auto& map : maps)
            log_error("  %s", map.first.c_str());

        VCML_ERROR("no such keymap: %s", name.c_str());
    }

}}

