/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/backend.h"
#include "vcml/ui/icon.h"
#include "vcml/ui/vnc.h"

#ifdef HAVE_SDL2
#include "vcml/ui/sdl.h"
#endif

namespace vcml {
namespace ui {

static void draw_icon_centered(u8* fbptr, u32 width, u32 height) {
    MWR_ERROR_ON(width < ICON_WIDTH, "screen too small");
    MWR_ERROR_ON(height < ICON_HEIGHT, "screen too small");

    u32 sx = (width - ICON_WIDTH) / 2;
    u32 sy = (height - ICON_HEIGHT) / 2;

    for (u32 y = 0; y < ICON_HEIGHT; y++) {
        u8* dest = fbptr + ((sy + y) * width + sx) * sizeof(u32);
        const u32* src = ICON_DATA + y * ICON_WIDTH;
        memcpy(dest, src, ICON_WIDTH * sizeof(u32));
    }
}

backend::backend(const string& type, u32 id):
    m_name(mkstr("%s:%u", type.c_str(), id)),
    m_type(type),
    m_id(id),
    m_mode(),
    m_fb(nullptr),
    m_nullfb(nullptr),
    log(m_name) {
}

backend::~backend() {
    // nothing to do
}

void backend::init(const videomode& mode, u8* fbptr) {
    if (has_framebuffer())
        VCML_ERROR("ui backend %s already initialized", name());

    m_mode = mode;
    m_fb = fbptr;

    if (m_fb == nullptr)
        m_fb = m_nullfb = new u8[mode.size]();
}

void backend::reinit(const videomode& newmode, u8* newptr) {
    shutdown();
    init(newmode, newptr);
}

void backend::shutdown() {
    if (m_nullfb)
        delete[] m_nullfb;
    m_fb = m_nullfb = nullptr;
    m_mode.clear();
}

void backend::render(u32 x, u32 y, u32 w, u32 h) {
    // to be overloaded
}

void backend::notify_key(u32 keysym, bool down) {
    lock_guard<mutex> l(m_inputs_mtx);
    for (auto input : m_inputs)
        input->notify_key(keysym, down);
}

void backend::notify_btn(u32 button, bool down) {
    lock_guard<mutex> l(m_inputs_mtx);
    for (auto input : m_inputs)
        input->notify_btn(button, down);
}

void backend::notify_pos(u32 x, u32 y) {
    lock_guard<mutex> l(m_inputs_mtx);
    for (auto input : m_inputs) {
        x = min(x, xres() - 1);
        y = min(y, yres() - 1);

        // normalise absolute positions to [0..10000]
        input->notify_pos((u64)(x * input->xmax()) / (xres() - 1),
                          (u64)(y * input->ymax()) / (yres() - 1));
    }
}

void backend::handle_option(const string& option) {
    VCML_REPORT("%s: unsupported option \"%s\"", name(), option.c_str());
}

void backend::setup(const videomode& mode, u8* fbptr) {
    if (has_framebuffer())
        reinit(mode, fbptr);
    else
        init(mode, fbptr);
}

void backend::cleanup() {
    m_inputs.clear();
    shutdown();
}

void backend::attach(input* device) {
    log_info("attached to %s", device->input_name());
    lock_guard<mutex> l(m_inputs_mtx);
    m_inputs.push_back(device);
    if (!has_framebuffer()) {
        init(videomode::a8r8g8b8(320, 200), nullptr);
        draw_icon_centered(framebuffer(), xres(), yres());
    }
}

void backend::detach(input* device) {
    lock_guard<mutex> l(m_inputs_mtx);
    stl_remove(m_inputs, device);
}

static bool parse_backend_desc(const string& name, string& type, u32& nr,
                               vector<string>& options) {
    if (name.empty()) {
        type = "null";
        nr = 0;
        return true;
    }

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

backend* create_null(u32 id) {
    return new backend("null", id);
}

unordered_map<string, backend::create_fn> backend::types;

VCML_DEFINE_UI_BACKEND(null, create_null)
VCML_DEFINE_UI_BACKEND(vnc, vnc::create)

#ifdef HAVE_SDL2
VCML_DEFINE_UI_BACKEND(sdl, sdl::create)
#endif

void backend::define(const string& type, create_fn fn) {
    if (stl_contains(types, type))
        VCML_ERROR("ui backend '%s' already registered", type.c_str());
    types[type] = std::move(fn);
}

backend* backend::create(const string& desc) {
    if (mwr::getenv_or_default("VCML_NO_GUI", false))
        return nullptr;

    u32 nr;
    string type;
    vector<string> options;

    if (!parse_backend_desc(desc, type, nr, options))
        VCML_REPORT("cannot parse ui backend: %s", desc.c_str());

    auto it = types.find(type);
    if (it == types.end()) {
        stringstream ss;
        ss << "unknown ui backend '" << type << "', available backend:";
        for (const auto& avail : types)
            ss << " " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    backend* b = it->second(nr);
    for (const string& option : options)
        b->handle_option(option);
    return b;
}

void backend::destroy(backend* b) {
    b->shutdown();
    delete b;
}

} // namespace ui
} // namespace vcml
