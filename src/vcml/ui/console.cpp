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

#include "vcml/ui/console.h"

namespace vcml { namespace ui {

    console::console():
        m_keyboards(),
        m_pointers(),
        m_displays(),
        displays("displays", "") {
        vector<string> types = split(displays, ' ');
        for (const string& type : types) {
            try {
                auto disp = display::lookup(type);
                if (disp)
                    m_displays.insert(disp);
            } catch (std::exception& ex) {
                log_warn("%s", ex.what());
            }
        }
    }

    console::~console() {
        // nothing to do
    }

    void console::notify(keyboard& kbd) {
        m_keyboards.insert(&kbd);
        for (auto& disp : m_displays)
            disp->add_keyboard(&kbd);
    }

    void console::notify(pointer& ptr) {
        m_pointers.insert(&ptr);
        for (auto& disp : m_displays)
            disp->add_pointer(&ptr);
    }

    void console::setup(const fbmode& mode, u8* fbptr) {
        for (auto& disp : m_displays)
            disp->init(mode, fbptr);
    }

    void console::render() {
        for (auto& disp : m_displays)
            disp->render();
    }

    void console::shutdown() {
        for (auto& disp : m_displays) {
            for (auto kbd : m_keyboards)
                disp->remove_keyboard(kbd);
            for (auto ptr : m_pointers)
                disp->remove_pointer(ptr);

            disp->shutdown();
        }

        m_keyboards.clear();
        m_pointers.clear();
        m_displays.clear();
    }

}}
