/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/models/opencores/ocfbc.h"

#define OCFBC_VBL(val) (1 << (((val) & CTLR_VBL8) >> 7))
#define OCFBC_BPP(val) (((((val) & CTLR_BPP32) >> 9) + 1) * 8)

namespace vcml { namespace opencores {

    u32 ocfbc::read_STAT() {
        log_debug("read STAT register = 0x%08x", STAT.get());
        return STAT;
    }

    u32 ocfbc::write_CTRL(u32 val) {
        VCML_LOG_REG_BIT_CHANGE(CTLR_VEN,   CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_VIE,   CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_HIE,   CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_VBSIE, CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_CBSIE, CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_VBSWE, CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_CBSWE, CTLR, val);

        int old_vbl = OCFBC_VBL(CTLR);
        int new_vbl = OCFBC_VBL(val);
        if (new_vbl != old_vbl) {
            log_debug("video burst changed to %d (from %d)", new_vbl, old_vbl);
        }

        int old_bpp = OCFBC_BPP(CTLR);
        int new_bpp = OCFBC_BPP(val);
        if (new_bpp != old_bpp) {
            log_debug("color depth changed to %d (from %d)", new_bpp, old_bpp);
        }

        VCML_LOG_REG_BIT_CHANGE(CTLR_PC, CTLR, val);

        if ((val & CTLR_VEN) && !(CTLR & CTLR_VEN)) {
            log_debug("device enabled, video ram at 0x%08x", VBARA.get());
        }
        return val;
    }

    u32 ocfbc::write_HTIM(u32 val) {
        u32 sync = (val >> 24) & 0xff;
        u32 gdel = (val >> 16) & 0xff;
        u32 gate = (val & 0xffff) + 1;
        log_debug("write HTIM: hsync = %d, hgate delay = %d, hgate = %d",
                  sync, gdel, gate);
        return val;
    }

    u32 ocfbc::write_VTIM(u32 val) {
        u32 sync = (val >> 24) & 0xff;
        u32 gdel = (val >> 16) & 0xff;
        u32 gate = (val & 0xffff) + 1;
        log_debug("write VTIM: vsync = %d, vgate delay = %d, vgate = %d",
                  sync, gdel, gate);
        return val;
    }

    tlm_response_status ocfbc::read(const range& addr, void* ptr, int flags) {
        if (!addr.inside(m_palette_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* palette = (u8*)m_palette + addr.start - PALETTE_ADDR;
        memcpy(ptr, palette, addr.length());
        return TLM_OK_RESPONSE;
    }

    tlm_response_status ocfbc::write(const range& addr, const void* p, int f) {
        if (!addr.inside(m_palette_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* palette = (u8*)m_palette + addr.start - PALETTE_ADDR;
        memcpy(palette, p, addr.length());
        return TLM_OK_RESPONSE;
    }

    ocfbc::ocfbc(const sc_module_name& nm):
        peripheral(nm),
        CTLR("CTRLR", 0x00, 0),
        STAT("STATR", 0x04, 0),
        HTIM("HTIMR", 0x08, 0),
        VTIM("VTIMR", 0x0c, 0),
        HVLEN("HVLEN", 0x10, 0),
        VBARA("VBARA", 0x14, 0),
        IRQ("IRQ"),
        IN("IN"),
        OUT("OUT"),
        vncport("vncport", 0) {
        if (vncport > 0)
            m_vnc = debugging::vncserver::lookup(vncport);
        if (m_vnc)
            log_debug("using vncserver at port %d", (int)m_vnc->get_port());

        CTLR.allow_read_write();
        CTLR.write = &ocfbc::write_CTRL;

        STAT.allow_read();
        STAT.read = &ocfbc::read_STAT;

        HTIM.allow_read_write();
        HTIM.write = &ocfbc::write_HTIM;

        VTIM.allow_read_write();
        VTIM.write = &ocfbc::write_VTIM;
    }

    ocfbc::~ocfbc() {
        /* nothing to do */
    }

}}
