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

#ifndef HAVE_LIBVNC
#error This unit requires libvcl to compile
#endif

#include "vcml/debugging/vncserver.h"

namespace vcml { namespace debugging {

    vnc_fbmode fbmode_argb32(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = width * height * 4;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset = 24;
        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    vnc_fbmode fbmode_bgra32(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = width * height * 4;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset =  0;
        mode.r.offset =  8;
        mode.g.offset = 16;
        mode.b.offset = 24;

        mode.endian = host_endian();

        return mode;
    }

    vnc_fbmode fbmode_rgb24(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = width * height * 3;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    vnc_fbmode fbmode_bgr24(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = width * height * 3;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset =  0;
        mode.g.offset =  8;
        mode.b.offset = 16;

        mode.endian = host_endian();

        return mode;
    }

    vnc_fbmode fbmode_rgb16(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = width * height * 2;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = 5;
        mode.g.size = 6;
        mode.b.size = 5;

        mode.r.offset = 11;
        mode.g.offset = 5;
        mode.b.offset = 0;

        mode.endian = host_endian();

        return mode;
    }

    vnc_fbmode fbmode_gray8(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = width * height;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = 8;
        mode.g.size = 8;
        mode.b.size = 8;

        mode.r.offset = 0;
        mode.g.offset = 0;
        mode.b.offset = 0;

        mode.endian = host_endian();

        return mode;
    }

    std::map<u16, shared_ptr<vncserver>> vncserver::servers;

    static void vncserver_log_func(const char* format, ...) {
        va_list args;
        va_start(args, format);
        string str = vmkstr(format, args);
        va_end(args);

        log_debug(str.c_str());
    }

    static void vncserver_err_func(const char* format, ...) {
        va_list args;
        va_start(args, format);
        string str = vmkstr(format, args);
        va_end(args);

        log_error(str.c_str());
    }

    void* vncserver::thread_func(void* arg) {
        vncserver* server = (vncserver*)arg;
        server->run();
        return NULL;
    }

    void vncserver::key_func(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
        int port = cl->screen->port;
        shared_ptr<vncserver> server = vncserver::servers[port];
        VCML_ERROR_ON(server == NULL, "no vnc server found for port %d", port);
        server->dokey(key, down != 0);
    }

    void vncserver::run() {
        log_debug("starting vnc server on port %d", m_screen->port);
        while (m_running && rfbIsActive(m_screen))
            rfbProcessEvents(m_screen, 1000);
        rfbShutdownServer(m_screen, false);
    }

    void vncserver::dokey(unsigned int key, bool down) {
        for (auto handler : m_key_handler)
            (*handler)(key, down);
    }

    vncserver::vncserver(u16 port):
        m_screen(NULL),
        m_thread(),
        m_running(true),
        m_fbmode(fbmode_argb32(800, 600)),
        m_fb(NULL),
        m_key_handler() {
        VCML_ERROR_ON(port == 0, "invalid port specified: %d", (int)port);

        rfbLog = &vncserver_log_func;
        rfbErr = &vncserver_err_func;

        int w = m_fbmode.resx;
        int h = m_fbmode.resy;

        m_screen = rfbGetScreen(NULL, NULL, w, h, 8, 4, 4);
        m_screen->port = m_screen->ipv6port = port;
        m_screen->kbdAddEvent = &vncserver::key_func;

        // setup green initial framebuffer
        m_fb = setup_framebuffer(m_fbmode);
        for (u32 i = 1; i < m_fbmode.size; i += 4)
            m_fb[i] = 0xff;

        rfbInitServer(m_screen);

        VCML_ERROR_ON(servers[port], "vnc port already used %d", (int)port);

        if (pthread_create(&m_thread, NULL, &vncserver::thread_func, this))
            VCML_ERROR("failed to create vnc server thread");

        stringstream ss; ss << "vnc_" << port;
        if (pthread_setname_np(m_thread, ss.str().c_str()))
            VCML_ERROR("failed to name vnc server thread");
    }

    vncserver::~vncserver() {
        log_debug("destroying vnc server at port %d", m_screen->port);
        m_running = false;
        pthread_join(m_thread, NULL);
        rfbScreenCleanup(m_screen);
        if (m_fb)
            delete [] m_fb;
    }

    void vncserver::add_key_listener(function<void(u32, bool)>* handler) {
        if (stl_contains(m_key_handler, handler))
            VCML_ERROR("key handler already registered");
        m_key_handler.push_back(handler);
    }

    void vncserver::remove_key_listener(function<void(u32, bool)>* handler) {
        if (!stl_contains(m_key_handler, handler))
            VCML_ERROR("attempt to remove unknown key handler");
        stl_remove_erase(m_key_handler, handler);
    }

    u8* vncserver::setup_framebuffer(const vnc_fbmode& desc) {
        u8* fb = new u8[desc.size];
        setup_framebuffer(desc, fb);
        m_fb = fb;
        return fb;
    }

    void vncserver::setup_framebuffer(const vnc_fbmode& mode, u8* ptr) {
        VCML_ERROR_ON(ptr == NULL, "attempt to map NULL as framebuffer");

        if (m_fb != NULL) {
            delete [] m_fb;
            m_fb = NULL;
        }

        m_fbmode = mode;
        m_screen->frameBuffer = (char*)ptr;

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

        rfbNewFramebuffer(m_screen, m_screen->frameBuffer, m_fbmode.resx,
                          m_fbmode.resy, mode.r.size, samples, thebits / 8);

        m_screen->serverFormat.redShift   = mode.r.offset;
        m_screen->serverFormat.greenShift = mode.g.offset;
        m_screen->serverFormat.blueShift  = mode.b.offset;

        m_screen->serverFormat.redMax   = (1 << mode.r.size) - 1;
        m_screen->serverFormat.greenMax = (1 << mode.g.size) - 1;
        m_screen->serverFormat.blueMax  = (1 << mode.b.size) - 1;

        m_screen->serverFormat.bitsPerPixel = thebits;
        m_screen->serverFormat.bigEndian = mode.endian == VCML_ENDIAN_BIG;
    }

    void vncserver::render() {
        int x2 = m_fbmode.resx - 1;
        int y2 = m_fbmode.resy - 1;
        rfbMarkRectAsModified(m_screen, 0, 0, x2, y2);
    }

    shared_ptr<vncserver> vncserver::lookup(u16 port) {
        shared_ptr<vncserver>& server = servers[port];
        if (server == NULL)
            server.reset(new vncserver(port));
        return server;
    }

}}
