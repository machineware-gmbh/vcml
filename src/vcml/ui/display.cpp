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

#ifdef HAVE_LIBVNC
#include "vcml/ui/vnc.h"
#endif

#ifdef HAVE_SDL2
#include "vcml/ui/sdl.h"
#endif

#ifdef HAVE_LIBRFB
#include "vcml/ui/rfb.h"
#endif

namespace vcml {
namespace ui {

display::display(const string& type, u32 nr):
    m_name(mkstr("%s:%u", type.c_str(), nr)),
    m_type(type),
    m_dispno(nr),
    m_mode(),
    m_fb(nullptr),
    m_nullfb(nullptr) {
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
    for (keyboard* kb : m_keyboards)
        kb->notify_key(keysym, down);
}

void display::notify_btn(u32 button, bool down) {
    for (pointer* ptr : m_pointers)
        ptr->notify_btn(button, down);
}

void display::notify_rel(i32 x, i32 y, i32 w) {
    for (pointer* ptr : m_pointers)
        ptr->notify_rel(x, y, w);
}

void display::add_keyboard(keyboard* kb) {
    m_keyboards.push_back(kb);
    if (!has_framebuffer())
        init(videomode::a8r8g8b8(320, 200), nullptr);
}

void display::add_pointer(pointer* ptr) {
    m_pointers.push_back(ptr);
    if (!has_framebuffer())
        init(videomode::a8r8g8b8(320, 200), nullptr);
}

void display::remove_keyboard(keyboard* kb) {
    stl_remove(m_keyboards, kb);
}

void display::remove_pointer(pointer* ptr) {
    stl_remove(m_pointers, ptr);
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
#ifdef HAVE_LIBVNC
    { "vnc", vnc::create },
#endif
#ifdef HAVE_SDL2
    { "sdl", sdl::create },
#endif
#ifdef HAVE_LIBRFB
    { "rfb", rfb::create },
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
