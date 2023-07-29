/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/fbdev.h"

namespace vcml {
namespace generic {

void fbdev::update() {
    while (true) {
        wait_clock_cycle();
        m_console.render();
    }
}

fbdev::fbdev(const sc_module_name& nm, u32 defx, u32 defy):
    component(nm),
    m_console(),
    m_mode(),
    m_vptr(nullptr),
    addr("addr", 0),
    xres("xres", defx),
    yres("yres", defy),
    format("format", "a8r8g8b8"),
    out("out") {
    VCML_ERROR_ON(xres == 0u, "xres cannot be zero");
    VCML_ERROR_ON(yres == 0u, "yres cannot be zero");
    VCML_ERROR_ON(xres > 8192u, "xres out of bounds %u", xres.get());
    VCML_ERROR_ON(yres > 8192u, "yres out of bounds %u", yres.get());

    const unordered_map<string, ui::videomode> modes = {
        { "r5g6b5", ui::videomode::r5g6b5(xres, yres) },
        { "r8g8b8", ui::videomode::r8g8b8(xres, yres) },
        { "x8r8g8b8", ui::videomode::x8r8g8b8(xres, yres) },
        { "a8r8g8b8", ui::videomode::a8r8g8b8(xres, yres) },
        { "a8b8g8r8", ui::videomode::a8b8g8r8(xres, yres) },
    };

    auto it = modes.find(to_lower(format));
    if (it != modes.end())
        m_mode = it->second;

    if (!m_mode.is_valid()) {
        log_warn("invalid color format: %s", format.get().c_str());
        m_mode = ui::videomode::a8r8g8b8(xres, yres);
    }

    if (m_console.has_display()) {
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
    log_debug("video memory at 0x%016llx..0x%016llx", vmem.start, vmem.end);

    if (!m_console.has_display())
        return;

    if (!allow_dmi) {
        log_warn("fbdev requires DMI to be enabled");
        return;
    }

    m_vptr = out.lookup_dmi_ptr(vmem, VCML_ACCESS_READ);
    if (m_vptr == nullptr) {
        log_warn("failed to get DMI pointer for 0x%016llx", addr.get());
        return;
    }

    log_debug("using DMI pointer %p", m_vptr);
    m_console.setup(m_mode, m_vptr);
}

void fbdev::end_of_simulation() {
    m_console.shutdown();
    component::end_of_simulation();
}

VCML_EXPORT_MODEL(vcml::generic::fbdev, name, args) {
    if (args.empty())
        return new fbdev(name);

    u32 w = 1280;
    u32 h = 720;
    int n = sscanf(args[0].c_str(), "%ux%u", &w, &h);
    VCML_ERROR_ON(n != 2, "invalid format string: %s", args[0].c_str());
    return new fbdev(name, w, h);
}

} // namespace generic
} // namespace vcml
