/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/models/generic/fbdev.h"

namespace vcml { namespace generic {

    void fbdev::update() {
        while (true) {
            wait_clock_cycle();
            auto disp = ui::display::lookup(display);
            disp->render();
        }
    }

    fbdev::fbdev(const sc_module_name& nm, u32 defx, u32 defy):
        component(nm),
        m_mode(),
        m_vptr(nullptr),
        addr("addr", 0),
        resx("resx", defx),
        resy("resy", defy),
        format("format", "a8r8g8b8"),
        display("display", ""),
        OUT("OUT") {
        VCML_ERROR_ON(resx == 0u, "resx cannot be zero");
        VCML_ERROR_ON(resy == 0u, "resy cannot be zero");
        VCML_ERROR_ON(resx > 8192u, "resx out of bounds %u", resx.get());
        VCML_ERROR_ON(resy > 8192u, "resy out of bounds %u", resy.get());

        const unordered_map<string, ui::fbmode> modes = {
            { "r5g6b5",   ui::fbmode::r5g6b5(resx, resy)   },
            { "r8g8b8",   ui::fbmode::r8g8b8(resx, resy)   },
            { "x8r8g8b8", ui::fbmode::x8r8g8b8(resx, resy) },
            { "a8r8g8b8", ui::fbmode::a8r8g8b8(resx, resy) },
            { "a8b8g8r8", ui::fbmode::a8b8g8r8(resx, resy) },
        };

        auto it = modes.find(to_lower(format));
        if (it != modes.end())
            m_mode = it->second;

        if (!m_mode.is_valid()) {
            log_warn("invalid color format: %s", format.get().c_str());
            m_mode = ui::fbmode::a8r8g8b8(resx, resy);
        }

        if (display != "") {
            SC_HAS_PROCESS(fbdev);
            SC_THREAD(update);
        }
    }

    fbdev::~fbdev() {
        // nothing to do
    }

    void fbdev::reset() {
        // nothing to do
    }

    void fbdev::end_of_elaboration() {
        component::end_of_elaboration();

        range vmem(addr, addr + size() - 1);
        log_debug("video memory at 0x%016lx..0x%016lx", vmem.start, vmem.end);

        if (display == "")
            return;

        if (!allow_dmi) {
            log_warn("fbdev requires DMI to be enabled");
            return;
        }

        m_vptr = OUT.lookup_dmi_ptr(vmem, VCML_ACCESS_READ);
        if (m_vptr == nullptr) {
            log_warn("failed to get DMI pointer for 0x%016lx", addr.get());
            return;
        }

        log_debug("using DMI pointer %p", m_vptr);

        auto disp = ui::display::lookup(display);
        disp->init(m_mode, m_vptr);
    }

    void fbdev::end_of_simulation() {
        component::end_of_simulation();
        auto disp = ui::display::lookup(display);
        disp->shutdown();
    }

}}
