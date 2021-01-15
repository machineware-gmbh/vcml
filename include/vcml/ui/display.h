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

#include "vcml/logging/logger.h"

#include "vcml/ui/fbmode.h"
#include "vcml/ui/keymap.h"

namespace vcml { namespace ui {

    enum key_state : u32 {
        VCML_KEY_UP = 0u,
        VCML_KEY_DOWN = 1u,
        VCML_KEY_HELD = 2u,
    };

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

    protected:
        static unordered_map<string, shared_ptr<display>> displays;

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

        virtual void add_key_listener(key_listener& l, const string& layout);
        virtual void add_ptr_listener(pos_listener& p, key_listener& k);

        virtual void remove_key_listener(key_listener& l);
        virtual void remove_ptr_listener(pos_listener& p, key_listener& k);

        static shared_ptr<display> lookup(const string& name);
    };

}}

#endif
