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

#if defined(HAVE_LIBVNC) && !defined(VCML_DEBUGGING_VNCSERVER_H)
#define VCML_DEBUGGING_VNCSERVER_H

#include <rfb/rfb.h>

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"

namespace vcml { namespace debugging {

    typedef struct {
        u8 offset;
        u8 size;
    } vnc_color_format;

    typedef struct {
        u32 resx;
        u32 resy;
        u32 size;
        vnc_color_format a;
        vnc_color_format r;
        vnc_color_format g;
        vnc_color_format b;
    } vnc_fbdesc;

    vnc_fbdesc fbdesc_argb32(u32 width, u32 height);
    vnc_fbdesc fbdesc_rgb24(u32 width, u32 height);
    vnc_fbdesc fbdesc_rgb16(u32 width, u32 height);
    vnc_fbdesc fbdesc_gray8(u32 width, u32 height);

    class vncserver {
    private:
        rfbScreenInfo* m_screen;
        pthread_t      m_thread;
        volatile bool  m_running;

        vnc_fbdesc     m_fbdesc;
        u8*            m_fb;

        vector<function<void(u32, bool)>*> m_key_handler;

        static std::map<u16, shared_ptr<vncserver>> servers;

        void run();
        void dokey(unsigned int key, bool down);

        static void* thread_func(void* data);
        static void  key_func(rfbBool down, rfbKeySym key, rfbClientPtr cl);

        vncserver();
        vncserver(u16 port);
        vncserver(const vncserver&);

    public:
        u16 get_port() const { return (u16)m_screen->port; }

        u8*  setup_framebuffer(const vnc_fbdesc& desc);
        void setup_framebuffer(const vnc_fbdesc& desc, u8* ptr);

        void render();

        virtual ~vncserver();

        void add_key_listener(function<void(u32, bool)>* handler);
        void remove_key_listener(function<void(u32, bool)>* handler);

        static shared_ptr<vncserver> lookup(u16 port);
    };

}}

#endif
