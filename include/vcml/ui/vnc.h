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

#ifndef VCML_UI_VNC_H
#define VCML_UI_VNC_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/report.h"
#include "vcml/logging/logger.h"

namespace vcml { namespace ui {

    typedef struct {
        u8 offset;
        u8 size;
    } vnc_color_format;

    typedef struct {
        u32 resx;
        u32 resy;
        u64 size;
        vnc_color_format a;
        vnc_color_format r;
        vnc_color_format g;
        vnc_color_format b;
        vcml_endian endian;
    } vnc_fbmode;

    vnc_fbmode fbmode_argb32(u32 width, u32 height);
    vnc_fbmode fbmode_bgra32(u32 width, u32 height);
    vnc_fbmode fbmode_rgb24(u32 width, u32 height);
    vnc_fbmode fbmode_bgr24(u32 width, u32 height);
    vnc_fbmode fbmode_rgb16(u32 width, u32 height);
    vnc_fbmode fbmode_gray8(u32 width, u32 height);

    class vnc
    {
    private:
        u16        m_port;
        vnc_fbmode m_mode;
        u8*        m_myfb;
        u8*        m_fb;

        vector<function<void(u32, bool)>*> m_key_listener;
        vector<function<void(u32, u32, u32)>*> m_ptr_listener;

        vnc() = delete;
        vnc(const vnc&) = delete;

        void cleanup();

    protected:
        static unordered_map<u16, shared_ptr<vnc>> servers;

        vnc(u16 port);
        virtual void init_framebuffer(const vnc_fbmode& mode, u8* fb);

    public:
        u32 resx() const { return m_mode.resx; }
        u32 resy() const { return m_mode.resy; }
        u16 port() const { return m_port; }

        u8* framebuffer()      const { return m_fb; }
        u64 framebuffer_size() const { return m_mode.size; }

        virtual ~vnc();

        virtual void render();

        u8*  setup_framebuffer(const vnc_fbmode& desc);
        void setup_framebuffer(const vnc_fbmode& desc, u8* ptr);

        void add_key_listener(function<void(u32, bool)>* listener);
        void remove_key_listener(function<void(u32, bool)>* listener);
        void notify_key_listeners(unsigned int key, bool down);

        void add_ptr_listener(function<void(u32, u32, u32)>* listener);
        void remove_ptr_listener(function<void(u32, u32, u32)>* listener);
        void notify_ptr_listeners(u32 buttons, u32 x, u32 y);

        static shared_ptr<vnc> lookup(u16 port);
    };

}}

#endif
