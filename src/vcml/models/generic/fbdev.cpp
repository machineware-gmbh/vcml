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
#include "vcml/debugging/vncserver.h"

namespace vcml { namespace generic {

    void fbdev::update() {
#ifdef HAVE_LIBVNC
        while (true) {
            wait_clock_cycle();
            auto vnc = debugging::vncserver::lookup(vncport);
            vnc->render();
        }
#endif
    }

    fbdev::fbdev(const sc_module_name& nm, u32 defx, u32 defy):
        component(nm),
        m_stride(),
        m_size(),
        m_vptr(nullptr),
        addr("addr", 0),
        resx("resx", defx),
        resy("resy", defy),
        vncport("vncport", 0),
        OUT("OUT") {
        VCML_ERROR_ON(resx == 0u, "resx cannot be zero");
        VCML_ERROR_ON(resy == 0u, "resy cannot be zero");
        VCML_ERROR_ON(resx > 8192u, "resx out of bounds %u", resx.get());
        VCML_ERROR_ON(resy > 8192u, "resy out of bounds %u", resy.get());

        m_stride = resx * 4;
        m_size = m_stride * resy;
    }

    fbdev::~fbdev() {
#ifdef HAVE_LIBVNC
        if (vncport > 0 && m_vptr != nullptr)
            debugging::vncserver::lookup(vncport)->stop();
#endif
    }

    void fbdev::reset() {
        // nothing to do
    }

    void fbdev::end_of_elaboration() {
        range vmem(addr, addr + m_size - 1);
        log_debug("video memory at %p..%p", vmem.start, vmem.end);

        if (vncport == 0)
            return;

        tlm_dmi dmi;
        tlm_generic_payload tx;
        tx_setup(tx, TLM_READ_COMMAND, addr, nullptr, m_size);

        if (!OUT->get_direct_mem_ptr(tx, dmi) || !dmi.get_dmi_ptr()) {
            log_warn("failed to get DMI pointer for %p", addr.get());
            return;
        }

        if (!vmem.inside(dmi)) {
            log_warn("framebuffer DMI range too small");
            return;
        }

        if  (!dmi.is_read_allowed()) {
            log_warn("framebuffer DMI pointer denies reading");
            return;
        }

        m_vptr = dmi.get_dmi_ptr() - dmi.get_start_address() + addr;
        log_debug("using DMI pointer %p", m_vptr);

#ifdef HAVE_LIBVNC
        auto vnc = debugging::vncserver::lookup(vncport);
        debugging::vnc_fbmode mode = debugging::fbmode_argb32(resx, resy);
        vnc->setup_framebuffer(mode, m_vptr);

        SC_HAS_PROCESS(fbdev);
        SC_THREAD(update);
#endif
    }

}}
