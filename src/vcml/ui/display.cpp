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

#include "vcml/ui/display.h"

#ifdef HAVE_LIBVNC
#include "vcml/ui/vnc.h"
#endif

#ifdef HAVE_SDL2
#include "vcml/ui/sdl.h"
#endif

namespace vcml { namespace ui {

    void display::key_listener_state::notify_key(u32 sym, bool down) {
        u32 state = down ? VCML_KEY_DOWN : VCML_KEY_UP;
        if (down && sym == prev_sym)
            state = VCML_KEY_HELD;

        if (layout == "") {
            dokey(sym, state);
            return;
        }

        const auto& map = ui::keymap::lookup(layout);
        auto info = map.lookup_symbol(sym);

        if (info == nullptr) {
            log_debug("no key code found for key 0x%x", sym);
            return;
        }

        if (!info->is_special()) {
            if (down && (shift_l ^ capsl) != info->shift)
                dokey(KEY_LEFTSHIFT, info->shift ^ capsl);
            if (down && (shift_r ^ capsl) != info->shift)
                dokey(KEY_RIGHTSHIFT, info->shift ^ capsl);
            if (down && alt_l != info->l_alt)
                dokey(KEY_LEFTALT, info->l_alt);
            if (down && alt_r != info->r_alt)
                dokey(KEY_RIGHTALT, info->r_alt);
        }

        dokey(info->code, state);

        if (!info->is_special()) {
            if (down && (shift_l ^ capsl) != info->shift)
                dokey(KEY_LEFTSHIFT, !(info->shift ^ capsl));
            if (down && (shift_r ^ capsl) != info->shift)
                dokey(KEY_RIGHTSHIFT, !(info->shift ^ capsl));
            if (down && alt_l != info->l_alt)
                dokey(KEY_LEFTALT, alt_l);
            if (down && alt_r != info->r_alt)
                dokey(KEY_RIGHTALT, alt_r);
        }

        if (info->code == KEY_CAPSLOCK && down)
            capsl = !capsl;
        if (info->code == KEY_LEFTSHIFT)
            shift_l = down;
        if (info->code == KEY_RIGHTSHIFT)
            shift_r = down;
        if (info->code == KEY_LEFTALT)
            alt_l = down;
        if (info->code == KEY_RIGHTALT)
            alt_r = down;

        prev_sym = down ? sym : -1;
    }

    void display::ptr_listener_state::notify_btn(u32 button, bool down) {
        if (button == BUTTON_NONE)
            return;

        u32 state = buttons;
        if (down)
            state |=  (1u << (button - 1));
        else
            state &= ~(1u << (button - 1));

        if (state == buttons)
            return;

        if (state && !buttons)
            dobtn(BTN_TOUCH, VCML_KEY_DOWN);

        u32 val = down ? VCML_KEY_DOWN : VCML_KEY_UP;

        switch (button) {
        case BUTTON_LEFT: (*btnev)(BTN_TOOL_FINGER, val); break;
        case BUTTON_RIGHT: (*btnev)(BTN_TOOL_DOUBLETAP, val); break;
        case BUTTON_MIDDLE: (*btnev)(BTN_TOOL_TRIPLETAP, val); break;
        default:
            break;
        }

        if (!state && buttons)
            dobtn(BTN_TOUCH, VCML_KEY_UP);

        buttons = state;
    }

    void display::ptr_listener_state::notify_pos(u32 x, u32 y) {
        if (posev != nullptr && (x != prev_x || y != prev_y))
            (*posev)(x, y);

        prev_x = x;
        prev_y = y;
    }

    unordered_map<string, shared_ptr<display>> display::displays = {
        { "", shared_ptr<display>(new display("", 0)) } // no-op server
    };

    display::display(const string& type, u32 nr):
        m_name(mkstr("%s:%u", type.c_str(), nr)),
        m_type(type),
        m_dispno(nr),
        m_mode(),
        m_fb(nullptr) {
    }

    display::~display() {
        // nothing to do
    }

    void display::init(const fbmode& mode, u8* fbptr) {
        m_mode = mode;
        m_fb = fbptr;
    }

    void display::render() {
        // nothing to do
    }

    void display::shutdown() {
        // nothing to do
    }

    void display::notify_key(u32 keysym, bool down) {
        if (!thctl_is_sysc_thread())
            thctl_enter_critical();

        for (auto& listener : m_key_listeners)
            listener.notify_key(keysym, down);

        if (!thctl_is_sysc_thread())
            thctl_exit_critical();
    }

    void display::notify_btn(u32 button, bool down) {
        if (!thctl_is_sysc_thread())
            thctl_enter_critical();

        for (auto& listener : m_ptr_listeners)
            listener.notify_btn(button, down);

        if (!thctl_is_sysc_thread())
            thctl_exit_critical();
    }

    void display::notify_pos(u32 x, u32 y) {
        if (!thctl_is_sysc_thread())
            thctl_enter_critical();

        for (auto& listener : m_ptr_listeners)
            listener.notify_pos(x, y);

        if (!thctl_is_sysc_thread())
            thctl_exit_critical();
    }

    void display::add_key_listener(key_listener& l, const string& layout) {
        m_key_listeners.push_back(key_listener_state(&l, layout));
    }

    void display::add_ptr_listener(pos_listener& p, key_listener& k) {
        m_ptr_listeners.push_back(ptr_listener_state(&p, &k));
    }

    void display::remove_key_listener(key_listener& l) {
        stl_remove_erase_if(m_key_listeners, [l](key_listener_state& st) {
            return st.keyev == &l;
        });
    }

    void display::remove_ptr_listener(pos_listener& p, key_listener& k) {
        stl_remove_erase_if(m_ptr_listeners, [p, k](ptr_listener_state& st) {
            return st.posev == &p || st.btnev == &k;
        });
    }

    static bool parse_display(const string& name, string& id, u32& no) {
        auto it = name.rfind(":");
        if (it == string::npos)
            return false;

        id = name.substr(0, it);
        no = from_string<u32>(name.substr(it + 1, string::npos));

        return true;
    }

    shared_ptr<display> display::lookup(const string& name) {
        shared_ptr<display>& disp = displays[name];
        if (disp != nullptr)
            return disp;

        u32 no;
        string id;

        if (!parse_display(name, id, no))
            VCML_ERROR("cannot parse display name: %s", name.c_str());

        if (id == "display")
            disp.reset(new display("display", no));

        if (id == "vnc") {
#ifdef HAVE_LIBVNC
            disp.reset(new vnc(no));
#else
            disp.reset(new display("display", no));
#endif
        }

        if (id == "sdl") {
#ifdef HAVE_SDL2
            disp.reset(new sdl(no));
#else
            disp.reset(new display("display", no));
#endif
        }

        if (disp == nullptr)
            VCML_ERROR("display type '%s' not supported", id.c_str());

        return disp;
    }

}}
