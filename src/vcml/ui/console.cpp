/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/console.h"
#include "vcml/core/module.h"

namespace vcml {
namespace ui {

size_t console::attach_display(const string& type) {
    auto display = display::lookup(type);
    if (m_mode.is_valid())
        display->init(m_mode, m_fbptr);
    m_displays[m_next_id] = display;
    return m_next_id++;
}

bool console::detach_display(size_t id) {
    auto it = m_displays.find(id);
    if (it == m_displays.end())
        return false;
    if (m_mode.is_valid())
        it->second->shutdown();
    display::remove(it->second);
    m_displays.erase(it);
    return true;
}

bool console::cmd_attach_display(const vector<string>& args, ostream& os) {
    try {
        size_t id = attach_display(args[0]);
        os << "created display " << id;
        return true;
    } catch (std::exception& ex) {
        os << "error creating display " << args[0] << ":" << ex.what();
        return false;
    }
}

bool console::cmd_detach_display(const vector<string>& args, ostream& os) {
    for (const string& arg : args) {
        if (to_lower(arg) == "all") {
            m_displays.clear();
            return true;
        } else {
            size_t id = from_string<size_t>(arg);
            if (!detach_display(id))
                os << "invalid backend id: " << id;
        }
    }

    return true;
}

bool console::cmd_list_displays(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        for (auto& it : m_displays)
            os << it.first << ": " << it.second->type() << ",";
        return true;
    }

    for (const string& arg : args) {
        size_t id = from_string<size_t>(arg);

        auto it = m_displays.find(id);
        os << id << ": ";
        os << (it == m_displays.end() ? "none" : it->second->type());
        os << ",";
    }

    return true;
}

bool console::cmd_screenshot(const vector<string>& args, ostream& os) {
    string path = mkstr("%s.bmp", m_parent ? m_parent->name() : "screenshot");
    if (args.size() > 0)
        path = args[0];

    if (screenshot(path)) {
        os << "screenshot stored in '" << path << "'";
        return true;
    }

    os << "failed to store screenshot in '" << path << "'";
    return false;
}

console::console(sc_object* parent):
    m_parent(parent),
    m_fbptr(),
    m_mode(),
    m_next_id(),
    m_inputs(),
    m_displays(),
    displays(parent, "displays") {
    for (const string& type : displays) {
        try {
            attach_display(type);
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
        }
    }

    if (module* host = dynamic_cast<module*>(parent)) {
        host->register_command(
            "attach_display", 1, this, &console::cmd_attach_display,
            "creates a new display of a given type and attaches it to this "
            "console, usage: attach_display <type>");
        host->register_command(
            "detach_display", 1, this, &console::cmd_detach_display,
            "disconnects a display from this console with the given IDs, "
            "usage: detach_display <ID> [ID]..| all");
        host->register_command(
            "list_displays", 0, this, &console::cmd_list_displays,
            "lists all known displays that are attached to this console");
        host->register_command(
            "screenshot", 0, this, &console::cmd_screenshot,
            "store a screenshot of the framebuffer, usage: screenshot [path]");
    }
}

console::~console() {
    shutdown();
}

void console::notify(input& device) {
    m_inputs.insert(&device);
    for (auto& [id, disp] : m_displays)
        disp->attach(&device);
}

void console::setup(const videomode& mode, u8* fbptr) {
    for (auto& [id, disp] : m_displays)
        disp->setup(mode, fbptr);

    m_mode = mode;
    m_fbptr = fbptr;
}

void console::render(u32 x, u32 y, u32 w, u32 h) {
    for (auto& [id, disp] : m_displays)
        disp->render(x, y, w, h);
}

void console::render() {
    for (auto& [id, disp] : m_displays)
        disp->render();
}

void console::shutdown() {
    m_mode.clear();
    m_fbptr = nullptr;

    for (auto& [id, disp] : m_displays)
        disp->cleanup();

    m_inputs.clear();
    m_displays.clear();
}

struct bmp_header {
    char header[2]; // 0x42 0x4d for BMP (BM)
    u32 size;       // total size (bytes)
    u16 res0;
    u16 res1;
    u32 offset; // offset of the pixel array
};

struct dib_header {
    u32 size;           // size of this header (bytes) -> 40
    i32 width;          // in pixels
    i32 height;         // in pixels
    u16 color_planes;   // number of color planes -> 1
    u16 bits_per_pixel; // typically 1, 4, 5, 16, 24, 32
    u32 method;         // compression method
    u32 image_size;     // raw bitmap size; 0 for BI_RGB bitmaps
    i32 xres;           // (pixel per metre, signed integer)
    i32 yres;           // (pixel per metre, signed integer)
    u32 col_num;        // colors in the color palette; 0 default to 2^n
    u32 imp_col;        // important colors used; 0 means all
};

template <typename T>
inline void write_binary(ostream& os, const T& val) {
    os.write((const char*)&val, sizeof(T));
}

template <>
inline void write_binary<bmp_header>(ostream& os, const bmp_header& h) {
    write_binary(os, h.header[0]);
    write_binary(os, h.header[1]);
    write_binary(os, h.size);
    write_binary(os, h.res0);
    write_binary(os, h.res1);
    write_binary(os, h.offset);
}

template <>
inline void write_binary<dib_header>(ostream& os, const dib_header& h) {
    write_binary(os, h.size);
    write_binary(os, h.width);
    write_binary(os, h.height);
    write_binary(os, h.color_planes);
    write_binary(os, h.bits_per_pixel);
    write_binary(os, h.method);
    write_binary(os, h.image_size);
    write_binary(os, h.xres);
    write_binary(os, h.yres);
    write_binary(os, h.col_num);
    write_binary(os, h.imp_col);
}

bool console::screenshot(const string& path) const {
    // need to have a framebuffer and a sane pixelformat
    if (!m_fbptr || m_mode.bpp > 4 || m_mode.grayscale)
        return false;

    // lines must be multiples of 32bits long
    u32 padding = (4 - ((m_mode.xres * 3) % 4)) % 4;
    u32 stride = m_mode.xres * 3 + padding;
    u32 size = stride * m_mode.yres;

    bmp_header bmp{};
    dib_header dib{};

    dib.size = 40;
    dib.width = m_mode.xres;
    dib.height = m_mode.yres;
    dib.color_planes = 1;
    dib.bits_per_pixel = 24;
    dib.method = 0;
    dib.image_size = size;
    dib.xres = 0;
    dib.yres = 0;
    dib.col_num = 0;
    dib.imp_col = 0;

    bmp.header[0] = 'B';
    bmp.header[1] = 'M';
    bmp.offset = 14 + dib.size;
    bmp.size = bmp.offset + size;

    ofstream stream(path, std::ios::binary);
    if (!stream)
        return false;

    write_binary(stream, bmp);
    write_binary(stream, dib);

    u32 r_mask = bitmask(m_mode.r.size);
    u32 g_mask = bitmask(m_mode.g.size);
    u32 b_mask = bitmask(m_mode.b.size);

    for (u32 y = m_mode.yres; y-- > 0;) {
        for (u32 x = 0; x < m_mode.xres; x++) {
            u32 pixel = read_pixel(x, y);

            u8 r = (pixel >> m_mode.r.offset) & r_mask;
            u8 g = (pixel >> m_mode.g.offset) & g_mask;
            u8 b = (pixel >> m_mode.b.offset) & b_mask;

            write_binary(stream, b);
            write_binary(stream, g);
            write_binary(stream, r);
        }

        for (u32 i = 0; i < padding; i++)
            write_binary<u8>(stream, 0);
    }

    return true;
}

} // namespace ui
} // namespace vcml
