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

#include "vcml/ui/vnc.h"

namespace vcml { namespace ui {

    static void rfb_log_func(const char* format, ...) {
        va_list args;
        va_start(args, format);
        string str = vmkstr(format, args);
        va_end(args);
        log_debug(trim(str).c_str());
    }

    static void rfb_err_func(const char* format, ...) {
        va_list args;
        va_start(args, format);
        string str = vmkstr(format, args);
        va_end(args);
        log_error(trim(str).c_str());
    }

    static void rfb_key_func(rfbBool down, rfbKeySym sym, rfbClientPtr cl) {
        int port = cl->screen->port;
        auto disp = display::lookup(mkstr("vnc:%d", port));
        VCML_ERROR_ON(!disp, "no display found for port %d", port);
        auto vnc_ = dynamic_cast<vnc*>(disp.get());
        VCML_ERROR_ON(!vnc_, "no vnc server found for port %d", port);
        vnc_->key_event((u32)sym, (bool)down);
    }

    static void rfb_ptr_func(int mask, int x, int y, rfbClientPtr cl) {
        int port = cl->screen->port;
        auto disp = display::lookup(mkstr("vnc:%d", port));
        VCML_ERROR_ON(!disp, "no display found for port %d", port);
        auto vnc_ = dynamic_cast<vnc*>(disp.get());
        VCML_ERROR_ON(!vnc_, "no vnc server found for port %d", port);
        vnc_->ptr_event((u32)mask, (u32)x, (u32)y);
    }

    void vnc::vnc_key_listener::notify(u32 sym, bool down) {
        const auto& map = ui::keymap::lookup(layout);
        auto info = map.lookup_symbol(sym);

        u32 state = down ? VCML_KEY_DOWN : VCML_KEY_UP;
        if (down && sym == prev_sym)
            state = VCML_KEY_HELD;

        if (info == nullptr) {
            log_debug("no key code found for key 0x%x", sym);
            return;
        }

        if (!info->is_special()) {
            if (down && (shift ^ capsl) != info->shift)
                notify_key(KEY_LEFTSHIFT, info->shift ^ capsl);
            if (down && alt_l != info->l_alt)
                notify_key(KEY_LEFTALT, info->l_alt);
            if (down && alt_r != info->r_alt)
                notify_key(KEY_RIGHTALT, info->r_alt);
        }

        notify_key(info->code, state);

        if (!info->is_special()) {
            if (down && (shift ^ capsl) != info->shift)
                notify_key(KEY_LEFTSHIFT, !(info->shift ^ capsl));
            if (down && alt_l != info->l_alt)
                notify_key(KEY_LEFTALT, alt_l);
            if (down && alt_r != info->r_alt)
                notify_key(KEY_RIGHTALT, alt_r);
        }

        if (info->code == KEY_CAPSLOCK && down)
            capsl = !capsl;
        if (info->code == KEY_LEFTSHIFT || info->code == KEY_RIGHTSHIFT)
            shift = down;
        if (info->code == KEY_LEFTALT)
            alt_l = down;
        if (info->code == KEY_RIGHTALT)
            alt_r = down;

        prev_sym = down ? sym : -1;
    }

    void vnc::vnc_ptr_listener::notify(u32 buttons, u32 x, u32 y) {
        u32 status = buttons & 0b111; // lclick, mclick, rclick
        u32 change = status ^ prev_buttons;

        if (change)
            notify_btn(BTN_TOUCH, !prev_buttons);

        if (change & (1u << 0))
            notify_btn(BTN_TOOL_FINGER, (status >> 0) & 1u);
        if (change & (1u << 1))
            notify_btn(BTN_TOOL_TRIPLETAP, (status >> 1) & 1u);
        if (change & (1u << 2))
            notify_btn(BTN_TOOL_DOUBLETAP, (status >> 2) & 1u);

        if (prev_x != x || prev_y != y)
            notify_ptr(x, y);

        prev_buttons = status;
        prev_x = x;
        prev_y = y;
    }

    void vnc::run() {
        log_debug("starting vnc server on port %d", m_screen->port);

        while (m_running && rfbIsActive(m_screen))
            rfbProcessEvents(m_screen, 1000);

        log_debug("terminating vnc server on port %d", m_screen->port);

        rfbShutdownServer(m_screen, true);
        rfbScreenCleanup(m_screen);
    }

    vnc::vnc(u32 no):
        display("vnc", no),
        m_port(no), // vnc port = display number
        m_running(true),
        m_mutex(),
        m_screen(),
        m_thread() {
        VCML_ERROR_ON(no != (u32)m_port, "invalid port specified: %u", no);

        rfbLog = &rfb_log_func;
        rfbErr = &rfb_err_func;

        m_screen = rfbGetScreen(NULL, NULL, resx(), resy(), 8, 4, 4);
        m_screen->desktopName = name();
        m_screen->port = m_screen->ipv6port = m_port;
        m_screen->kbdAddEvent = &rfb_key_func;
        m_screen->ptrAddEvent = &rfb_ptr_func;

        rfbInitServer(m_screen);

        m_thread = thread(&vnc::run, this);
        set_thread_name(m_thread, name());
    }

    vnc::~vnc() {
        VCML_ERROR_ON(m_thread.joinable(), "vnc %s not shut down", name());
    }

    void vnc::init(const fbmode& mode, u8* fb)  {
        display::init(mode, fb);

        m_screen->frameBuffer = (char*)fb;

        u8 thebits = mode.a.offset + mode.a.size;
        thebits = max<u8>(thebits, mode.r.size + mode.r.offset);
        thebits = max<u8>(thebits, mode.g.size + mode.g.offset);
        thebits = max<u8>(thebits, mode.b.size + mode.b.offset);

        u32 samples = 0;
        if (mode.a.size > 0)
            samples++;
        if (mode.r.size > 0)
            samples++;
        if (mode.g.size > 0)
            samples++;
        if (mode.g.size > 0)
            samples++;

        rfbNewFramebuffer(m_screen, m_screen->frameBuffer, resx(), resy(),
                          mode.r.size, samples, thebits / 8);

        m_screen->serverFormat.redShift   = mode.r.offset;
        m_screen->serverFormat.greenShift = mode.g.offset;
        m_screen->serverFormat.blueShift  = mode.b.offset;

        m_screen->serverFormat.redMax   = (1 << mode.r.size) - 1;
        m_screen->serverFormat.greenMax = (1 << mode.g.size) - 1;
        m_screen->serverFormat.blueMax  = (1 << mode.b.size) - 1;

        m_screen->serverFormat.bitsPerPixel = thebits;
        m_screen->serverFormat.bigEndian = mode.endian == VCML_ENDIAN_BIG;
    }

    void vnc::render() {
        int x2 = resx() - 1;
        int y2 = resy() - 1;
        rfbMarkRectAsModified(m_screen, 0, 0, x2, y2);
    }

    void vnc::shutdown() {
        if (!m_thread.joinable())
            return;

        m_running = false;
        m_thread.join();
    }

    void vnc::add_key_listener(key_listener& l, const string& layout) {
        lock_guard<mutex> lock(m_mutex);

        m_key_listener.push_back(vnc_key_listener(&l, layout));
    }

    void vnc::add_ptr_listener(pos_listener& ptr, key_listener& key) {
        lock_guard<mutex> lock(m_mutex);

        m_ptr_listener.push_back(vnc_ptr_listener(&ptr, &key));
    }

    void vnc::remove_key_listener(key_listener& l) {
        lock_guard<mutex> lock(m_mutex);

        stl_remove_erase_if(m_key_listener, [l](vnc_key_listener& vl) {
            return vl.keyev == &l;
        });
    }

    void vnc::remove_ptr_listener(pos_listener& p, key_listener& k) {
        lock_guard<mutex> lock(m_mutex);

        stl_remove_erase_if(m_ptr_listener, [p, k](vnc_ptr_listener& vl) {
            return vl.ptrev == &p || vl.btnev == &k;
        });
    }

    void vnc::key_event(u32 sym, bool down) {
        lock_guard<mutex> lock(m_mutex);

        thctl_enter_critical();
        for (auto& listener : m_key_listener)
            listener.notify(sym, down);
        thctl_exit_critical();
    }

    void vnc::ptr_event(u32 mask, u32 x, u32 y) {
        lock_guard<mutex> lock(m_mutex);

        thctl_enter_critical();
        for (auto& listener : m_ptr_listener)
            listener.notify(mask, x, y);
        thctl_exit_critical();
    }

}}
