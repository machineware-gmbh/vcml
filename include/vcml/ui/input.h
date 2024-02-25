/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_INPUT_H
#define VCML_UI_INPUT_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"
#include "vcml/ui/keymap.h"

namespace vcml {
namespace ui {

enum key_state : u32 {
    VCML_KEY_UP = 0u,
    VCML_KEY_DOWN = 1u,
    VCML_KEY_HELD = 2u,
};

enum mouse_button : u32 {
    BUTTON_NONE = 0u,
    BUTTON_LEFT = 1u,
    BUTTON_MIDDLE = 2u,
    BUTTON_RIGHT = 3u,
};

enum event_type : u32 {
    EVTYPE_KEY = 1u,
    EVTYPE_REL = 2u,
};

struct input_event {
    event_type type;

    union {
        struct {
            u32 code;
            u32 state;
            u32 reserved;
        } key;

        struct {
            i32 x;
            i32 y;
            i32 w;
        } rel;
    };

    bool is_key() const { return type == EVTYPE_KEY; }
    bool is_rel() const { return type == EVTYPE_REL; }
};

class input
{
private:
    string m_name;

    mutable mutex m_mutex;
    queue<input_event> m_events;

protected:
    void push_event(const input_event& ev);
    void push_key(u32 key, u32 state);
    void push_rel(i32 x, i32 y, i32 w);

public:
    const char* input_name() const { return m_name.c_str(); }

    input(const char* name);
    virtual ~input();

    bool has_events() const;
    bool pop_event(input_event& ev);
};

class keyboard : public input
{
private:
    bool m_ctrl_l;
    bool m_ctrl_r;
    bool m_shift_l;
    bool m_shift_r;
    bool m_capsl;
    bool m_alt_l;
    bool m_alt_r;
    bool m_meta_l;
    bool m_meta_r;
    u32 m_prev_sym;
    string m_layout;

    static unordered_map<string, keyboard*> s_keyboards;

public:
    bool ctrl_l() const { return m_ctrl_l; }
    bool ctrl_r() const { return m_ctrl_r; }
    bool ctrl() const { return m_ctrl_l || m_ctrl_r; }

    bool shift_l() const { return m_shift_l; }
    bool shift_r() const { return m_shift_r; }
    bool shift() const { return m_shift_l || m_shift_r; }

    bool alt_l() const { return m_alt_l; }
    bool alt_r() const { return m_alt_r; }
    bool alt() const { return m_alt_l || m_alt_r; }

    bool meta_l() const { return m_meta_l; }
    bool meta_r() const { return m_meta_r; }
    bool meta() const { return m_meta_l || m_meta_r; }

    bool capslock() const { return m_capsl; }

    const char* layout() const { return m_layout.c_str(); }
    void set_layout(const string& layout) { m_layout = layout; }

    keyboard(const char* name, const string& layout = "");
    virtual ~keyboard();

    void notify_key(u32 symbol, bool down);

    static vector<keyboard*> all();
    static keyboard* find(const char* name);
};

class pointer : public input
{
private:
    mutable mutex m_mutex;
    queue<input_event> m_events;

    u32 m_buttons;
    u32 m_abs_x;
    u32 m_abs_y;
    u32 m_abs_w;

    static unordered_map<string, pointer*> s_pointers;

public:
    u32 x() const { return m_abs_x; }
    u32 y() const { return m_abs_y; }
    u32 w() const { return m_abs_w; }

    bool left() const { return m_buttons & BUTTON_LEFT; }
    bool middle() const { return m_buttons & BUTTON_MIDDLE; }
    bool right() const { return m_buttons & BUTTON_RIGHT; }

    pointer(const char* name);
    virtual ~pointer();

    void notify_btn(u32 btn, bool down);
    void notify_rel(i32 x, i32 y, i32 w);

    static vector<pointer*> all();
    static pointer* find(const char* name);
};

} // namespace ui
} // namespace vcml

#endif
