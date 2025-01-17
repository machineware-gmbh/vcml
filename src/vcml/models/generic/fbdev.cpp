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

template <typename T>
void write_to_ostream(ostream& os, const T* val) {
    os.write(reinterpret_cast<const char*>(val), sizeof(T));
}

struct bmp_header {
    array<char, 2> header; // 0x42 0x4D for BMP (BM)
    u32 size;              // total size (bytes)
    u16 reserved_0;
    u16 reserved_1;
    u32 offset; // offset of the pixel array

    u32 header_size() { return 14; }

    bmp_header():
        header({ 'B', 'M' }),
        size(0),
        reserved_0(0),
        reserved_1(0),
        offset(0) {}

    void write(ostream& out) {
        write_to_ostream(out, &header[0]);
        write_to_ostream(out, &header[1]);
        write_to_ostream(out, &size);
        write_to_ostream(out, &reserved_0);
        write_to_ostream(out, &reserved_1);
        write_to_ostream(out, &offset);
    }
};
struct dib_header {
    u32 size;           // size of this header (bytes) -> 40
    i32 width;          // in pixels
    i32 height;         // in pixels
    u16 color_planes;   // number of color planes -> 1
    u16 bits_per_pixel; // typically 1, 4, 5, 16, 24, 32
    u32 method;         // compression method
    u32 image_size; // size of the raw bitmap data; a dummy 0 can be given for
                    // BI_RGB bitmaps
    i32 xres;       // (pixel per metre, signed integer)
    i32 yres;       // (pixel per metre, signed integer)
    u32 col_num; // number of colors in the color palette, or 0 to default to
                 // 2^n
    u32 imp_col_num; // the number of important colors used, or 0 when every
                     // color is important; generally ignored

    u32 header_size() { return 40; }

    dib_header():
        size(header_size()),
        width(0),
        height(0),
        color_planes(1),
        bits_per_pixel(24),
        method(0),
        image_size(0),
        xres(0),
        yres(0),
        col_num(0),
        imp_col_num(0) {}

    void write(ostream& out) {
        write_to_ostream(out, &size);
        write_to_ostream(out, &width);
        write_to_ostream(out, &height);
        write_to_ostream(out, &color_planes);
        write_to_ostream(out, &bits_per_pixel);
        write_to_ostream(out, &method);
        write_to_ostream(out, &image_size);
        write_to_ostream(out, &xres);
        write_to_ostream(out, &yres);
        write_to_ostream(out, &col_num);
        write_to_ostream(out, &imp_col_num);
    }
};

bool host_is_little_endian() {
    u64 tst = 1;
    return *reinterpret_cast<u8*>(&tst) == 1;
}

bool fbdev::cmd_screenshot(const vector<string>& args, ostream& os) {
    if (!host_is_little_endian()) {
        os << "non-little-endian host not supported";
        return false;
    }

    if (m_mode.endian != ENDIAN_LITTLE) {
        os << "non-little-endian framebuffers not supported";
        return false;
    }

    if (m_mode.bpp > 4) {
        os << "framebuffers with more that 4 bytes per pixel are not "
              "supported";
        return false;
    }

    const u8 bits_per_pixel = 3 * 8;
    u32 extra_bits = ((32 - ((m_mode.xres * bits_per_pixel) % 32)) %
                      32); // num. of bytes per line needs to be multiple of 4
    u32 padded_size = (m_mode.xres * bits_per_pixel + extra_bits) / 8 * yres;

    struct bmp_header header;
    struct dib_header dib_header;

    header.offset = header.header_size() + dib_header.header_size();
    header.size = header.offset + padded_size;

    dib_header.width = m_mode.xres;
    dib_header.height = m_mode.yres;
    dib_header.bits_per_pixel = bits_per_pixel;
    dib_header.image_size = padded_size;

    std::ofstream out(args.at(0), std::ios::binary);
    header.write(out);
    dib_header.write(out);

    u32 a_max = ((1 << m_mode.a.size) - 1);
    u32 r_max = ((1 << m_mode.r.size) - 1);
    u32 g_max = ((1 << m_mode.g.size) - 1);
    u32 b_max = ((1 << m_mode.b.size) - 1);
    u32 a_mask = a_max << m_mode.a.offset;
    u32 r_mask = r_max << m_mode.r.offset;
    u32 g_mask = g_max << m_mode.g.offset;
    u32 b_mask = b_max << m_mode.b.offset;

    for (u32 y = yres; y-- > 0;) {
        for (u32 x = 0; x < xres; ++x) {
            u32 pixel = 0;
            u8* base = m_vptr + (y * m_mode.stride + x * m_mode.bpp);

            for (size_t i = m_mode.bpp; i-- > 0;) {
                pixel <<= 8;
                pixel |= base[i];
            }

            u32 a = (pixel & a_mask) >> m_mode.a.offset;
            u32 r = (pixel & r_mask) >> m_mode.r.offset;
            u32 g = (pixel & g_mask) >> m_mode.g.offset;
            u32 b = (pixel & b_mask) >> m_mode.b.offset;

            float a_scale = static_cast<float>(a) / static_cast<float>(a_max);
            u8 r_scale = ((r * 255.0f) * a_scale) / r_max;
            u8 g_scale = ((g * 255.0f) * a_scale) / g_max;
            u8 b_scale = ((b * 255.0f) * a_scale) / b_max;

            out << b_scale << g_scale << r_scale;
        }
    }
    out.close();

    os << "screenshot stored in '" << args.at(0) << "'";
    return true;
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

    register_command("screenshot", 1, &fbdev::cmd_screenshot,
                     "store a screenshot of the framebuffer");
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

    if (!allow_dmi) {
        log_warn("fbdev requires DMI to be enabled");
        return;
    }

    m_vptr = out.lookup_dmi_ptr(vmem, VCML_ACCESS_READ);
    if (m_vptr == nullptr) {
        log_warn("failed to get DMI pointer for 0x%016llx", addr.get());
        return;
    }

    if (!m_console.has_display())
        return;

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
