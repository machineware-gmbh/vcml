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
#define OCFBC_BPP(val) ((((val) & CTLR_BPP32) >> 9) + 1)

namespace vcml { namespace opencores {

    SC_HAS_PROCESS(ocfbc);

    u32 ocfbc::read_STAT() {
        log_debug("read STAT register = 0x%08x", STAT.get());
        return STAT;
    }

    u32 ocfbc::write_STAT(u32 val) {
        // only the lower 8 bits are writable
        val = (STAT & 0xFFFFFF00) | (val & 0xFF);

        if ((STAT & STAT_SINT) && !(val & STAT_SINT)) {
            log_debug("clearing system error interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_LUINT) && !(val & STAT_LUINT)) {
            log_debug("clearing FIFO underrun interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_VINT) && !(val & STAT_VINT)) {
            log_debug("clearing vertical interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_HINT) && !(val & STAT_HINT)) {
            log_debug("clearing horizontal interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_VBSINT) && !(val & STAT_VBSINT)) {
            log_debug("clearing video bank switch interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_CBSINT) && !(val & STAT_CBSINT)) {
            log_debug("clearing color bank switch interrupt");
            IRQ = false;
        }

        return val;
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

        int old_bpp = OCFBC_BPP(CTLR) * 8;
        int new_bpp = OCFBC_BPP(val) * 8;
        if (new_bpp != old_bpp) {
            log_debug("color depth changed to %d (from %d)", new_bpp, old_bpp);
        }

        VCML_LOG_REG_BIT_CHANGE(CTLR_PC, CTLR, val);

        if ((val & CTLR_VEN) && !(CTLR & CTLR_VEN)) {
            log_debug("device enabled, video ram at 0x%08x", VBARA.get());
            m_enable.notify(SC_ZERO_TIME);

            m_resx = (HTIM & 0xffff) + 1;
            m_resy = (VTIM & 0xffff) + 1;
            m_bpp  = OCFBC_BPP(val);
            m_pc   = (val & CTLR_PC) == CTLR_PC;

            create();
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

    tlm_response_status ocfbc::read(const range& addr, void* ptr,
                                    const tlm_sbi& info) {
        if (!addr.inside(m_palette_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* palette = (u8*)m_palette + addr.start - PALETTE_ADDR;
        memcpy(ptr, palette, addr.length());
        return TLM_OK_RESPONSE;
    }

    tlm_response_status ocfbc::write(const range& addr, const void* ptr,
                                     const tlm_sbi& info) {
        if (!addr.inside(m_palette_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* palette = (u8*)m_palette + addr.start - PALETTE_ADDR;
        memcpy(palette, ptr, addr.length());
        return TLM_OK_RESPONSE;
    }

    void ocfbc::create() {
        if (!m_console.has_display())
            return;

        u32 base = (STAT & STAT_AVMP) ? VBARB : VBARA;
        u32 size = m_resx * m_resy * m_bpp;
        u8* vram = nullptr;

        if (allow_dmi) {
            const range area(base, base + size - 1);
            vram = OUT.lookup_dmi_ptr(area, VCML_ACCESS_READ);
        }

        ui::fbmode mode;
        // Below we should ideally just select the mode that suits our
        // display mode and pass our own model endianess. However, some
        // VNC clients seem to have trouble reading big endian data, and
        // 24bpp modes are also not being treated consistently. Therefore
        // below is a combination of endian + format that seems to work
        // most of the time. Tested with RealVNC and Remmina.
        switch (m_bpp) {
        case 4: if (is_little_endian())
                    mode = ui::fbmode::a8r8g8b8(m_resx, m_resy);
                else
                    mode = ui::fbmode::b8g8r8a8(m_resx, m_resy);
                mode.endian = ENDIAN_LITTLE;
                break;

        case 3: if (is_little_endian())
                    mode = ui::fbmode::r8g8b8(m_resx, m_resy);
                else
                    mode = ui::fbmode::b8g8r8(m_resx, m_resy);
                mode.endian = ENDIAN_LITTLE;
                break;

        case 2: mode = ui::fbmode::r5g6b5(m_resx, m_resy);
                mode.endian = get_endian();
                break;

        case 1: if (m_pc)
                    mode = ui::fbmode::a8r8g8b8(m_resx, m_resy);
                else
                    mode = ui::fbmode::gray8(m_resx, m_resy);
                mode.endian = ENDIAN_LITTLE;
                break;

        default:
            VCML_ERROR("unknown mode: %ubpp", m_bpp * 8);
        }

        // cannot use DMI with pseudocolor
        if (!vram || m_pc) {
            log_debug("copying vnc framebuffer from vram");
            m_fb = new u8[mode.size];
            m_console.setup(mode, m_fb);
        } else {
            log_debug("mapping vnc framebuffer into vram");
            m_console.setup(mode, vram);
            m_fb = nullptr;
        }
    }

    void ocfbc::render() {
        if (m_fb != nullptr) { // need to copy data to framebuffer manually
            tlm_response_status rs;

            u32 burstsz = OCFBC_VBL(CTLR);
            u32 linesz = m_resx * m_bpp;

            u8 linebuf[linesz];
            u8* fb = m_fb;

            for (u32 y = 0; y < m_resy; y++) {
                // burst-read one horizontal line of pixels into buffer
                for (u32 x = 0; x < linesz; x += burstsz) {
                    u32 base = (STAT & STAT_AVMP) ? VBARB : VBARA;
                    u32 addr = base + y * linesz + x;
                    u8* dest = m_pc ? linebuf : fb;
                    if (failed(rs = OUT.read(addr, dest + x, burstsz))) {
                        log_debug("failed to read vmem at 0x%08x: %s", addr,
                                  tlm_response_to_str(rs));
                        STAT |= STAT_SINT;
                        IRQ = true;
                    }
                }

                if (!m_pc) {
                    fb += linesz; // done, data is already copied
                } else {
                    u32* palette = m_palette;
                    if (STAT & STAT_ACMP)
                        palette = m_palette + 0x100;

                    for (u32 x = 0; x < linesz; x++) {
                        u32 color = to_host_endian(palette[linebuf[x]]);
                        *fb++ = (color >>  0) & 0xff; // b
                        *fb++ = (color >>  8) & 0xff; // g
                        *fb++ = (color >> 16) & 0xff; // r
                        *fb++ = 0xff; // a
                    }
                }

                // Note that the HSYNC interrupt will only be triggered when
                // DMI is not used. Otherwise, this is never executed
                if (CTLR & CTLR_HIE) {
                    STAT |= STAT_HINT;
                    IRQ = true;
                }
            }
        }

        if (CTLR & CTLR_CBSWE) {
            STAT ^= STAT_ACMP;   // toggle ACMP bit
            CTLR &= ~CTLR_CBSWE; // clear CBSWE bit
            if (CTLR & CTLR_CBSIE)
                IRQ = true;
        }

        if (CTLR & CTLR_VBSWE) {
            STAT ^= STAT_AVMP;   // toggle AVMP bit
            CTLR &= ~CTLR_VBSWE; // clear VBSWE bit
            if (CTLR & CTLR_VBSIE)
                IRQ = true;
        }

        if (CTLR & CTLR_VIE) {
            STAT |= STAT_VINT;
            IRQ = true; // VSYNC interrupt
        }

        m_console.render(); // output image
    }

    void ocfbc::update() {
        while (true) {
            while (!(CTLR & CTLR_VEN))
                wait(m_enable);

            sc_time t = sc_time_stamp();
            render();
            sc_time delta = sc_time_stamp() - t;
            sc_time frame(1.0 / clock, SC_SEC);
            if (delta < frame) {
                wait(frame - delta); // wait until next frame
            } else {
                log_debug("skipped %d frames", (int)(delta / frame));
                wait(frame - delta % frame);
            }
        }
    }

    bool ocfbc::cmd_info(const vector<string>& args, ostream& os) {
        os << "resolution:  " << m_resx << "x" << m_resy
           << "@" << clock.get() << "Hz" << std::endl
           << "framebuffer: " << (m_fb ? "copied" : "mapped") << std::endl
           << "interrupt:   " << (IRQ.read() ? "set" : "cleared") << std::endl;
        return true;
    }

    ocfbc::ocfbc(const sc_module_name& nm):
        peripheral(nm),
        m_palette_addr(PALETTE_ADDR, PALETTE_ADDR + sizeof(m_palette)),
        m_palette(),
        m_fb(nullptr),
        m_resx(0),
        m_resy(0),
        m_bpp(0),
        m_pc(false),
        m_enable("enabled"),
        CTLR("CTRLR", 0x00, 0),
        STAT("STATR", 0x04, 0),
        HTIM("HTIMR", 0x08, 0),
        VTIM("VTIMR", 0x0c, 0),
        HVLEN("HVLEN", 0x10, 0),
        VBARA("VBARA", 0x14, 0),
        VBARB("VBARB", 0x18, 0),
        IRQ("IRQ"),
        IN("IN"),
        OUT("OUT"),
        clock("clock", 60) { // 60Hz

        CTLR.allow_read_write();
        CTLR.write = &ocfbc::write_CTRL;

        STAT.allow_read_write();
        STAT.read = &ocfbc::read_STAT;
        STAT.write = &ocfbc::write_STAT;

        HTIM.allow_read_write();
        HTIM.write = &ocfbc::write_HTIM;

        VTIM.allow_read_write();
        VTIM.write = &ocfbc::write_VTIM;

        SC_THREAD(update);

        register_command("info", 0, this, &ocfbc::cmd_info,
                         "shows information about the framebuffer");
    }

    ocfbc::~ocfbc() {
        if (m_fb != nullptr)
            delete [] m_fb;
    }

    void ocfbc::end_of_simulation() {
        m_console.shutdown();
        peripheral::end_of_simulation();
    }

}}
