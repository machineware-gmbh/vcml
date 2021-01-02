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

#ifndef VCML_UI_KEYMAP_H
#define VCML_UI_KEYMAP_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/logging/logger.h"

#include <linux/input.h>

namespace vcml { namespace ui {

    struct syminfo {
        u16  sym;   // key symbol (e.g. 'A' or '\t') from vnc
        u16  code;  // linux key code (depends on keyboard layout)
        bool shift; // additionally needs shift to produce key symbol
        bool l_alt; // additionally needs alt to produce key symbol
        bool r_alt; // additionally needs altgr to produce key symbol
        const char* name;
    };

    class keymap
    {
    private:
        keymap(const vector<syminfo>& layout);

    public:
        const vector<syminfo>& layout;

        keymap() = delete;
        keymap(const keymap&) = default;
        keymap(keymap&&) = default;

        bool is_reserved(u16 code) const;
        bool is_reserved(const syminfo* info) const {
            return !info || is_reserved(info->code);
        }

        const syminfo* lookup_symbol(u32 symbol) const;
        vector<u16> translate_symbol(u32 symbol) const;

        static const keymap& lookup(const string& name);
    };

}}

#endif
