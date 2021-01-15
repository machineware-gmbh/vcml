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
#include "vcml/common/thctl.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/fbmode.h"
#include "vcml/ui/display.h"

#include <rfb/rfb.h>

namespace vcml { namespace ui {

    class vnc: public display
    {
    private:
        u16 m_port;
        atomic<bool> m_running;
        mutex m_mutex;
        rfbScreenInfo* m_screen;
        thread m_thread;

        struct vnc_key_listener {
            key_listener* keyev;
            string layout;
            bool shift;
            bool capsl;
            bool alt_l;
            bool alt_r;

            u32 prev_sym;

            void notify_key(u32 key, u32 state) {
                if (keyev != nullptr)
                    (*keyev)(key, state);
            }

            vnc_key_listener(key_listener* k, const string& l):
                keyev(k), layout(l), shift(), capsl(), alt_l(), alt_r(),
                prev_sym(-1) {
            }

            void notify(u32 sym, bool down);
        };

        struct vnc_ptr_listener {
            pos_listener* ptrev;
            key_listener* btnev;

            u32 prev_buttons;
            u32 prev_x;
            u32 prev_y;

            void notify_ptr(u32 x, u32 y) {
                if (ptrev != nullptr)
                    (*ptrev)(x, y);
            }

            void notify_btn(u32 key, u32 state) {
                if (btnev != nullptr)
                    (*btnev)(key, state);
            }

            vnc_ptr_listener(pos_listener* p, key_listener* k):
                ptrev(p), btnev(k), prev_buttons(), prev_x(), prev_y() {
            }

            void notify(u32 buttons, u32 x, u32 y);
        };

        vector<vnc_key_listener> m_key_listener;
        vector<vnc_ptr_listener> m_ptr_listener;

        void run();

    public:
        u16 port() const { return m_port; }

        vnc(u32 no);
        virtual ~vnc();

        virtual void init(const fbmode& mode, u8* fb) override;
        virtual void render() override;
        virtual void shutdown() override;

        virtual void add_key_listener(key_listener& listener,
                                      const string& layout) override;
        virtual void add_ptr_listener(pos_listener& ptr_listener,
                                      key_listener& btn_listener) override;

        virtual void remove_key_listener(key_listener& listener) override;
        virtual void remove_ptr_listener(pos_listener& ptr_listener,
                                         key_listener& btn_listener) override;

        void key_event(u32 sym, bool down);
        void ptr_event(u32 mask, u32 x, u32 y);
    };

}}

#endif
