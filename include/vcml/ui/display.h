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

#ifndef VCML_UI_DISPLAY_H
#define VCML_UI_DISPLAY_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/report.h"
#include "vcml/common/thctl.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/fbmode.h"
#include "vcml/ui/keymap.h"

namespace vcml { namespace ui {

    typedef function<void(u32, u32)> key_listener;
    typedef function<void(u32, u32)> pos_listener;

    class display
    {
    private:
        string m_name;
        string m_type;
        u32    m_dispno;
        fbmode m_mode;
        u8*    m_fb;

        struct key_listener_state {
            key_listener* keyev;
            string layout;

            bool shift_l;
            bool shift_r;
            bool capsl;
            bool alt_l;
            bool alt_r;
            u32 prev_sym;

            bool shift() const { return shift_l | shift_r; }

            void dokey(u32 key, u32 state) {
                if (keyev != nullptr)
                    (*keyev)(key, state);
            }

            key_listener_state(key_listener* k, const string& l): keyev(k),
                layout(l), shift_l(), shift_r(), capsl(), alt_l(), alt_r(),
                prev_sym() {
            }

            void notify_key(u32 sym, bool down);
        };

        struct ptr_listener_state {
            key_listener* btnev;
            pos_listener* posev;

            u32 buttons;
            u32 prev_x;
            u32 prev_y;

            void dobtn(u32 btn, u32 state) {
                if (btnev != nullptr)
                    (*btnev)(btn, state);
            }

            ptr_listener_state(pos_listener* pos, key_listener* btn):
                btnev(btn), posev(pos), buttons(), prev_x(), prev_y() {
            }

            void notify_btn(u32 btn, bool down);
            void notify_pos(u32 x, u32 y);
        };

        vector<key_listener_state> m_key_listeners;
        vector<ptr_listener_state> m_ptr_listeners;

    protected:
        static unordered_map<string, function<display*(u32)>> types;
        static unordered_map<string, shared_ptr<display>> displays;

        static display* create(u32 nr);

        display() = delete;
        display(const display&) = delete;
        display(const string& type, u32 nr);

    public:
        u32 resx() const { return m_mode.resx; }
        u32 resy() const { return m_mode.resy; }

        u32 dispno() const { return m_dispno; }

        const char* type() const { return m_type.c_str(); }
        const char* name() const { return m_name.c_str(); }

        const fbmode& mode() const { return m_mode; }

        u8* framebuffer()      const { return m_fb; }
        u64 framebuffer_size() const { return m_mode.size; }

        virtual ~display();

        virtual void init(const fbmode& mode, u8* fbptr);
        virtual void render();
        virtual void shutdown();

        virtual void notify_key(u32 keysym, bool down);
        virtual void notify_btn(u32 button, bool down);
        virtual void notify_pos(u32 x, u32 y);

        virtual void add_key_listener(key_listener& l, const string& layout);
        virtual void add_ptr_listener(pos_listener& p, key_listener& k);

        virtual void remove_key_listener(key_listener& l);
        virtual void remove_ptr_listener(pos_listener& p, key_listener& k);

        static shared_ptr<display> lookup(const string& name);
        static void register_display_type(const string& type,
                                          function<display*(u32)> create);
    };

}}

#endif
