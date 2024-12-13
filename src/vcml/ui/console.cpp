/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/console.h"

namespace vcml {
namespace ui {

console::console(): m_inputs(), m_displays(), displays("displays") {
    for (const string& type : displays) {
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
    shutdown();
}

void console::notify(input& device) {
    m_inputs.insert(&device);
    for (auto& disp : m_displays)
        disp->attach(&device);
}

void console::setup(const videomode& mode, u8* fbptr) {
    for (auto& disp : m_displays)
        disp->init(mode, fbptr);
}

void console::render(u32 x, u32 y, u32 w, u32 h) {
    for (auto& disp : m_displays)
        disp->render(x, y, w, h);
}

void console::render() {
    for (auto& disp : m_displays)
        disp->render();
}

void console::shutdown() {
    for (auto& disp : m_displays) {
        for (auto device : m_inputs)
            disp->detach(device);
        disp->shutdown();
    }

    m_inputs.clear();
    m_displays.clear();
}

} // namespace ui
} // namespace vcml
