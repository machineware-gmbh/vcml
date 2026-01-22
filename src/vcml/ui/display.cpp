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

void display::handle_option(const string& option) {
    VCML_REPORT("%s: unsupported option \"%s\"", name(), option.c_str());
}

void display::attach(input* device) {
    m_inputs.push_back(device);
    if (!has_framebuffer())
        init(videomode::a8r8g8b8(320, 200), nullptr);
}

void display::detach(input* device) {
    stl_remove(m_inputs, device);
}

static bool parse_display(const string& name, string& type, u32& nr,
                          vector<string>& options) {
    size_t comma = name.find(',');
    string head = name.substr(0, comma);

    if (comma != string::npos)
        options = split(name.substr(comma + 1), ',');

    size_t colon = head.find(':');
    if (colon == string::npos)
        return false;

    type = head.substr(0, colon);
    string nr_str = head.substr(colon + 1);

    char* end;
    nr = strtoul(nr_str.c_str(), &end, 0);
    if (*end || nr_str.empty())
        return false;

    return true;
}

display* create_null(u32 id) {
    return new display("display", id);
}

unordered_map<string, display::create_fn> display::types;

VCML_DEFINE_UI_DISPLAY(null, create_null)
VCML_DEFINE_UI_DISPLAY(vnc, vnc::create)

#ifdef HAVE_SDL2
VCML_DEFINE_UI_DISPLAY(sdl, sdl::create)
#endif

unordered_map<u32, shared_ptr<display>> display::displays = {
    { 0, shared_ptr<display>(create_null(0)) } // no-op server
};

void display::define(const string& type, create_fn fn) {
    if (stl_contains(types, type))
        VCML_ERROR("display type '%s' already registered", type.c_str());
    types[type] = std::move(fn);
}

shared_ptr<display> display::lookup(const string& name) {
    if (mwr::getenv_or_default("VCML_NO_GUI", false))
        return nullptr;

    if (name.empty())
        return displays[0];

    u32 nr;
    string type;
    vector<string> options;

    if (!parse_display(name, type, nr, options))
        VCML_ERROR("cannot parse display name: %s", name.c_str());

    shared_ptr<display>& disp = displays[nr];
    if (disp == nullptr) {
        auto it = types.find(type);
        if (it == types.end()) {
            stringstream ss;
            ss << "unknown display '" << type << "', available displays:";
            for (const auto& avail : types)
                ss << " " << avail.first;
            VCML_REPORT("%s", ss.str().c_str());
        }

        disp.reset(it->second(nr));
    }

    for (const string& option : options)
        disp->handle_option(option);

    return disp;
}

} // namespace ui
} // namespace vcml
