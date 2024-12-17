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
    m_events.push(ev);
}

void input::push_key(u16 key, u32 state) {
    input_event ev = {};
    ev.type = EV_KEY;
    ev.code = key;
    ev.state = state;
    push_event(ev);
}

void input::push_rel(u16 axis, i32 delta) {
    input_event ev = {};
    ev.type = EV_REL;
    ev.code = axis;
    ev.state = delta;
    push_event(ev);
}

void input::push_abs(u16 axis, u32 abs) {
    input_event ev = {};
    ev.type = EV_ABS;
    ev.code = axis;
    ev.state = abs;
    push_event(ev);
}

void input::push_syn() {
    input_event ev = {};
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.state = 0;
    push_event(ev);
}

unordered_map<string, input*>& input::all_inputs() {
    static unordered_map<string, input*> inputs;
    return inputs;
}

input::input(const string& name): m_name(name), m_mutex(), m_events() {
    VCML_ERROR_ON(find(name), "input '%s' already exists", name.c_str());
    all_inputs()[name] = this;
}

input::~input() {
    all_inputs().erase(input_name());
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

void input::notify_key(u32 symbol, bool down) {
    lock_guard<mutex> lock(m_mutex);
    handle_key(symbol, down);
}

void input::notify_btn(u32 button, bool down) {
    lock_guard<mutex> lock(m_mutex);
    handle_btn(button, down);
}

void input::notify_pos(u32 x, u32 y) {
    lock_guard<mutex> lock(m_mutex);
    handle_pos(x, y);
}

void keyboard::handle_key(u32 sym, bool down) {
    u32 state = down ? 1 : 0;
    if (down && sym == m_prev_sym)
        state++;

    if (m_layout.empty()) {
        push_key(sym, state);
        push_syn();
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

    push_syn();
}

void keyboard::handle_btn(u32 btn, bool state) {
    // ignore mouse button events
}

void keyboard::handle_pos(u32 x, u32 y) {
    // ignore mouse move events
}

keyboard::keyboard(const string& name, const string& layout):
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
}

void mouse::handle_key(u32 sym, bool down) {
    // ignore keyboard events
}

void mouse::handle_btn(u32 button, bool down) {
    u32 buttons = m_buttons;
    if (down)
        buttons |= button;
    else
        buttons &= ~button;

    if (buttons == m_buttons)
        return;

    switch (button) {
    case BUTTON_LEFT:
        push_key(BTN_LEFT, down);
        break;
    case BUTTON_RIGHT:
        push_key(BTN_RIGHT, down);
        break;
    case BUTTON_MIDDLE:
        push_key(BTN_MIDDLE, down);
        break;
    case BUTTON_SIDE:
        push_key(BTN_SIDE, down);
        break;
    case BUTTON_EXTRA:
        push_key(BTN_EXTRA, down);
    case BUTTON_WHEEL_UP:
        push_rel(REL_WHEEL, down ? 1 : 0);
        break;
    case BUTTON_WHEEL_DOWN:
        push_rel(REL_WHEEL, down ? -1 : 0);
        break;
    case BUTTON_WHEEL_RIGHT:
        push_rel(REL_HWHEEL, down ? 1 : 0);
        break;
    case BUTTON_WHEEL_LEFT:
        push_rel(REL_HWHEEL, down ? -1 : 0);
        break;
    default:
        break;
    }

    push_syn();

    m_buttons = buttons;
}

void mouse::handle_pos(u32 xabs, u32 yabs) {
    i32 dx = xabs - m_xabs;
    i32 dy = yabs - m_yabs;

    m_xabs = xabs;
    m_yabs = yabs;

    if (dx)
        push_rel(REL_X, dx);
    if (dy)
        push_rel(REL_Y, dy);
    if (dx || dy)
        push_syn();
}

mouse::mouse(const string& name):
    input(name), m_buttons(), m_xabs(), m_yabs() {
}

void touchpad::handle_key(u32 sym, bool down) {
    // ignore keyboard events
}

void touchpad::handle_btn(u32 button, bool down) {
    u32 buttons = m_buttons;
    if (down)
        buttons |= button;
    else
        buttons &= ~button;

    if (m_buttons == buttons)
        return;

    if (buttons && !m_buttons) {
        push_key(BTN_TOUCH, true);
        push_abs(ABS_X, m_xabs);
        push_abs(ABS_Y, m_yabs);
        push_syn();
    }

    if (!buttons && m_buttons) {
        push_key(BTN_TOUCH, false);
        push_syn();
    }

    m_buttons = buttons;
}

void touchpad::handle_pos(u32 xabs, u32 yabs) {
    if (is_touching()) {
        if (xabs != m_xabs)
            push_abs(ABS_X, xabs);
        if (yabs != m_yabs)
            push_abs(ABS_Y, yabs);
        if (xabs != m_xabs || yabs != m_yabs)
            push_syn();
    }

    m_xabs = xabs;
    m_yabs = yabs;
}

touchpad::touchpad(const string& name):
    input(name), m_buttons(), m_xabs(), m_yabs() {
}

void multitouch::handle_key(u32 sym, bool down) {
    // ignore keyboard events
}

void multitouch::handle_btn(u32 button, bool down) {
    // BUTTON_LEFT   - finger 1
    // BUTTON_RIGHT  - finger 2
    // BUTTON_MIDDLE - finger 3
    // BUTTON_SIDE   - finger 4
    // BUTTON_EXTRA  - finger 5
    if (button <= BUTTON_NONE || button > BUTTON_EXTRA)
        return;

    u32 fingers = m_fingers;

    if (down)
        fingers |= button;
    else
        fingers &= ~button;

    if (m_fingers == fingers)
        return;

    for (u32 slot = 0, mask = 1; slot < 32; slot++, mask <<= 1) {
        if ((fingers & mask) && !(m_fingers & mask)) {
            push_abs(ABS_MT_SLOT, slot);
            push_abs(ABS_MT_TRACKING_ID, m_track++);
            push_abs(ABS_MT_POSITION_X, m_xabs);
            push_abs(ABS_MT_POSITION_Y, m_yabs);
            push_syn();
        }

        if (!(fingers & mask) && (m_fingers & mask)) {
            push_abs(ABS_MT_SLOT, slot);
            push_abs(ABS_MT_TRACKING_ID, -1);
            push_syn();
        }
    }

    m_fingers = fingers;
}

void multitouch::handle_pos(u32 xabs, u32 yabs) {
    for (u32 slot = 0, mask = 1; slot < 32; slot++, mask <<= 1) {
        if (m_fingers & mask) {
            if (xabs != m_xabs || yabs != m_yabs)
                push_abs(ABS_MT_SLOT, slot);
            if (xabs != m_xabs)
                push_abs(ABS_MT_POSITION_X, xabs);
            if (yabs != m_yabs)
                push_abs(ABS_MT_POSITION_Y, yabs);
            if (xabs != m_xabs || yabs != m_yabs)
                push_syn();
        }
    }

    m_xabs = xabs;
    m_yabs = yabs;
}

multitouch::multitouch(const string& name):
    input(name), m_fingers(), m_xabs(), m_yabs(), m_track(0) {
}

} // namespace ui
} // namespace vcml
