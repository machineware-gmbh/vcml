/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/input.h"

namespace vcml {
namespace ui {

void input::push_event(const input_event& ev) {
    lock_guard<mutex> lock(m_mutex);
    m_events.push(ev);
}

void input::push_key(u32 key, u32 state) {
    input_event ev = {};
    ev.type = EVTYPE_KEY;
    ev.key.code = key;
    ev.key.state = state;
    push_event(ev);
}

void input::push_rel(i32 x, i32 y, i32 w) {
    input_event ev = {};
    ev.type = EVTYPE_REL;
    ev.rel.x = x;
    ev.rel.y = y;
    ev.rel.w = w;
    push_event(ev);
}

input::input(const char* name): m_name(name), m_mutex(), m_events() {
}

input::~input() {
    // nothing to do
}

bool input::has_events() const {
    lock_guard<mutex> lock(m_mutex);
    return !m_events.empty();
}

bool input::pop_event(input_event& ev) {
    lock_guard<mutex> lock(m_mutex);
    if (m_events.empty())
        return false;

    ev = m_events.front();
    m_events.pop();
    return true;
}

keyboard::keyboard(const char* name, const string& layout):
    input(name),
    m_ctrl_l(false),
    m_ctrl_r(false),
    m_shift_l(false),
    m_shift_r(false),
    m_capsl(false),
    m_alt_l(false),
    m_alt_r(false),
    m_meta_l(false),
    m_meta_r(false),
    m_prev_sym(~0u),
    m_layout(layout) {
    if (stl_contains(s_keyboards, string(name)))
        VCML_ERROR("keyboard input device '%s' already exists", name);
    s_keyboards[name] = this;
}

keyboard::~keyboard() {
    s_keyboards.erase(input_name());
}

#define KEY_LEFTSHIFT  42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTALT    56
#define KEY_RIGHTALT   100
#define KEY_CAPSLOCK   58

#define BTN_TOUCH  0x14a
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

void keyboard::notify_key(u32 sym, bool down) {
    u32 state = down ? VCML_KEY_DOWN : VCML_KEY_UP;
    if (down && sym == m_prev_sym)
        state = VCML_KEY_HELD;

    if (m_layout.empty()) {
        push_key(sym, state);
        return;
    }

    const auto& map = ui::keymap::lookup(m_layout);
    auto info = map.lookup_symbol(sym);

    if (info == nullptr) {
        log_debug("no key code found for key 0x%x", sym);
        return;
    }

    if (!info->is_special() && down) {
        if ((m_shift_l ^ m_capsl) != info->shift)
            push_key(KEY_LEFTSHIFT, info->shift ^ m_capsl);
        if ((m_shift_r ^ m_capsl) != info->shift)
            push_key(KEY_RIGHTSHIFT, info->shift ^ m_capsl);
        if (m_alt_l != info->l_alt)
            push_key(KEY_LEFTALT, info->l_alt);
        if (m_alt_r != info->r_alt)
            push_key(KEY_RIGHTALT, info->r_alt);
    }

    push_key(info->code, state);

    if (!info->is_special() && down) {
        if ((m_shift_l ^ m_capsl) != info->shift)
            push_key(KEY_LEFTSHIFT, m_shift_l);
        if ((m_shift_r ^ m_capsl) != info->shift)
            push_key(KEY_RIGHTSHIFT, m_shift_r);
        if (m_alt_l != info->l_alt)
            push_key(KEY_LEFTALT, m_alt_l);
        if (m_alt_r != info->r_alt)
            push_key(KEY_RIGHTALT, m_alt_r);
    }

    if (info->code == KEY_CAPSLOCK && down)
        m_capsl = !m_capsl;
    if (info->code == KEY_LEFTSHIFT)
        m_shift_l = down;
    if (info->code == KEY_RIGHTSHIFT)
        m_shift_r = down;
    if (info->code == KEY_LEFTALT)
        m_alt_l = down;
    if (info->code == KEY_RIGHTALT)
        m_alt_r = down;
    if (info->code == KEY_LEFTCTRL)
        m_ctrl_l = down;
    if (info->code == KEY_RIGHTCTRL)
        m_ctrl_r = down;
    if (info->code == KEY_LEFTMETA)
        m_meta_l = down;
    if (info->code == KEY_RIGHTMETA)
        m_meta_r = down;

    m_prev_sym = down ? sym : -1;
}

unordered_map<string, keyboard*> keyboard::s_keyboards;

vector<keyboard*> keyboard::all() {
    vector<keyboard*> res;
    res.reserve(s_keyboards.size());
    for (const auto& it : s_keyboards)
        res.push_back(it.second);
    return res;
}

keyboard* keyboard::find(const char* name) {
    auto it = s_keyboards.find(name);
    return it != s_keyboards.end() ? it->second : nullptr;
}

pointer::pointer(const char* name):
    input(name), m_buttons(), m_abs_x(), m_abs_y(), m_abs_w() {
    if (stl_contains(s_pointers, string(name)))
        VCML_ERROR("pointer input device '%s' already exists", name);
    s_pointers[name] = this;
}

pointer::~pointer() {
    s_pointers.erase(input_name());
}

void pointer::notify_btn(u32 button, bool down) {
    if (button == BUTTON_NONE)
        return;

    u32 buttons = m_buttons;
    if (down)
        buttons |= (1u << (button - 1));
    else
        buttons &= ~(1u << (button - 1));

    if (buttons == m_buttons)
        return;

    if (buttons && !m_buttons)
        push_key(BTN_TOUCH, VCML_KEY_DOWN);

    u32 val = down ? VCML_KEY_DOWN : VCML_KEY_UP;

    switch (button) {
    case BUTTON_LEFT:
        push_key(BTN_LEFT, val);
        break;
    case BUTTON_RIGHT:
        push_key(BTN_RIGHT, val);
        break;
    case BUTTON_MIDDLE:
        push_key(BTN_MIDDLE, val);
        break;
    default:
        break;
    }

    if (!buttons && m_buttons)
        push_key(BTN_TOUCH, VCML_KEY_UP);

    m_buttons = buttons;
}

void pointer::notify_rel(i32 x, i32 y, i32 w) {
    if (x || y || w)
        push_rel(x, y, w);

    m_abs_x += x;
    m_abs_y += y;
    m_abs_w += w;
}

unordered_map<string, pointer*> pointer::s_pointers;

vector<pointer*> pointer::all() {
    vector<pointer*> res;
    res.reserve(s_pointers.size());
    for (const auto& it : s_pointers)
        res.push_back(it.second);
    return res;
}

pointer* pointer::find(const char* name) {
    auto it = s_pointers.find(name);
    return it != s_pointers.end() ? it->second : nullptr;
}

} // namespace ui
} // namespace vcml
