/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/rfb.h"

namespace vcml {
namespace ui {

static const unordered_map<u32, u32> RFB_KEYSYMS = {
    { XK_0, KEYSYM_0 },
    { XK_1, KEYSYM_1 },
    { XK_2, KEYSYM_2 },
    { XK_3, KEYSYM_3 },
    { XK_4, KEYSYM_4 },
    { XK_5, KEYSYM_5 },
    { XK_6, KEYSYM_6 },
    { XK_7, KEYSYM_7 },
    { XK_8, KEYSYM_8 },
    { XK_9, KEYSYM_9 },

    { XK_A, KEYSYM_A },
    { XK_B, KEYSYM_B },
    { XK_C, KEYSYM_C },
    { XK_D, KEYSYM_D },
    { XK_E, KEYSYM_E },
    { XK_F, KEYSYM_F },
    { XK_G, KEYSYM_G },
    { XK_H, KEYSYM_H },
    { XK_I, KEYSYM_I },
    { XK_J, KEYSYM_J },
    { XK_K, KEYSYM_K },
    { XK_L, KEYSYM_L },
    { XK_M, KEYSYM_M },
    { XK_N, KEYSYM_N },
    { XK_O, KEYSYM_O },
    { XK_P, KEYSYM_P },
    { XK_Q, KEYSYM_Q },
    { XK_R, KEYSYM_R },
    { XK_S, KEYSYM_S },
    { XK_T, KEYSYM_T },
    { XK_U, KEYSYM_U },
    { XK_V, KEYSYM_V },
    { XK_W, KEYSYM_W },
    { XK_X, KEYSYM_X },
    { XK_Y, KEYSYM_Y },
    { XK_Z, KEYSYM_Z },
    { XK_a, KEYSYM_a },
    { XK_b, KEYSYM_b },
    { XK_c, KEYSYM_c },
    { XK_d, KEYSYM_d },
    { XK_e, KEYSYM_e },
    { XK_f, KEYSYM_f },
    { XK_g, KEYSYM_g },
    { XK_h, KEYSYM_h },
    { XK_i, KEYSYM_i },
    { XK_j, KEYSYM_j },
    { XK_k, KEYSYM_k },
    { XK_l, KEYSYM_l },
    { XK_m, KEYSYM_m },
    { XK_n, KEYSYM_n },
    { XK_o, KEYSYM_o },
    { XK_p, KEYSYM_p },
    { XK_q, KEYSYM_q },
    { XK_r, KEYSYM_r },
    { XK_s, KEYSYM_s },
    { XK_t, KEYSYM_t },
    { XK_u, KEYSYM_u },
    { XK_v, KEYSYM_v },
    { XK_w, KEYSYM_w },
    { XK_x, KEYSYM_x },
    { XK_y, KEYSYM_y },
    { XK_z, KEYSYM_z },

    { XK_exclam, KEYSYM_EXCLAIM },
    { XK_quotedbl, KEYSYM_DBLQUOTE },
    { XK_numbersign, KEYSYM_HASH },
    { XK_dollar, KEYSYM_DOLLAR },
    { XK_percent, KEYSYM_PERCENT },
    { XK_ampersand, KEYSYM_AMPERSAND },
    { XK_apostrophe, KEYSYM_QUOTE },
    { XK_parenleft, KEYSYM_LEFTPAR },
    { XK_parenright, KEYSYM_RIGHTPAR },
    { XK_asterisk, KEYSYM_ASTERISK },
    { XK_plus, KEYSYM_PLUS },
    { XK_comma, KEYSYM_COMMA },
    { XK_minus, KEYSYM_MINUS },
    { XK_period, KEYSYM_DOT },
    { XK_slash, KEYSYM_SLASH },
    { XK_colon, KEYSYM_COLON },
    { XK_semicolon, KEYSYM_SEMICOLON },
    { XK_less, KEYSYM_LESS },
    { XK_equal, KEYSYM_EQUAL },
    { XK_greater, KEYSYM_GREATER },
    { XK_question, KEYSYM_QUESTION },
    { XK_at, KEYSYM_AT },
    { XK_bracketleft, KEYSYM_LEFTBRACKET },
    { XK_backslash, KEYSYM_BACKSLASH },
    { XK_bracketright, KEYSYM_RIGHTBRACKET },
    { XK_asciicircum, KEYSYM_CARET },
    { XK_underscore, KEYSYM_UNDERSCORE },
    { XK_quoteleft, KEYSYM_BACKQUOTE },
    { XK_braceleft, KEYSYM_LEFTBRACE },
    { XK_bar, KEYSYM_PIPE },
    { XK_braceright, KEYSYM_RIGHTBRACE },
    { XK_asciitilde, KEYSYM_TILDE },

    { XK_Escape, KEYSYM_ESC },
    { XK_Return, KEYSYM_ENTER },
    { XK_BackSpace, KEYSYM_BACKSPACE },
    { XK_space, KEYSYM_SPACE },
    { XK_Tab, KEYSYM_TAB },
    { XK_Shift_L, KEYSYM_LEFTSHIFT },
    { XK_Shift_R, KEYSYM_RIGHTSHIFT },
    { XK_Control_L, KEYSYM_LEFTCTRL },
    { XK_Control_R, KEYSYM_RIGHTCTRL },
    { XK_Alt_L, KEYSYM_LEFTALT },
    { XK_Alt_R, KEYSYM_RIGHTALT },
    { XK_Meta_L, KEYSYM_LEFTMETA },
    { XK_Meta_R, KEYSYM_RIGHTMETA },
    { XK_Menu, KEYSYM_MENU },
    { XK_Caps_Lock, KEYSYM_CAPSLOCK },

    { XK_F1, KEYSYM_F1 },
    { XK_F2, KEYSYM_F2 },
    { XK_F3, KEYSYM_F3 },
    { XK_F4, KEYSYM_F4 },
    { XK_F5, KEYSYM_F5 },
    { XK_F6, KEYSYM_F6 },
    { XK_F7, KEYSYM_F7 },
    { XK_F8, KEYSYM_F8 },
    { XK_F9, KEYSYM_F9 },
    { XK_F10, KEYSYM_F10 },
    { XK_F11, KEYSYM_F11 },
    { XK_F12, KEYSYM_F12 },

    { XK_Print, KEYSYM_PRINT },
    { XK_Scroll_Lock, KEYSYM_SCROLLOCK },
    { XK_Pause, KEYSYM_PAUSE },

    { XK_Insert, KEYSYM_INSERT },
    { XK_Delete, KEYSYM_DELETE },
    { XK_Home, KEYSYM_HOME },
    { XK_End, KEYSYM_END },
    { XK_Page_Up, KEYSYM_PAGEUP },
    { XK_Page_Down, KEYSYM_PAGEDOWN },

    { XK_Left, KEYSYM_LEFT },
    { XK_Right, KEYSYM_RIGHT },
    { XK_Up, KEYSYM_UP },
    { XK_Down, KEYSYM_DOWN },

    { XK_Num_Lock, KEYSYM_NUMLOCK },
    { XK_KP_0, KEYSYM_KP0 },
    { XK_KP_1, KEYSYM_KP1 },
    { XK_KP_2, KEYSYM_KP2 },
    { XK_KP_3, KEYSYM_KP3 },
    { XK_KP_4, KEYSYM_KP4 },
    { XK_KP_5, KEYSYM_KP5 },
    { XK_KP_6, KEYSYM_KP6 },
    { XK_KP_7, KEYSYM_KP7 },
    { XK_KP_8, KEYSYM_KP8 },
    { XK_KP_9, KEYSYM_KP9 },
    { XK_KP_Enter, KEYSYM_KPENTER },
    { XK_KP_Add, KEYSYM_KPPLUS },
    { XK_KP_Subtract, KEYSYM_KPMINUS },
    { XK_KP_Multiply, KEYSYM_KPMUL },
    { XK_KP_Divide, KEYSYM_KPDIV },
    { XK_KP_Separator, KEYSYM_KPDOT },
    { XK_KP_Up, KEYSYM_KPUP },
    { XK_KP_Down, KEYSYM_KPDOWN },
    { XK_KP_Left, KEYSYM_KPLEFT },
    { XK_KP_Right, KEYSYM_KPRIGHT },
    { XK_KP_Home, KEYSYM_KPHOME },
    { XK_KP_End, KEYSYM_KPEND },
    { XK_KP_Page_Up, KEYSYM_KPPAGEUP },
    { XK_KP_Page_Down, KEYSYM_KPPAGEDOWN },
    { XK_KP_Insert, KEYSYM_KPINSERT },
    { XK_KP_Delete, KEYSYM_KPDELETE },
};

static u32 rfb_keysym_to_vcml_keysym(u32 keysym) {
    const auto it = RFB_KEYSYMS.find(keysym);
    if (it != RFB_KEYSYMS.end())
        return it->second;
    return KEYSYM_NONE;
}

static void rfb_key_func(char down, unsigned int sym, unsigned short port) {
    auto disp = display::lookup(mkstr("rfb:%d", port));
    VCML_ERROR_ON(!disp, "no display found for port %d", port);
    auto rfb_server = dynamic_cast<rfb*>(disp.get());
    VCML_ERROR_ON(!rfb_server, "no librfb server found for port %d", port);
    rfb_server->key_event((u32)sym, (bool)down);
}

static void rfb_ptr_func(char mask, unsigned short x, unsigned short y,
                         unsigned short port) {
    auto disp = display::lookup(mkstr("rfb:%d", port));
    VCML_ERROR_ON(!disp, "no display found for port %d", port);
    auto rfb_server = dynamic_cast<rfb*>(disp.get());
    VCML_ERROR_ON(!rfb_server, "no librfb server found for port %d", port);
    rfb_server->ptr_event((u32)mask, (u32)x, (u32)y);
}

void rfb::run() {
    const videomode& fbm = mode();

    RfbConfig rc;
    rc.pixel_fmt.bits_per_pixel = fbm.bpp * 8;
    rc.pixel_fmt.depth = 24;
    rc.pixel_fmt.be = fbm.endian == ENDIAN_BIG;
    rc.pixel_fmt.true_color = true;
    rc.pixel_fmt.rmax = (1 << fbm.r.size) - 1;
    rc.pixel_fmt.gmax = (1 << fbm.g.size) - 1;
    rc.pixel_fmt.bmax = (1 << fbm.b.size) - 1;
    rc.pixel_fmt.rshift = fbm.r.offset;
    rc.pixel_fmt.gshift = fbm.g.offset;
    rc.pixel_fmt.bshift = fbm.b.offset;
    rc.framebuffer = framebuffer();
    rc.width = fbm.xres;
    rc.height = fbm.yres;
    rc.server_name = name();
    rc.port_ipv4 = m_port;
    rc.key_callback = &rfb_key_func;
    rc.ptr_callback = &rfb_ptr_func;

    m_screen = rfb_init_server(rc);

    log_debug("starting librfb server on port %d", m_port);

    while (m_running && sim_running())
        rfb_poll_work(m_screen);

    log_debug("terminating librfb server on port %d", m_port);

    rfb_shutdown_server(m_screen);
}

rfb::rfb(u32 no):
    display("rfb", no),
    m_port(no), // rfb port = display number
    m_buttons(),
    m_ptr_x(),
    m_ptr_y(),
    m_running(false),
    m_mutex(),
    m_screen(),
    m_thread() {
    VCML_ERROR_ON(no != (u32)m_port, "invalid port specified: %u", no);
}

rfb::~rfb() {
    // nothing to do
}

void rfb::init(const videomode& mode, u8* fb) {
    display::init(mode, fb);

    m_running = true;
    m_thread = thread(&rfb::run, this);
    set_thread_name(m_thread, name());
}

void rfb::shutdown() {
    if (m_thread.joinable()) {
        m_running = false;
        m_thread.join();
    }

    display::shutdown();
}

void rfb::key_event(u32 sym, bool down) {
    u32 symbol = rfb_keysym_to_vcml_keysym(sym);
    if (symbol != KEYSYM_NONE)
        notify_key(symbol, down);
}

enum rfb_buttons : u32 {
    RFB_BTN_LEFT = 1 << 0,
    RFB_BTN_MIDDLE = 1 << 1,
    RFB_BTN_RIGHT = 1 << 2,
};

void rfb::ptr_event(u32 mask, u32 x, u32 y) {
    u32 change = mask ^ m_buttons;
    if (change & RFB_BTN_LEFT)
        notify_btn(BUTTON_LEFT, mask & RFB_BTN_LEFT);
    if (change & RFB_BTN_MIDDLE)
        notify_btn(BUTTON_MIDDLE, mask & RFB_BTN_MIDDLE);
    if (change & RFB_BTN_RIGHT)
        notify_btn(BUTTON_RIGHT, mask & RFB_BTN_RIGHT);
    m_buttons = mask;

    if (m_ptr_x != x || m_ptr_y != y) {
        display::notify_rel(x - m_ptr_x, y - m_ptr_y, 0);
        m_ptr_x = x;
        m_ptr_y = y;
    }
}

display* rfb::create(u32 nr) {
    return new rfb(nr);
}

} // namespace ui
} // namespace vcml
