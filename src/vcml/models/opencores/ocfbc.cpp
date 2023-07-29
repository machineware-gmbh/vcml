/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/opencores/ocfbc.h"

#define OCFBC_VBL(val) (1 << (((val)&CTLR_VBL8) >> 7))
#define OCFBC_BPP(val) ((((val)&CTLR_BPP32) >> 9) + 1)

namespace vcml {
namespace opencores {

u32 ocfbc::read_stat() {
    log_debug("read STAT register = 0x%08x", stat.get());
    return stat;
}

void ocfbc::write_stat(u32 val) {
    // only the lower 8 bits are writable
    val = (stat & 0xFFFFFF00) | (val & 0xFF);

    if ((stat & STAT_SINT) && !(val & STAT_SINT)) {
        log_debug("clearing system error interrupt");
        irq = false;
    }

    if ((stat & STAT_LUINT) && !(val & STAT_LUINT)) {
        log_debug("clearing FIFO underrun interrupt");
        irq = false;
    }

    if ((stat & STAT_VINT) && !(val & STAT_VINT)) {
        log_debug("clearing vertical interrupt");
        irq = false;
    }

    if ((stat & STAT_HINT) && !(val & STAT_HINT)) {
        log_debug("clearing horizontal interrupt");
        irq = false;
    }

    if ((stat & STAT_VBSINT) && !(val & STAT_VBSINT)) {
        log_debug("clearing video bank switch interrupt");
        irq = false;
    }

    if ((stat & STAT_CBSINT) && !(val & STAT_CBSINT)) {
        log_debug("clearing color bank switch interrupt");
        irq = false;
    }

    stat = val;
}

void ocfbc::write_ctrl(u32 val) {
    VCML_LOG_REG_BIT_CHANGE(CTLR_VEN, ctlr, val);
    VCML_LOG_REG_BIT_CHANGE(CTLR_VIE, ctlr, val);
    VCML_LOG_REG_BIT_CHANGE(CTLR_HIE, ctlr, val);
    VCML_LOG_REG_BIT_CHANGE(CTLR_VBSIE, ctlr, val);
    VCML_LOG_REG_BIT_CHANGE(CTLR_CBSIE, ctlr, val);
    VCML_LOG_REG_BIT_CHANGE(CTLR_VBSWE, ctlr, val);
    VCML_LOG_REG_BIT_CHANGE(CTLR_CBSWE, ctlr, val);

    int old_vbl = OCFBC_VBL(ctlr);
    int new_vbl = OCFBC_VBL(val);
    if (new_vbl != old_vbl) {
        log_debug("video burst changed to %d (from %d)", new_vbl, old_vbl);
    }

    int old_bpp = OCFBC_BPP(ctlr) * 8;
    int new_bpp = OCFBC_BPP(val) * 8;
    if (new_bpp != old_bpp) {
        log_debug("color depth changed to %d (from %d)", new_bpp, old_bpp);
    }

    VCML_LOG_REG_BIT_CHANGE(CTLR_PC, ctlr, val);

    if ((val & CTLR_VEN) && !(ctlr & CTLR_VEN)) {
        log_debug("device enabled, video ram at 0x%08x", vbara.get());
        m_enable.notify(SC_ZERO_TIME);

        m_xres = (htim & 0xffff) + 1;
        m_yres = (vtim & 0xffff) + 1;
        m_bpp = OCFBC_BPP(val);
        m_pc = (val & CTLR_PC) == CTLR_PC;

        create();
    }

    ctlr = val;
}

void ocfbc::write_htim(u32 val) {
    u32 sync = (val >> 24) & 0xff;
    u32 gdel = (val >> 16) & 0xff;
    u32 gate = (val & 0xffff) + 1;
    log_debug("write HTIM: hsync = %d, hgate delay = %d, hgate = %d", sync,
              gdel, gate);
    htim = val;
}

void ocfbc::write_vtim(u32 val) {
    u32 sync = (val >> 24) & 0xff;
    u32 gdel = (val >> 16) & 0xff;
    u32 gate = (val & 0xffff) + 1;
    log_debug("write VTIM: vsync = %d, vgate delay = %d, vgate = %d", sync,
              gdel, gate);
    vtim = val;
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

    u32 base = (stat & STAT_AVMP) ? vbarb : vbara;
    u32 size = m_xres * m_yres * m_bpp;
    u8* vram = nullptr;

    if (allow_dmi) {
        const range area(base, base + size - 1);
        vram = out.lookup_dmi_ptr(area, VCML_ACCESS_READ);
    }

    ui::videomode mode;
    // Below we should ideally just select the mode that suits our
    // display mode and pass our own model endianess. However, some
    // VNC clients seem to have trouble reading big endian data, and
    // 24bpp modes are also not being treated consistently. Therefore
    // below is a combination of endian + format that seems to work
    // most of the time. Tested with RealVNC and Remmina.
    switch (m_bpp) {
    case 4:
        if (is_little_endian())
            mode = ui::videomode::a8r8g8b8(m_xres, m_yres);
        else
            mode = ui::videomode::b8g8r8a8(m_xres, m_yres);
        mode.endian = ENDIAN_LITTLE;
        break;

    case 3:
        if (is_little_endian())
            mode = ui::videomode::r8g8b8(m_xres, m_yres);
        else
            mode = ui::videomode::b8g8r8(m_xres, m_yres);
        mode.endian = ENDIAN_LITTLE;
        break;

    case 2:
        mode = ui::videomode::r5g6b5(m_xres, m_yres);

        mode.endian = endian;
        break;

    case 1:
        if (m_pc)
            mode = ui::videomode::a8r8g8b8(m_xres, m_yres);
        else
            mode = ui::videomode::gray8(m_xres, m_yres);
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

        u32 burstsz = OCFBC_VBL(ctlr);
        u32 linesz = m_xres * m_bpp;

        u8 linebuf[linesz];
        memset(linebuf, 0, sizeof(linebuf));

        u8* fb = m_fb;

        for (u32 y = 0; y < m_yres; y++) {
            // burst-read one horizontal line of pixels into buffer
            for (u32 x = 0; x < linesz; x += burstsz) {
                u32 base = (stat & STAT_AVMP) ? vbarb : vbara;
                u32 addr = base + y * linesz + x;
                u8* dest = m_pc ? linebuf : fb;
                if (failed(rs = out.read(addr, dest + x, burstsz))) {
                    log_debug("failed to read vmem at 0x%08x: %s", addr,
                              tlm_response_to_str(rs));
                    stat |= STAT_SINT;
                    irq = true;
                }
            }

            if (!m_pc) {
                fb += linesz; // done, data is already copied
            } else {
                u32* palette = m_palette;
                if (stat & STAT_ACMP)
                    palette = m_palette + 0x100;

                for (u32 x = 0; x < linesz; x++) {
                    u32 color = to_host_endian(palette[linebuf[x]]);
                    *fb++ = (color >> 0) & 0xff;  // b
                    *fb++ = (color >> 8) & 0xff;  // g
                    *fb++ = (color >> 16) & 0xff; // r
                    *fb++ = 0xff;                 // a
                }
            }

            // Note that the HSYNC interrupt will only be triggered when
            // DMI is not used. Otherwise, this is never executed
            if (ctlr & CTLR_HIE) {
                stat |= STAT_HINT;
                irq = true;
            }
        }
    }

    if (ctlr & CTLR_CBSWE) {
        stat ^= STAT_ACMP;   // toggle ACMP bit
        ctlr &= ~CTLR_CBSWE; // clear CBSWE bit
        if (ctlr & CTLR_CBSIE)
            irq = true;
    }

    if (ctlr & CTLR_VBSWE) {
        stat ^= STAT_AVMP;   // toggle AVMP bit
        ctlr &= ~CTLR_VBSWE; // clear VBSWE bit
        if (ctlr & CTLR_VBSIE)
            irq = true;
    }

    if (ctlr & CTLR_VIE) {
        stat |= STAT_VINT;
        irq = true; // VSYNC interrupt
    }

    m_console.render(); // output image
}

void ocfbc::update() {
    while (true) {
        while (!(ctlr & CTLR_VEN))
            wait(m_enable);

        const sc_time& t = sc_time_stamp();

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
    os << "resolution:  " << m_xres << "x" << m_yres << "@" << clock.get()
       << "Hz" << std::endl
       << "framebuffer: " << (m_fb ? "copied" : "mapped") << std::endl
       << "interrupt:   " << (irq.read() ? "set" : "cleared") << std::endl;
    return true;
}

ocfbc::ocfbc(const sc_module_name& nm):
    peripheral(nm),
    m_palette_addr(PALETTE_ADDR, PALETTE_ADDR + sizeof(m_palette)),
    m_palette(),
    m_fb(nullptr),
    m_xres(0),
    m_yres(0),
    m_bpp(0),
    m_pc(false),
    m_enable("enabled"),
    ctlr("ctlr", 0x00, 0),
    stat("stat", 0x04, 0),
    htim("htim", 0x08, 0),
    vtim("vtim", 0x0c, 0),
    hvlen("hvlen", 0x10, 0),
    vbara("vbara", 0x14, 0),
    vbarb("vbarb", 0x18, 0),
    irq("irq"),
    in("in"),
    out("out"),
    clock("clock", 60 * Hz) {
    ctlr.allow_read_write();
    ctlr.on_write(&ocfbc::write_ctrl);

    stat.allow_read_write();
    stat.on_read(&ocfbc::read_stat);
    stat.on_write(&ocfbc::write_stat);

    htim.allow_read_write();
    htim.on_write(&ocfbc::write_htim);

    vtim.allow_read_write();
    vtim.on_write(&ocfbc::write_vtim);

    SC_HAS_PROCESS(ocfbc);
    SC_THREAD(update);

    register_command("info", 0, &ocfbc::cmd_info,
                     "shows information about the framebuffer");
}

ocfbc::~ocfbc() {
    if (m_fb != nullptr)
        delete[] m_fb;
}

void ocfbc::end_of_simulation() {
    m_console.shutdown();
    peripheral::end_of_simulation();
}

VCML_EXPORT_MODEL(vcml::opencores::ocfbc, name, args) {
    return new ocfbc(name);
}

} // namespace opencores
} // namespace vcml
