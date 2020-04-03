/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include <linux/input.h>

#include "vcml/common/thctl.h"
#include "vcml/models/opencores/ockbd.h"

#define MOD_RELEASE (1 << 7)

namespace vcml { namespace opencores {

    struct keyinfo {
        u32  key;   /* key symbol (e.g. 'A' or '\t') from vnc */
        u8   code;  /* raw scan code of key (depends on keyboard layout) */
        bool shift; /* additionally needs shift to produce key symbol */
        bool alt;   /* additionally needs alt to produce key symbol */
        bool altgr; /* additionally needs altgr to produce key symbol */
        const char* name;
    };

    vector<keyinfo> keyboard_us_layout = {
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
        { 0xffe5, KEY_RESERVED,   false, false, false, "KEY_CAPSLOCK"   },
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
        { 0xffe1, KEY_RESERVED,   false, false, false, "KEY_LEFTSHIFT"  },
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
        { 0xffe2, KEY_RESERVED,   false, false, false, "KEY_RIGHTSHIFT" },

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
        { 0xffe9, KEY_RESERVED,   false, false, false, "KEY_LEFTALT"    },
        {    ' ', KEY_SPACE,      false, false, false, "KEY_SPACE"      },
        { 0xffec, KEY_RIGHTMETA,  false, false, false, "KEY_RIGHTMETA"  },
        { 0xff67, KEY_MENU,       false, false, false, "KEY_MENU"       },

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
        { 0xff7f, KEY_RESERVED,   false, false, false, "KEY_NUMLOCK"    },
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
    };

    static const keyinfo* lookup_key(u32 key, const vector<keyinfo>& layout) {
        for (auto & info : layout)
            if (info.key == key)
                return &info;
        return NULL;
    }

    void ockbd::key_event(u32 key, bool down) {
        const keyinfo* info = lookup_key(key, keyboard_us_layout);
        if (info == NULL) {
            if (down)
                log_debug("no scancode for key 0x%x", key, key);
            return;
        }

        if (down)
            log_debug("found scancode for key 0x%x: %s", key, info->name);

        if (info->code == KEY_RESERVED)
            return;

        if (info->shift)
            push_key(KEY_LEFTSHIFT, down);
        if (info->alt)
            push_key(KEY_LEFTALT, down);
        if (info->altgr)
            push_key(KEY_RIGHTALT, down);

        push_key(info->code, down);
    }

    void ockbd::push_key(u8 scancode, bool down) {
        if (!down)
            scancode |= MOD_RELEASE;

        thctl_enter_critical();

        if (m_key_fifo.size() < fifosize)
            m_key_fifo.push(scancode);
        else if (down)
            log_debug("FIFO full, dropping key 0x%x", scancode);

        if (!IRQ && !m_key_fifo.empty())
            log_debug("setting IRQ");

        IRQ = !m_key_fifo.empty();

        thctl_exit_critical();
    }

    u8 ockbd::read_KHR() {
        VCML_ERROR_ON(IRQ && m_key_fifo.empty(), "IRQ without data");

        if (m_key_fifo.empty()) {
            log_debug("read KHR without data and interrupt");
            return 0;
        }

        u8 key = m_key_fifo.front();
        m_key_fifo.pop();

        log_debug("cpu fetched key 0x%x from KHR, %d keys remaining",
                  (int)key, (int)m_key_fifo.size());
        if (IRQ && m_key_fifo.empty())
            log_debug("clearing IRQ");

        IRQ = !m_key_fifo.empty();
        return key;
    }

    ockbd::ockbd(const sc_module_name& nm):
        peripheral(nm),
        m_key_fifo(),
        m_key_handler(),
        KHR("KHR", 0x0, 0),
        IRQ("IRQ"),
        IN("IN"),
        fifosize("fifosize", 16),
        vncport("vncport", 0) {

        KHR.allow_read();
        KHR.read = &ockbd::read_KHR;

        using std::placeholders::_1;
        using std::placeholders::_2;
        m_key_handler = std::bind(&ockbd::key_event, this, _1, _2);

#ifdef HAVE_LIBVNC
        if (vncport > 0) {
            shared_ptr<debugging::vncserver> vnc =
                    debugging::vncserver::lookup(vncport);
            vnc->add_key_listener(&m_key_handler);
        }
#endif
    }

    ockbd::~ockbd() {
#ifdef HAVE_LIBVNC
        if (vncport > 0) {
            shared_ptr<debugging::vncserver> vnc =
                    debugging::vncserver::lookup(vncport);
            vnc->remove_key_listener(&m_key_handler);
        }
#endif
    }

}}
