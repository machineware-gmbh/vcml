/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/display.h"
#include "vcml/ui/vnc.h"

#ifdef HAVE_SDL2
#include "vcml/ui/sdl.h"
#endif

namespace vcml {
namespace ui {

display::display(const string& type, u32 nr):
    m_name(mkstr("%s:%u", type.c_str(), nr)),
    m_type(type),
    m_dispno(nr),
    m_mode(),
    m_fb(nullptr),
    m_nullfb(nullptr),
    log(m_name) {
}

display::~display() {
    // nothing to do
}

void display::init(const videomode& mode, u8* fbptr) {
    if (has_framebuffer())
        shutdown();

    m_mode = mode;
    m_fb = fbptr;

    if (m_fb == nullptr)
        m_fb = m_nullfb = new u8[mode.size]();
}

void display::render(u32 x, u32 y, u32 w, u32 h) {
    // nothing to do
}

void display::render() {
    // nothing to do
}

void display::shutdown() {
    if (m_nullfb)
        delete[] m_nullfb;
    m_fb = m_nullfb = nullptr;
}

void display::notify_key(u32 keysym, bool down) {
    for (auto input : m_inputs)
        input->notify_key(keysym, down);
}

void display::notify_btn(u32 button, bool down) {
    for (auto input : m_inputs)
        input->notify_btn(button, down);
}

void display::notify_pos(u32 x, u32 y) {
    for (auto input : m_inputs) {
        x = min(x, xres() - 1);
        y = min(y, yres() - 1);

        // normalise absolute positions to [0..10000]
        input->notify_pos((u64)(x * input->xmax()) / (xres() - 1),
                          (u64)(y * input->ymax()) / (yres() - 1));
    }
}

void display::attach(input* device) {
    m_inputs.push_back(device);
    if (!has_framebuffer())
        init(videomode::a8r8g8b8(320, 200), nullptr);
}

void display::detach(input* device) {
    stl_remove(m_inputs, device);
}

static bool parse_display(const string& name, string& id, u32& nr) {
    auto it = name.rfind(':');
    if (it == string::npos)
        return false;

    id = name.substr(0, it);
    nr = from_string<u32>(name.substr(it + 1, string::npos));
    return true;
}

display* display::create(u32 nr) {
    return new display("display", nr);
}

unordered_map<string, function<display*(u32)>> display::types = {
    { "null", display::create },
    { "vnc", vnc::create },
#ifdef HAVE_SDL2
    { "sdl", sdl::create },
#endif
};

unordered_map<string, shared_ptr<display>> display::displays = {
    { "", shared_ptr<display>(new display("", 0)) } // no-op server
};

shared_ptr<display> display::lookup(const string& name) {
    shared_ptr<display>& disp = displays[name];
    if (disp != nullptr)
        return disp;

    u32 nr;
    string type;

    if (!parse_display(name, type, nr))
        VCML_ERROR("cannot parse display name: %s", name.c_str());

    auto it = types.find(type);
    if (it == types.end()) {
        stringstream ss;
        ss << "unknown display '" << type << "', available displays:";
        for (const auto& avail : types)
            ss << " " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    disp.reset(it->second(nr));
    return disp;
}

void display::register_display_type(const string& type,
                                    function<display*(u32)> creator) {
    if (stl_contains(types, type))
        VCML_ERROR("display type '%s' already registered", type.c_str());
    types.insert({ type, std::move(creator) });
}

} // namespace ui
} // namespace vcml
