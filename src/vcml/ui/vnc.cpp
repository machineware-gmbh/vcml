/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/vnc.h"

namespace vcml {
namespace ui {

static const unordered_map<u32, u32> VNC_KEYSYMS = {
    { 0x0030, KEYSYM_0 },
    { 0x0031, KEYSYM_1 },
    { 0x0032, KEYSYM_2 },
    { 0x0033, KEYSYM_3 },
    { 0x0034, KEYSYM_4 },
    { 0x0035, KEYSYM_5 },
    { 0x0036, KEYSYM_6 },
    { 0x0037, KEYSYM_7 },
    { 0x0038, KEYSYM_8 },
    { 0x0039, KEYSYM_9 },

    { 0x0041, KEYSYM_A },
    { 0x0042, KEYSYM_B },
    { 0x0043, KEYSYM_C },
    { 0x0044, KEYSYM_D },
    { 0x0045, KEYSYM_E },
    { 0x0046, KEYSYM_F },
    { 0x0047, KEYSYM_G },
    { 0x0048, KEYSYM_H },
    { 0x0049, KEYSYM_I },
    { 0x004a, KEYSYM_J },
    { 0x004b, KEYSYM_K },
    { 0x004c, KEYSYM_L },
    { 0x004d, KEYSYM_M },
    { 0x004e, KEYSYM_N },
    { 0x004f, KEYSYM_O },
    { 0x0050, KEYSYM_P },
    { 0x0051, KEYSYM_Q },
    { 0x0052, KEYSYM_R },
    { 0x0053, KEYSYM_S },
    { 0x0054, KEYSYM_T },
    { 0x0055, KEYSYM_U },
    { 0x0056, KEYSYM_V },
    { 0x0057, KEYSYM_W },
    { 0x0058, KEYSYM_X },
    { 0x0059, KEYSYM_Y },
    { 0x005a, KEYSYM_Z },
    { 0x0061, KEYSYM_a },
    { 0x0062, KEYSYM_b },
    { 0x0063, KEYSYM_c },
    { 0x0064, KEYSYM_d },
    { 0x0065, KEYSYM_e },
    { 0x0066, KEYSYM_f },
    { 0x0067, KEYSYM_g },
    { 0x0068, KEYSYM_h },
    { 0x0069, KEYSYM_i },
    { 0x006a, KEYSYM_j },
    { 0x006b, KEYSYM_k },
    { 0x006c, KEYSYM_l },
    { 0x006d, KEYSYM_m },
    { 0x006e, KEYSYM_n },
    { 0x006f, KEYSYM_o },
    { 0x0070, KEYSYM_p },
    { 0x0071, KEYSYM_q },
    { 0x0072, KEYSYM_r },
    { 0x0073, KEYSYM_s },
    { 0x0074, KEYSYM_t },
    { 0x0075, KEYSYM_u },
    { 0x0076, KEYSYM_v },
    { 0x0077, KEYSYM_w },
    { 0x0078, KEYSYM_x },
    { 0x0079, KEYSYM_y },
    { 0x007a, KEYSYM_z },

    { 0x0021, KEYSYM_EXCLAIM },
    { 0x0022, KEYSYM_DBLQUOTE },
    { 0x0023, KEYSYM_HASH },
    { 0x0024, KEYSYM_DOLLAR },
    { 0x0025, KEYSYM_PERCENT },
    { 0x0026, KEYSYM_AMPERSAND },
    { 0x0027, KEYSYM_QUOTE },
    { 0x0028, KEYSYM_LEFTPAR },
    { 0x0029, KEYSYM_RIGHTPAR },
    { 0x002a, KEYSYM_ASTERISK },
    { 0x002b, KEYSYM_PLUS },
    { 0x002c, KEYSYM_COMMA },
    { 0x002d, KEYSYM_MINUS },
    { 0x002e, KEYSYM_DOT },
    { 0x002f, KEYSYM_SLASH },
    { 0x003a, KEYSYM_COLON },
    { 0x003b, KEYSYM_SEMICOLON },
    { 0x003c, KEYSYM_LESS },
    { 0x003d, KEYSYM_EQUAL },
    { 0x003e, KEYSYM_GREATER },
    { 0x003f, KEYSYM_QUESTION },
    { 0x0040, KEYSYM_AT },
    { 0x005b, KEYSYM_LEFTBRACKET },
    { 0x005c, KEYSYM_BACKSLASH },
    { 0x005d, KEYSYM_RIGHTBRACKET },
    { 0x005e, KEYSYM_CARET },
    { 0x005f, KEYSYM_UNDERSCORE },
    { 0x0060, KEYSYM_BACKQUOTE },
    { 0x007b, KEYSYM_LEFTBRACE },
    { 0x007c, KEYSYM_PIPE },
    { 0x007d, KEYSYM_RIGHTBRACE },
    { 0x007e, KEYSYM_TILDE },

    { 0xff1b, KEYSYM_ESC },
    { 0xff0d, KEYSYM_ENTER },
    { 0xff08, KEYSYM_BACKSPACE },
    { 0x0020, KEYSYM_SPACE },
    { 0xff09, KEYSYM_TAB },
    { 0xffe1, KEYSYM_LEFTSHIFT },
    { 0xffe2, KEYSYM_RIGHTSHIFT },
    { 0xffe3, KEYSYM_LEFTCTRL },
    { 0xffe4, KEYSYM_RIGHTCTRL },
    { 0xffe9, KEYSYM_LEFTALT },
    { 0xffea, KEYSYM_RIGHTALT },
    { 0xffe7, KEYSYM_LEFTMETA },
    { 0xffe8, KEYSYM_RIGHTMETA },
    { 0xff67, KEYSYM_MENU },
    { 0xffe5, KEYSYM_CAPSLOCK },

    { 0xffbe, KEYSYM_F1 },
    { 0xffbf, KEYSYM_F2 },
    { 0xffc0, KEYSYM_F3 },
    { 0xffc1, KEYSYM_F4 },
    { 0xffc2, KEYSYM_F5 },
    { 0xffc3, KEYSYM_F6 },
    { 0xffc4, KEYSYM_F7 },
    { 0xffc5, KEYSYM_F8 },
    { 0xffc6, KEYSYM_F9 },
    { 0xffc7, KEYSYM_F10 },
    { 0xffc8, KEYSYM_F11 },
    { 0xffc9, KEYSYM_F12 },

    { 0xff61, KEYSYM_PRINT },
    { 0xff14, KEYSYM_SCROLLOCK },
    { 0xff13, KEYSYM_PAUSE },

    { 0xff63, KEYSYM_INSERT },
    { 0xffff, KEYSYM_DELETE },
    { 0xff50, KEYSYM_HOME },
    { 0xff57, KEYSYM_END },
    { 0xff55, KEYSYM_PAGEUP },
    { 0xff56, KEYSYM_PAGEDOWN },

    { 0xff51, KEYSYM_LEFT },
    { 0xff53, KEYSYM_RIGHT },
    { 0xff52, KEYSYM_UP },
    { 0xff54, KEYSYM_DOWN },

    { 0xff7f, KEYSYM_NUMLOCK },
    { 0xffb0, KEYSYM_KP0 },
    { 0xffb1, KEYSYM_KP1 },
    { 0xffb2, KEYSYM_KP2 },
    { 0xffb3, KEYSYM_KP3 },
    { 0xffb4, KEYSYM_KP4 },
    { 0xffb5, KEYSYM_KP5 },
    { 0xffb6, KEYSYM_KP6 },
    { 0xffb7, KEYSYM_KP7 },
    { 0xffb8, KEYSYM_KP8 },
    { 0xffb9, KEYSYM_KP9 },
    { 0xff8d, KEYSYM_KPENTER },
    { 0xffab, KEYSYM_KPPLUS },
    { 0xffad, KEYSYM_KPMINUS },
    { 0xffaa, KEYSYM_KPMUL },
    { 0xffaf, KEYSYM_KPDIV },
    { 0xffac, KEYSYM_KPDOT },
    { 0xff97, KEYSYM_KPUP },
    { 0xff99, KEYSYM_KPDOWN },
    { 0xff96, KEYSYM_KPLEFT },
    { 0xff98, KEYSYM_KPRIGHT },
    { 0xff95, KEYSYM_KPHOME },
    { 0xff9c, KEYSYM_KPEND },
    { 0xff9a, KEYSYM_KPPAGEUP },
    { 0xff9b, KEYSYM_KPPAGEDOWN },
    { 0xff9e, KEYSYM_KPINSERT },
    { 0xff9f, KEYSYM_KPDELETE },
};

static u32 vnc_keysym_to_vcml_keysym(u32 keysym) {
    const auto it = VNC_KEYSYMS.find(keysym);
    if (it != VNC_KEYSYMS.end())
        return it->second;
    return KEYSYM_NONE;
}

static vnc_pixelformat pixelformat_from_mode(const videomode& vm) {
    vnc_pixelformat format{};
    format.bpp = vm.bpp * 8;
    format.depth = vm.r.size + vm.g.size + vm.b.size;
    format.endian = vm.endian == ENDIAN_BIG;
    format.truecolor = 1;
    format.rmax = bitmask(vm.r.size);
    format.gmax = bitmask(vm.g.size);
    format.bmax = bitmask(vm.b.size);
    format.roff = vm.r.offset;
    format.goff = vm.g.offset;
    format.boff = vm.b.offset;
    return format;
}

static bool operator==(const vnc_pixelformat& a, const vnc_pixelformat& b) {
    return a.bpp == b.bpp && a.depth == b.depth && a.endian == b.endian &&
           a.truecolor == b.truecolor && a.rmax == b.rmax &&
           a.gmax == b.gmax && a.bmax == b.bmax && a.roff == b.roff &&
           a.goff == b.goff && a.boff == b.boff;
}

static bool operator!=(const vnc_pixelformat& a, const vnc_pixelformat& b) {
    return !operator==(a, b);
}

void vnc::send_framebuffer_raw() {
    u8* fb = framebuffer();
    size_t sz = framebuffer_size();
    m_socket.send(fb, sz);
}

static void push_hextile(vector<u8>& data, u32 x, u32 y, u32 w, u32 h) {
    assert(x < 16);
    assert(y < 16);
    assert(w > 0 && w <= 16);
    assert(h > 0 && h <= 16);
    data.push_back((x & 0xf) << 4 | (y & 0xf));
    data.push_back(((w - 1) & 0xf) << 4 | ((h - 1) & 0xf));
}

static void push_hextile(vector<u8>& d, u32 x, u32 y, u32 w, u32 h, u32 col) {
    d.push_back(col);
    d.push_back(col >> 8);
    d.push_back(col >> 16);
    d.push_back(col >> 24);
    push_hextile(d, x, y, w, h);
}

#ifdef MWR_GCC
// gcc 13+14 tsan falls over itself trying to compile this, so disable it
__attribute__((no_sanitize("thread")))
#endif
void vnc::send_framebuffer_hextile(u32 x, u32 y, u32 w, u32 h,
                                   optional<u32>& bg, optional<u32>& fg) {
    enum hextile_flags : u8 {
        HEXTILE_RAW = 1,
        HEXTILE_BACKGROUND = 2,
        HEXTILE_FOREGROUND = 4,
        HEXTILE_ANY_SUBRECTS = 8,
        HEXTILE_COLORED_SUBRECTS = 16,
    };

    const auto& vm = mode();
    size_t ntiles = 0, ncolors = 0, nback = 0, nfront = 0;

    u8* fb = framebuffer() + y * vm.stride + x * vm.bpp;
    u32 back = 0, front = 0;

    for (u32 j = 0; j < h; j++) {
        u32* pixel = (u32*)(fb + j * vm.stride);
        for (u32 i = 0; i < w; i++) {
            switch (ncolors) {
            case 0:
                back = pixel[i];
                ncolors++;
                break;
            case 1:
                if (pixel[i] != back) {
                    front = pixel[i];
                    ncolors++;
                }
                break;
            case 2:
                if (pixel[i] == back)
                    nback++;
                else if (pixel[i] == front)
                    nfront++;
                else
                    ncolors++;
                break;
            default:
                break;
            }
        }

        if (ncolors > 2)
            break;
    }

    if (ncolors > 1 && nfront > nback)
        std::swap(back, front);

    u8 flags = 0;
    if (!bg || *bg != back) {
        flags |= HEXTILE_BACKGROUND;
        bg = back;
    }

    if (ncolors > 1 && ncolors < 3 && (!fg || *fg != front)) {
        flags |= HEXTILE_FOREGROUND;
        fg = front;
    }

    vector<u8> tiles;
    switch (ncolors) {
    case 1: // single color tile
        break;
    case 2: { // tile with foreground and background
        flags |= HEXTILE_ANY_SUBRECTS;
        for (u32 j = 0; j < h; j++) {
            u32 left = w;
            u32* px = (u32*)(fb + j * vm.stride);
            for (u32 i = 0; i < w; i++, px++) {
                if (*px == fg)
                    left = min(left, i);
                else if (left < w) {
                    push_hextile(tiles, left, j, i - left, 1);
                    left = w;
                    ntiles++;
                }
            }

            if (left < w) {
                push_hextile(tiles, left, j, w - left, 1);
                ntiles++;
            }
        }
        break;
    }

    default: // multicolor tile
        flags |= HEXTILE_ANY_SUBRECTS;
        flags |= HEXTILE_COLORED_SUBRECTS;
        fg = std::nullopt;
        for (u32 j = 0; j < h; j++) {
            u32 left = w;
            optional<u32> color;
            u32* px = (u32*)(fb + j * vm.stride);
            for (u32 i = 0; i < w; i++, px++) {
                if (!color && *px != bg) {
                    color = *px;
                    left = i;
                }

                if (color && *px != *color) {
                    push_hextile(tiles, left, j, i - left, 1, *color);
                    ntiles++;
                    color = std::nullopt;
                    if (*px != bg) {
                        color = *px;
                        left = i;
                    }
                }
            }

            if (color) {
                push_hextile(tiles, left, j, w - left, 1, *color);
                color = std::nullopt;
                ntiles++;
            }
        }
        break;
    }

    // use raw encoding if tiles take up too much space
    if (tiles.size() > w * h * vm.bpp) {
        flags = HEXTILE_RAW;
        bg = std::nullopt;
        fg = std::nullopt;
    }

    m_socket.send(flags);
    if (flags & HEXTILE_RAW) {
        for (u32 j = 0; j < h; j++) {
            m_socket.send(fb, w * vm.bpp);
            fb += vm.stride;
        }
    }
    if (flags & HEXTILE_BACKGROUND)
        m_socket.send(*bg);
    if (flags & HEXTILE_FOREGROUND)
        m_socket.send(*fg);
    if (flags & HEXTILE_ANY_SUBRECTS) {
        m_socket.send((u8)ntiles);
        m_socket.send(tiles.data(), tiles.size());
    }
}

void vnc::send_framebuffer_hextile() {
    optional<u32> bg, fg;

    const videomode& vm = mode();
    for (u32 y = 0; y < vm.yres; y += 16) {
        for (u32 x = 0; x < vm.xres; x += 16) {
            u32 w = min(16u, vm.xres - x);
            u32 h = min(16u, vm.yres - y);
            send_framebuffer_hextile(x, y, w, h, bg, fg);
        }
    }
}

template <>
void vnc::send(const vnc_pixelformat& format) {
    send<u8>(format.bpp);
    send<u8>(format.depth);
    send<u8>(format.endian);
    send<u8>(format.truecolor);
    send<u16>(format.rmax);
    send<u16>(format.gmax);
    send<u16>(format.bmax);
    send<u8>(format.roff);
    send<u8>(format.goff);
    send<u8>(format.boff);
}

template <>
vnc_pixelformat vnc::recv() {
    vnc_pixelformat format;
    format.bpp = recv<u8>();
    format.depth = recv<u8>();
    format.endian = recv<u8>();
    format.truecolor = recv<u8>();
    format.rmax = recv<u16>();
    format.gmax = recv<u16>();
    format.bmax = recv<u16>();
    format.roff = recv<u8>();
    format.goff = recv<u8>();
    format.boff = recv<u8>();
    return format;
}

void vnc::handshake() {
    if (m_socket.is_connected())
        m_socket.disconnect();

    m_socket.listen(m_port);
    log_debug("listening on port %hu", m_port);
    if (!m_socket.accept())
        return;

    m_socket.unlisten();

    log_debug("connected to \"%s\"", m_socket.peer());

    // protocol handshake
    char proto[13]{};
    string rfb_3_8 = "RFB 003.008\n";
    m_socket.send(rfb_3_8);
    m_socket.recv(proto, 12);
    log_debug("client requests protocol version: %s", proto);
    if (proto != rfb_3_8) {
        auto err = mkstr("unsupported RFB protocol version: %s", proto);
        send<u8>(0);
        send<u32>(err.length());
        m_socket.send(err);
        VCML_REPORT("%s", err.c_str());
    }

    // security handshake
    send<u8>(1);
    send<u8>(1);
    u8 security = recv<u8>();
    log_debug("client requests security mode %hhu", security);
    if (security != 1) {
        auto err = mkstr("unsupported RFB security: %hhu", security);
        send<u32>(1);
        send<u32>(err.length());
        m_socket.send(err);
        VCML_REPORT("%s", err.c_str());
    }

    log_debug("reporting connection success");
    send<u32>(0);

    // client init
    u8 shared = recv<u8>();
    log_debug("received client init 0x%02hhx", shared);

    // server init
    log_debug("sending server init");
    const videomode& vm = mode();
    m_format = pixelformat_from_mode(vm);
    send<u16>(vm.xres);
    send<u16>(vm.yres);
    send(m_format);
    send_padding(3);
    send<u32>(strlen(name()));
    m_socket.send(name());

    log_debug("connection complete");
}

void vnc::handle_set_encodings(const vector<i32>& encodings) {
    // encodings come in the form of preferred to least preferred
    for (auto it = encodings.rbegin(); it != encodings.rend(); it++) {
        switch (*it) {
        case VNC_ENC_RAW:
            log_debug("raw encoding is supported");
            m_encoding = VNC_ENC_RAW;
            break;
        case VNC_ENC_HEXTILE:
            log_debug("hextile encoding is supported");
            m_encoding = VNC_ENC_HEXTILE;
            break;
        case VNC_ENC_CURSOR:
            log_debug("cursor extension is supported");
            break;
        case VNC_ENC_DESKTOP_SIZE:
            log_debug("desktop_size extension is supported");
            break;
        default:
            log_debug("ignoring unknown encoding 0x%08x", *it);
            break;
        }
    }
}

void vnc::handle_set_pixel_format(const vnc_pixelformat& fmt) {
    if (fmt != m_format) {
        log_error("vnc client selected unsupported pixelformat");
        VCML_REPORT("unsupported client pixelformat");
    }
}

void vnc::handle_framebuffer_request(u8 inc, u16 x, u16 y, u16 w, u16 h) {
    log_debug("handle_framebuffer_request inc:%hhu x:%hu y:%hu w:%hu h:%hu",
              inc, x, y, w, h);

    const videomode& vm = mode();
    send<u8>(VNC_FRAMEBUFFER_UPDATE);
    send_padding(1);
    send<u16>(1); // one square
    send<u16>(0); // x
    send<u16>(0); // y
    send<u16>(vm.xres);
    send<u16>(vm.yres);
    send<u32>(m_encoding);

    switch (m_encoding) {
    case VNC_ENC_RAW:
        send_framebuffer_raw();
        break;
    case VNC_ENC_HEXTILE:
        send_framebuffer_hextile();
        break;
    default:
        VCML_REPORT("unsupported VNC encoding: 0x%08x", m_encoding);
    }
}

void vnc::handle_key_event(u32 key, u8 down) {
    u32 sym = vnc_keysym_to_vcml_keysym(key);
    if (sym != KEYSYM_NONE)
        notify_key(sym, down);
}

void vnc::handle_ptr_event(u8 buttons, u16 x, u16 y) {
    u8 change = buttons ^ m_buttons;
    if (change & VNC_BTN_LEFT)
        notify_btn(BUTTON_LEFT, buttons & VNC_BTN_LEFT);
    if (change & VNC_BTN_MIDDLE)
        notify_btn(BUTTON_MIDDLE, buttons & VNC_BTN_MIDDLE);
    if (change & VNC_BTN_RIGHT)
        notify_btn(BUTTON_RIGHT, buttons & VNC_BTN_RIGHT);
    if (change & VNC_BTN_WHEEL_UP)
        notify_btn(BUTTON_WHEEL_UP, buttons & VNC_BTN_WHEEL_UP);
    if (change & VNC_BTN_WHEEL_DOWN)
        notify_btn(BUTTON_WHEEL_DOWN, buttons & VNC_BTN_WHEEL_DOWN);
    if (change & VNC_BTN_WHEEL_LEFT)
        notify_btn(BUTTON_WHEEL_LEFT, buttons & VNC_BTN_WHEEL_LEFT);
    if (change & VNC_BTN_WHEEL_RIGHT)
        notify_btn(BUTTON_WHEEL_RIGHT, buttons & VNC_BTN_WHEEL_RIGHT);

    m_buttons = buttons;

    if (m_ptr_x != x || m_ptr_y != y) {
        notify_pos(x, y);
        m_ptr_x = x;
        m_ptr_y = y;
    }
}

void vnc::handle_cut_text(const string& text) {
    log_debug("handle_cut_text: %s", text.c_str());
}

void vnc::handle_command() {
    u8 request = m_socket.recv_char();
    switch (request) {
    case VNC_SET_PIXEL_FORMAT: {
        recv_padding(3);
        auto fmt = recv<vnc_pixelformat>();
        recv_padding(3);
        handle_set_pixel_format(fmt);
        break;
    }

    case VNC_SET_ENCODINGS: {
        recv_padding(1);
        u16 num = recv<u16>();
        vector<i32> encodings;
        while (num--)
            encodings.push_back(recv<u32>());
        handle_set_encodings(encodings);
        break;
    }

    case VNC_FRAMEBUFFER_REQUEST: {
        u8 inc = recv<u8>();
        u16 x = recv<u16>();
        u16 y = recv<u16>();
        u16 w = recv<u16>();
        u16 h = recv<u16>();
        handle_framebuffer_request(inc, x, y, w, h);
        break;
    }

    case VNC_KEY_EVENT: {
        u8 down = recv<u8>();
        recv_padding(2);
        u32 key = recv<u32>();
        handle_key_event(key, down);
        break;
    }

    case VNC_POINTER_EVENT: {
        u8 buttons = recv<u8>();
        u16 x = recv<u16>();
        u16 y = recv<u16>();
        handle_ptr_event(buttons, x, y);
        break;
    }

    case VNC_CLIENT_CUT_TEXT: {
        recv_padding(3);
        u32 len = recv<u32>();
        string text(len, ' ');
        m_socket.recv(text.data(), len);
        handle_cut_text(text);
        break;
    }

    default:
        log_warn("unsupported VNC client request: 0x%02hhx", request);
    }
}

void vnc::run() {
    mwr::set_thread_name(mkstr("vnc_%u", dispno()));
    m_socket.listen(m_port);

    while (m_running && sim_running()) {
        try {
            handshake();
            while (m_running && m_socket.is_connected())
                handle_command();
        } catch (std::exception& ex) {
            if (sim_running())
                log.debug(ex);
        }
    }
}

vnc::vnc(u32 no):
    display("vnc", no),
    m_port(no), // vnc port = display number
    m_buttons(),
    m_ptr_x(),
    m_ptr_y(),
    m_encoding(VNC_ENC_RAW),
    m_socket(),
    m_running(),
    m_mutex(),
    m_thread() {
    VCML_ERROR_ON(no != (u32)m_port, "invalid port specified: %u", no);

    static bool debug_vnc = []() {
        auto env = mwr::getenv("VCML_DEBUG_VNC");
        return env && *env != "0";
    }();

    log.set_level(debug_vnc ? LOG_DEBUG : LOG_INFO);
}

vnc::~vnc() {
    // nothing to do
}

void vnc::init(const videomode& mode, u8* fb) {
    display::init(mode, fb);
    m_running = true;
    m_thread = thread(&vnc::run, this);
}

void vnc::shutdown() {
    m_running = false;
    if (m_socket.is_connected())
        m_socket.disconnect();
    if (m_socket.is_listening())
        m_socket.unlisten();
    if (m_thread.joinable())
        m_thread.join();

    display::shutdown();
}

display* vnc::create(u32 nr) {
    return new vnc(nr);
}

} // namespace ui
} // namespace vcml
