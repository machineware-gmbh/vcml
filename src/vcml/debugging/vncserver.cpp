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

    vnc_fbdesc fbdesc_argb32(u32 width, u32 height) {
        vnc_fbdesc desc;

        desc.resx = width;
        desc.resy = height;
        desc.size = width * height * 4;

        desc.a.size = desc.r.size = desc.g.size = desc.b.size = 8;

        desc.a.offset = 0;
        desc.r.offset = 8;
        desc.g.offset = 16;
        desc.b.offset = 24;

        return desc;
    }

    vnc_fbdesc fbdesc_rgb24(u32 width, u32 height) {
        vnc_fbdesc desc;

        desc.resx = width;
        desc.resy = height;
        desc.size = width * height * 3;

        desc.a.size = 0;
        desc.a.offset = 0;

        desc.r.size = desc.g.size = desc.b.size = 8;

        desc.r.offset = 0;
        desc.g.offset = 8;
        desc.b.offset = 16;

        return desc;
    }

    vnc_fbdesc fbdesc_rgb16(u32 width, u32 height) {
        vnc_fbdesc desc;

        desc.resx = width;
        desc.resy = height;
        desc.size = width * height * 2;

        desc.a.size = 0;
        desc.a.offset = 0;

        desc.r.size = 5;
        desc.g.size = 6;
        desc.b.size = 5;

        desc.r.offset = 0;
        desc.g.offset = 5;
        desc.b.offset = 11;

        return desc;
    }

    vnc_fbdesc fbdesc_gray8(u32 width, u32 height) {
        vnc_fbdesc desc;

        desc.resx = width;
        desc.resy = height;
        desc.size = width * height;

        desc.a.size = 0;
        desc.a.offset = 0;

        desc.r.size = 8;
        desc.g.size = 8;
        desc.b.size = 8;

        desc.r.offset = 0;
        desc.g.offset = 0;
        desc.b.offset = 0;

        return desc;
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
        m_fbdesc(fbdesc_argb32(800, 600)),
        m_fb(new u8[m_fbdesc.size]),
        m_key_handler() {

        rfbLog = &vncserver_log_func;
        rfbErr = &vncserver_err_func;

        int w = m_fbdesc.resx;
        int h = m_fbdesc.resy;

        m_screen = rfbGetScreen(NULL, NULL, w, h, 8, 4, 4);
        m_screen->port = m_screen->ipv6port = port;
        m_screen->kbdAddEvent = &vncserver::key_func;
        m_screen->frameBuffer = (char*)m_fb;

        // blue initial framebuffer
        for (u32 i = 2; i < m_fbdesc.size; i += 4)
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

    u8* vncserver::setup_framebuffer(const vnc_fbdesc& desc) {
        u8* fb = new u8[desc.size];
        setup_framebuffer(desc, fb);
        m_fb = fb;
        return fb;
    }

    void vncserver::setup_framebuffer(const vnc_fbdesc& desc, u8* ptr) {
        VCML_ERROR_ON(ptr == NULL, "attempt to map NULL as framebuffer");

        if (m_fb != NULL) {
            delete [] m_fb;
            m_fb = NULL;
        }

        m_fbdesc = desc;
        m_screen->frameBuffer = (char*)ptr;

        u8 thebits = desc.a.offset + desc.a.size;
        thebits = max<u8>(thebits, desc.r.size + desc.r.offset);
        thebits = max<u8>(thebits, desc.g.size + desc.g.offset);
        thebits = max<u8>(thebits, desc.b.size + desc.b.offset);

        u32 samples = 0;
        if (desc.a.size > 0)
            samples++;
        if (desc.r.size > 0)
            samples++;
        if (desc.g.size > 0)
            samples++;
        if (desc.g.size > 0)
            samples++;

        rfbNewFramebuffer(m_screen, m_screen->frameBuffer, m_fbdesc.resx,
                          m_fbdesc.resy, desc.r.size, samples, thebits / 8);

        m_screen->serverFormat.redShift   = desc.r.offset;
        m_screen->serverFormat.greenShift = desc.g.offset;
        m_screen->serverFormat.blueShift  = desc.b.offset;

        m_screen->serverFormat.redMax   = (1 << desc.r.size) - 1;
        m_screen->serverFormat.greenMax = (1 << desc.g.size) - 1;
        m_screen->serverFormat.blueMax  = (1 << desc.b.size) - 1;

        m_screen->serverFormat.bitsPerPixel = thebits;
    }

    void vncserver::render() {
        int x2 = m_fbdesc.resx - 1;
        int y2 = m_fbdesc.resy - 1;
        rfbMarkRectAsModified(m_screen, 0, 0, x2, y2);
    }

    shared_ptr<vncserver> vncserver::lookup(u16 port) {
        shared_ptr<vncserver>& server = servers[port];
        if (server == NULL)
            server.reset(new vncserver(port));
        return server;
    }

}}
