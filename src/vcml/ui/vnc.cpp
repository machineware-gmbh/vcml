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

    vnc_fbmode fbmode_argb32(u32 width, u32 height) {
        vnc_fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

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
        mode.size = 4ul * width * height;

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
        mode.size = 3ul * width * height;

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
        mode.size = 3ul * width * height;

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
        mode.size = 2ul * width * height;

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
        mode.size = 1ul * width * height;

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

    void vnc::cleanup() {
        if (m_myfb) {
            delete [] m_myfb;
            m_myfb = nullptr;
        }
    }

    unordered_map<u16, shared_ptr<vnc>> vnc::servers;

    vnc::vnc(u16 port):
        m_port(port),
        m_mode(),
        m_myfb(nullptr),
        m_fb(nullptr),
        m_key_listener(),
        m_ptr_listener() {
        VCML_ERROR_ON(port == 0, "invalid VNC port specified: %hu", port);
    }

    vnc::~vnc() {
        // nothing to do
    }

    void vnc::render() {
        // to be overloaded
    }

    void vnc::init_framebuffer(const vnc_fbmode& mode, u8* fb) {
        // to be overloaded
    }

    u8* vnc::setup_framebuffer(const vnc_fbmode& mode) {
       u8* fb = new u8[mode.size];
       setup_framebuffer(mode, fb);
       return m_myfb = fb;
    }

    void vnc::setup_framebuffer(const vnc_fbmode& mode, u8* ptr) {
        VCML_ERROR_ON(ptr == nullptr, "cannot map NULL framebuffer");

        cleanup();

        m_mode = mode;
        m_fb = ptr;

        init_framebuffer(mode, ptr);
    }

    void vnc::add_key_listener(function<void(u32, bool)>* listener) {
        if (stl_contains(m_key_listener, listener))
            VCML_ERROR("key listener already registered");
        m_key_listener.push_back(listener);
    }

    void vnc::remove_key_listener(function<void(u32, bool)>* listener) {
        if (!stl_contains(m_key_listener, listener))
            VCML_ERROR("attempt to remove unknown key listener");
        stl_remove_erase(m_key_listener, listener);
    }

    void vnc::notify_key_listeners(unsigned int key, bool down) {
        for (auto listener : m_key_listener)
            (*listener)(key, down);
    }

    void vnc::add_ptr_listener(function<void(u32, u32, u32)>* listener) {
        if (stl_contains(m_ptr_listener, listener))
            VCML_ERROR("pointer listener already registered");
        m_ptr_listener.push_back(listener);
    }

    void vnc::remove_ptr_listener(function<void(u32, u32, u32)>* listener) {
        if (!stl_contains(m_ptr_listener, listener))
            VCML_ERROR("attempt to remove unknown pointer listener");
        stl_remove_erase(m_ptr_listener, listener);
    }

    void vnc::notify_ptr_listeners(u32 buttons, u32 x, u32 y) {
        for (auto listener : m_ptr_listener)
            (*listener)(buttons, x, y);
    }

#ifdef HAVE_LIBVNC
#include <rfb/rfb.h>

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

    class vnc_librfb: public vnc
    {
    private:
        atomic<bool> m_running;
        rfbScreenInfo* m_screen;
        thread m_thread;

    protected:
        virtual void init_framebuffer(const vnc_fbmode& mode, u8* fb) override {
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

        void run() {
            log_debug("starting vnc server on port %d", m_screen->port);
            while (m_running && rfbIsActive(m_screen))
                rfbProcessEvents(m_screen, 1000);
            rfbShutdownServer(m_screen, false);
        }

        static void key_func(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
            int port = cl->screen->port;
            shared_ptr<vnc> server = vnc::servers[port];
            VCML_ERROR_ON(!server, "no vnc server found for port %d", port);
            server->notify_key_listeners(key, down);
        }

        static void ptr_func(int mask, int x, int y, rfbClientPtr cl) {
            int port = cl->screen->port;
            shared_ptr<vnc> server = vnc::servers[port];
            VCML_ERROR_ON(!server, "no vnc server found for port %d", port);
            server->notify_ptr_listeners(mask, x, y);
        }

    public:
        vnc_librfb(u16 port):
            vnc(port),
            m_running(true),
            m_screen(),
            m_thread() {
            rfbLog = &rfb_log_func;
            rfbErr = &rfb_err_func;

            m_screen = rfbGetScreen(NULL, NULL, resx(), resy(), 8, 4, 4);
            m_screen->port = m_screen->ipv6port = port;
            m_screen->kbdAddEvent = &vnc_librfb::key_func;
            m_screen->ptrAddEvent = &vnc_librfb::ptr_func;

            // setup green initial framebuffer
            auto defmode = fbmode_argb32(1280, 720);
            setup_framebuffer(defmode);

            // ... and paint it green
            u8* ptr = framebuffer();
            u64 len = framebuffer_size();
            for (u32 i = 1; i < len; i += 4)
                ptr[i] = 0xff;

            rfbInitServer(m_screen);

            m_thread = thread(&vnc_librfb::run, this);
            set_thread_name(m_thread, mkstr("vnc_%hu", port));
        }

        virtual ~vnc_librfb() {
            m_running = false;
            if (m_thread.joinable())
                m_thread.join();
        }

        virtual void render() override {
            int x2 = resx() - 1;
            int y2 = resy() - 1;
            rfbMarkRectAsModified(m_screen, 0, 0, x2, y2);
        }

    };

#endif

    shared_ptr<vnc> vnc::lookup(u16 port) {
        shared_ptr<vnc>& server = servers[port];
#ifdef HAVE_LIBVNC
        if (server == nullptr)
            server.reset(new vnc_librfb(port));
#else
        if (server == nullptr)
            server.reset(new vnc(port));
#endif
        return server;
    }

}}
