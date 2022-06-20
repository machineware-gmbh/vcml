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

#ifndef VCML_UI_INPUT_H
#define VCML_UI_INPUT_H

#include "vcml/core/types.h"
#include "vcml/core/strings.h"
#include "vcml/core/report.h"

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
    EVTYPE_PTR = 2u,
};

struct input_event {
    event_type type;

    union {
        struct {
            u32 code;
            u32 state;
        } key;

        struct {
            u32 x;
            u32 y;
        } ptr;
    };

    bool is_key() const { return type == EVTYPE_KEY; }
    bool is_ptr() const { return type == EVTYPE_PTR; }
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
    void push_ptr(u32 x, u32 y);

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
    bool m_shift_l;
    bool m_shift_r;
    bool m_capsl;
    bool m_alt_l;
    bool m_alt_r;
    u32 m_prev_sym;
    string m_layout;

    static unordered_map<string, keyboard*> s_keyboards;

public:
    bool shift_l() const { return m_shift_l; }
    bool shift_r() const { return m_shift_r; }
    bool shift() const { return m_shift_l || m_shift_r; }

    bool alt_l() const { return m_alt_l; }
    bool alt_r() const { return m_alt_r; }
    bool alt() const { return m_alt_l || m_alt_r; }

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
    u32 m_prev_x;
    u32 m_prev_y;

    static unordered_map<string, pointer*> s_pointers;

public:
    u32 x() const { return m_prev_x; }
    u32 y() const { return m_prev_y; }

    bool left() const { return m_buttons & BUTTON_LEFT; }
    bool middle() const { return m_buttons & BUTTON_MIDDLE; }
    bool right() const { return m_buttons & BUTTON_RIGHT; }

    pointer(const char* name);
    virtual ~pointer();

    void notify_btn(u32 btn, bool down);
    void notify_pos(u32 x, u32 y);

    static vector<pointer*> all();
    static pointer* find(const char* name);
};

} // namespace ui
} // namespace vcml

#endif
