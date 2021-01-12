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

    void vnc::key_func(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
        int port = cl->screen->port;
        auto disp = display::lookup(mkstr("vnc:%d", port));
        VCML_ERROR_ON(!disp, "no vnc server found for port %d", port);
        disp->notify_key_listeners(key, down);
    }

    void vnc::ptr_func(int mask, int x, int y, rfbClientPtr cl) {
        int port = cl->screen->port;
        auto disp = display::lookup(mkstr("vnc:%d", port));
        VCML_ERROR_ON(!disp, "no vnc server found for port %d", port);
        disp->notify_ptr_listeners(mask, x, y);
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
        m_screen(),
        m_thread() {
        VCML_ERROR_ON(no != m_port, "invalid VNC port specified: %u", no);

        rfbLog = &rfb_log_func;
        rfbErr = &rfb_err_func;

        m_screen = rfbGetScreen(NULL, NULL, resx(), resy(), 8, 4, 4);
        m_screen->desktopName = name();
        m_screen->port = m_screen->ipv6port = m_port;
        m_screen->kbdAddEvent = &vnc::key_func;
        m_screen->ptrAddEvent = &vnc::ptr_func;

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
        display::render();
    }

    void vnc::shutdown() {
        if (!m_thread.joinable())
            return;

        m_running = false;
        m_thread.join();

        display::shutdown();
    }

}}
