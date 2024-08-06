/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/sdl.h"

namespace vcml {
namespace ui {

static const unordered_map<u32, u32> SDL_KEYSYMS = {
    { SDLK_0, KEYSYM_0 },
    { SDLK_1, KEYSYM_1 },
    { SDLK_2, KEYSYM_2 },
    { SDLK_3, KEYSYM_3 },
    { SDLK_4, KEYSYM_4 },
    { SDLK_5, KEYSYM_5 },
    { SDLK_6, KEYSYM_6 },
    { SDLK_7, KEYSYM_7 },
    { SDLK_8, KEYSYM_8 },
    { SDLK_9, KEYSYM_9 },

    { 'A', KEYSYM_A },
    { 'B', KEYSYM_B },
    { 'C', KEYSYM_C },
    { 'D', KEYSYM_D },
    { 'E', KEYSYM_E },
    { 'F', KEYSYM_F },
    { 'G', KEYSYM_G },
    { 'H', KEYSYM_H },
    { 'I', KEYSYM_I },
    { 'J', KEYSYM_J },
    { 'K', KEYSYM_K },
    { 'L', KEYSYM_L },
    { 'M', KEYSYM_M },
    { 'N', KEYSYM_N },
    { 'O', KEYSYM_O },
    { 'P', KEYSYM_P },
    { 'Q', KEYSYM_Q },
    { 'R', KEYSYM_R },
    { 'S', KEYSYM_S },
    { 'T', KEYSYM_T },
    { 'U', KEYSYM_U },
    { 'V', KEYSYM_V },
    { 'W', KEYSYM_W },
    { 'X', KEYSYM_X },
    { 'Y', KEYSYM_Y },
    { 'Z', KEYSYM_Z },

    { SDLK_a, KEYSYM_a },
    { SDLK_b, KEYSYM_b },
    { SDLK_c, KEYSYM_c },
    { SDLK_d, KEYSYM_d },
    { SDLK_e, KEYSYM_e },
    { SDLK_f, KEYSYM_f },
    { SDLK_g, KEYSYM_g },
    { SDLK_h, KEYSYM_h },
    { SDLK_i, KEYSYM_i },
    { SDLK_j, KEYSYM_j },
    { SDLK_k, KEYSYM_k },
    { SDLK_l, KEYSYM_l },
    { SDLK_m, KEYSYM_m },
    { SDLK_n, KEYSYM_n },
    { SDLK_o, KEYSYM_o },
    { SDLK_p, KEYSYM_p },
    { SDLK_q, KEYSYM_q },
    { SDLK_r, KEYSYM_r },
    { SDLK_s, KEYSYM_s },
    { SDLK_t, KEYSYM_t },
    { SDLK_u, KEYSYM_u },
    { SDLK_v, KEYSYM_v },
    { SDLK_w, KEYSYM_w },
    { SDLK_x, KEYSYM_x },
    { SDLK_y, KEYSYM_y },
    { SDLK_z, KEYSYM_z },

    { SDLK_EXCLAIM, KEYSYM_EXCLAIM },
    { SDLK_QUOTEDBL, KEYSYM_DBLQUOTE },
    { SDLK_HASH, KEYSYM_HASH },
    { SDLK_DOLLAR, KEYSYM_DOLLAR },
    { SDLK_PERCENT, KEYSYM_PERCENT },
    { SDLK_AMPERSAND, KEYSYM_AMPERSAND },
    { SDLK_QUOTE, KEYSYM_QUOTE },
    { SDLK_LEFTPAREN, KEYSYM_LEFTPAR },
    { SDLK_RIGHTPAREN, KEYSYM_RIGHTPAR },
    { SDLK_ASTERISK, KEYSYM_ASTERISK },
    { SDLK_PLUS, KEYSYM_PLUS },
    { SDLK_COMMA, KEYSYM_COMMA },
    { SDLK_MINUS, KEYSYM_MINUS },
    { SDLK_PERIOD, KEYSYM_DOT },
    { SDLK_SLASH, KEYSYM_SLASH },
    { SDLK_COLON, KEYSYM_COLON },
    { SDLK_SEMICOLON, KEYSYM_SEMICOLON },
    { SDLK_LESS, KEYSYM_LESS },
    { SDLK_EQUALS, KEYSYM_EQUAL },
    { SDLK_GREATER, KEYSYM_GREATER },
    { SDLK_QUESTION, KEYSYM_QUESTION },
    { SDLK_AT, KEYSYM_AT },
    { SDLK_LEFTBRACKET, KEYSYM_LEFTBRACKET },
    { SDLK_BACKSLASH, KEYSYM_BACKSLASH },
    { SDLK_RIGHTBRACKET, KEYSYM_RIGHTBRACKET },
    { SDLK_CARET, KEYSYM_CARET },
    { SDLK_UNDERSCORE, KEYSYM_UNDERSCORE },
    { SDLK_BACKQUOTE, KEYSYM_BACKQUOTE },
    { '{', KEYSYM_LEFTBRACE },
    { '|', KEYSYM_PIPE },
    { '}', KEYSYM_RIGHTBRACE },
    { '~', KEYSYM_TILDE },

    { SDLK_ESCAPE, KEYSYM_ESC },
    { SDLK_RETURN, KEYSYM_ENTER },
    { SDLK_BACKSPACE, KEYSYM_BACKSPACE },
    { SDLK_SPACE, KEYSYM_SPACE },
    { SDLK_TAB, KEYSYM_TAB },
    { SDLK_LSHIFT, KEYSYM_LEFTSHIFT },
    { SDLK_RSHIFT, KEYSYM_RIGHTSHIFT },
    { SDLK_LCTRL, KEYSYM_LEFTCTRL },
    { SDLK_RCTRL, KEYSYM_RIGHTCTRL },
    { SDLK_LALT, KEYSYM_LEFTALT },
    { SDLK_RALT, KEYSYM_RIGHTALT },
    { SDLK_LGUI, KEYSYM_LEFTMETA },
    { SDLK_RGUI, KEYSYM_RIGHTMETA },
    { SDLK_MENU, KEYSYM_MENU },
    { SDLK_CAPSLOCK, KEYSYM_CAPSLOCK },

    { SDLK_F1, KEYSYM_F1 },
    { SDLK_F2, KEYSYM_F2 },
    { SDLK_F3, KEYSYM_F3 },
    { SDLK_F4, KEYSYM_F4 },
    { SDLK_F5, KEYSYM_F5 },
    { SDLK_F6, KEYSYM_F6 },
    { SDLK_F7, KEYSYM_F7 },
    { SDLK_F8, KEYSYM_F8 },
    { SDLK_F9, KEYSYM_F9 },
    { SDLK_F10, KEYSYM_F10 },
    { SDLK_F11, KEYSYM_F11 },
    { SDLK_F12, KEYSYM_F12 },

    { SDLK_PRINTSCREEN, KEYSYM_PRINT },
    { SDLK_SCROLLLOCK, KEYSYM_SCROLLOCK },
    { SDLK_PAUSE, KEYSYM_PAUSE },

    { SDLK_INSERT, KEYSYM_INSERT },
    { SDLK_DELETE, KEYSYM_DELETE },
    { SDLK_HOME, KEYSYM_HOME },
    { SDLK_END, KEYSYM_END },
    { SDLK_PAGEUP, KEYSYM_PAGEUP },
    { SDLK_PAGEDOWN, KEYSYM_PAGEDOWN },

    { SDLK_LEFT, KEYSYM_LEFT },
    { SDLK_RIGHT, KEYSYM_RIGHT },
    { SDLK_UP, KEYSYM_UP },
    { SDLK_DOWN, KEYSYM_DOWN },

    { SDLK_NUMLOCKCLEAR, KEYSYM_NUMLOCK },
    { SDLK_KP_0, KEYSYM_KP0 },
    { SDLK_KP_1, KEYSYM_KP1 },
    { SDLK_KP_2, KEYSYM_KP2 },
    { SDLK_KP_3, KEYSYM_KP3 },
    { SDLK_KP_4, KEYSYM_KP4 },
    { SDLK_KP_5, KEYSYM_KP5 },
    { SDLK_KP_6, KEYSYM_KP6 },
    { SDLK_KP_7, KEYSYM_KP7 },
    { SDLK_KP_8, KEYSYM_KP8 },
    { SDLK_KP_9, KEYSYM_KP9 },
    { SDLK_KP_ENTER, KEYSYM_KPENTER },
    { SDLK_KP_PLUS, KEYSYM_KPPLUS },
    { SDLK_KP_MINUS, KEYSYM_KPMINUS },
    { SDLK_KP_MULTIPLY, KEYSYM_KPMUL },
    { SDLK_KP_DIVIDE, KEYSYM_KPDIV },
    { SDLK_KP_COMMA, KEYSYM_KPDOT },
};

static u32 sdl_keysym_to_vcml_keysym(u32 keysym) {
    const auto it = SDL_KEYSYMS.find(keysym);
    if (it != SDL_KEYSYMS.end())
        return it->second;
    return KEYSYM_NONE;
}

static u32 sdl_button_to_vcml_button(u32 button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE:
        return BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT:
        return BUTTON_RIGHT;
    default:
        return 0;
    }
}

static bool sdl_sym_is_text(const SDL_Keysym& keysym) {
    if (keysym.mod & KMOD_CTRL)
        return false;

    switch (keysym.sym) {
    case SDLK_UNKNOWN:
    case SDLK_RETURN:
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_TAB:
    case SDLK_DELETE:
        return false;

    default:
        return keysym.sym < 0xff;
    }
}

static int sdl_format_from_fbmode(const videomode& mode) {
    switch (mode.format) {
    case FORMAT_A8R8G8B8:
        return SDL_PIXELFORMAT_ARGB8888;
    case FORMAT_X8R8G8B8:
        return SDL_PIXELFORMAT_RGB888;
    case FORMAT_R8G8B8A8:
        return SDL_PIXELFORMAT_RGBA8888;
    case FORMAT_R8G8B8X8:
        return SDL_PIXELFORMAT_RGBX8888;
    case FORMAT_A8B8G8R8:
        return SDL_PIXELFORMAT_ABGR8888;
    case FORMAT_X8B8G8R8:
        return SDL_PIXELFORMAT_BGR888;
    case FORMAT_B8G8R8A8:
        return SDL_PIXELFORMAT_BGRA8888;
    case FORMAT_B8G8R8X8:
        return SDL_PIXELFORMAT_BGRX8888;

    case FORMAT_R8G8B8:
        return SDL_PIXELFORMAT_RGB24;
    case FORMAT_B8G8R8:
        return SDL_PIXELFORMAT_BGR24;

    case FORMAT_R5G6B5:
        return SDL_PIXELFORMAT_RGB565;
    case FORMAT_B5G6R5:
        return SDL_PIXELFORMAT_BGR565;

    case FORMAT_GRAY8:
        VCML_ERROR("%s unsupported", pixelformat_to_str(mode.format));

    default:
        VCML_ERROR("unknown pixelformat %u", mode.format);
    }
}

void sdl_client::notify_key(u32 keysym, bool down) {
    if (keysym == SDLK_g && (SDL_GetModState() & (KMOD_CTRL | KMOD_ALT))) {
        if (down) {
            grabbing = !grabbing;
            SDL_SetRelativeMouseMode(grabbing ? SDL_TRUE : SDL_FALSE);
        }

        return;
    }

    u32 symbol = sdl_keysym_to_vcml_keysym(keysym);
    if (symbol != KEYSYM_NONE && disp != nullptr)
        disp->notify_key(symbol, down);
}

void sdl_client::notify_btn(SDL_MouseButtonEvent& event) {
    u32 button = sdl_button_to_vcml_button(event.button);
    if (button != BUTTON_NONE && disp != nullptr)
        disp->notify_btn(button, event.state == SDL_PRESSED);
}

void sdl_client::notify_pos(SDL_MouseMotionEvent& event) {
    if (disp != nullptr)
        disp->notify_rel(event.xrel, event.yrel, 0);
}

void sdl_client::notify_wheel(SDL_MouseWheelEvent& event) {
    if (disp != nullptr)
        disp->notify_rel(0, 0, event.y);
}

void sdl_client::init_window() {
    const char* name = disp->name();

    const int w = (int)disp->xres();
    const int h = (int)disp->yres();
    const int x = SDL_WINDOWPOS_CENTERED;
    const int y = SDL_WINDOWPOS_CENTERED;

    window = SDL_CreateWindow(name, x, y, w, h, 0);
    if (window == nullptr)
        VCML_ERROR("cannot create SDL window: %s", SDL_GetError());

    window_id = SDL_GetWindowID(window);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
        VCML_ERROR("cannot create SDL renderer: %s", SDL_GetError());

    if (SDL_RenderSetLogicalSize(renderer, w, h) < 0)
        VCML_ERROR("cannot set renderer size: %s", SDL_GetError());

    const u8 r = 7, g = 25, b = 42;
    if (SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE) < 0)
        VCML_ERROR("cannot set clear color: %s", SDL_GetError());

    const int access = SDL_TEXTUREACCESS_STREAMING;
    const int format = sdl_format_from_fbmode(disp->mode());
    texture = SDL_CreateTexture(renderer, format, access, w, h);
    if (texture == nullptr)
        VCML_ERROR("cannot create SDL texture: %s", SDL_GetError());

    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void sdl_client::exit_window() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

void sdl_client::draw_window() {
    if (!disp || !window || !renderer || !texture)
        return;

    SDL_Rect rect = {};
    rect.x = 0;
    rect.y = 0;
    rect.w = disp->xres();
    rect.h = disp->yres();

    int pitch = disp->framebuffer_size() / disp->yres();
    const void* pixels = disp->framebuffer();

    SDL_RenderClear(renderer);

    if (pixels) {
        SDL_UpdateTexture(texture, &rect, pixels, pitch);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    }

    SDL_RenderPresent(renderer);
    frames++;

    // all times in microseconds
    const u64 update_interval = 1000000;
    u64 delta = mwr::timestamp_us() - time_frame;
    if (delta >= update_interval) {
        u64 now = time_to_us(sc_time_stamp());
        double rtf = (double)(now - time_sim) / delta;
        double fps = (double)frames / (delta / 1e6);

        size_t millis = (now % 1000000) / 1000;
        size_t times = now / 1000000;
        size_t hours = times / 3600;
        size_t minutes = (times % 3600) / 60;
        size_t seconds = times % 60;

        const char* name = disp->name();
        string cap = mkstr("%s fps:%.1f rtf:%.2f %02zu:%02zu:%02zu.%03zu",
                           name, fps, rtf, hours, minutes, seconds, millis);
        SDL_SetWindowTitle(window, cap.c_str());

        time_frame = mwr::timestamp_us();
        time_sim = now;
        frames = 0;
    }
}

sdl_client* sdl::find_by_window_id(u32 id) {
    for (size_t i = 0; i < m_clients.size(); i++)
        if (m_clients[i].window_id == id)
            return &m_clients[i];
    return nullptr;
}

void sdl::check_clients() {
    lock_guard<mutex> lock(m_client_mtx);
    for (auto& client : m_clients) {
        if (client.disp && !client.window)
            client.init_window();
        if (client.window && !client.disp)
            client.exit_window();
    }

    stl_remove_if(m_clients, [](const sdl_client& client) -> bool {
        return client.disp == nullptr && client.window == nullptr;
    });
}

void sdl::poll_events() {
    SDL_Event event = {};
    while (SDL_WaitEventTimeout(&event, 1) && sim_running()) {
        lock_guard<mutex> lock(m_client_mtx);
        switch (event.type) {
        case SDL_QUIT:
            break;

        case SDL_WINDOWEVENT: {
            auto client = find_by_window_id(event.window.windowID);
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                SDL_HideWindow(client->window);
                debugging::suspender::quit();
            }

            if (event.window.event == SDL_WINDOWEVENT_EXPOSED && client)
                client->draw_window();
            break;
        }

        case SDL_KEYUP:
        case SDL_KEYDOWN: {
            auto client = find_by_window_id(event.window.windowID);
            if (client && !sdl_sym_is_text(event.key.keysym))
                client->notify_key(event.key.keysym.sym, event.key.state);
            break;
        }

        case SDL_TEXTINPUT: {
            auto client = find_by_window_id(event.window.windowID);
            if (!client)
                break;

            for (const char* p = event.text.text; *p != '\0'; p++) {
                client->notify_key((u32)*p, true);
                client->notify_key((u32)*p, false);
            }

            break;
        }

        case SDL_MOUSEMOTION: {
            auto client = find_by_window_id(event.motion.windowID);
            if (client)
                client->notify_pos(event.motion);
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            auto client = find_by_window_id(event.button.windowID);
            if (client)
                client->notify_btn(event.button);
            break;
        }

        case SDL_MOUSEWHEEL: {
            auto client = find_by_window_id(event.wheel.windowID);
            if (client)
                client->notify_wheel(event.wheel);
            break;
        }

        default:
            break;
        }
    }
}

void sdl::draw_windows() {
    lock_guard<mutex> lock(m_client_mtx);
    for (auto& client : m_clients)
        client.draw_window();
}

void sdl::ui_run() {
    mwr::set_thread_name("sdl_ui_thread");

    if (SDL_WasInit(SDL_INIT_VIDEO) != SDL_INIT_VIDEO) {
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            log_error("cannot initialize SDL: %s", SDL_GetError());
            return;
        }
    }

    while (m_attached && sim_running()) {
        check_clients();
        poll_events();
        draw_windows();
    }

    while (m_attached)
        SDL_Delay(1);

    check_clients();
    SDL_Quit();
}

sdl::~sdl() {
    if (m_uithread.joinable()) {
        if (m_attached > 0)
            m_uithread.detach();
        else
            m_uithread.join();
    }
}

void sdl::register_display(display* disp) {
    lock_guard<mutex> attach_guard(m_attach_mtx);
    auto finder = [disp](const sdl_client& s) -> bool {
        return s.disp == disp;
    };

    lock_guard<mutex> client_guard(m_client_mtx);
    if (stl_contains_if(m_clients, finder))
        VCML_ERROR("display %s already registered", disp->name());

    sdl_client client = {};
    client.disp = disp;
    m_clients.push_back(client);
    m_attached++;

    if (!m_uithread.joinable())
        m_uithread = thread(&sdl::ui_run, this);
}

void sdl::unregister_display(display* disp) {
    lock_guard<mutex> attach_guard(m_attach_mtx);

    if (m_attached > 0) {
        lock_guard<mutex> client_guard(m_client_mtx);
        auto finder = [disp](const sdl_client& s) -> bool {
            return s.disp == disp;
        };

        auto it = std::find_if(m_clients.begin(), m_clients.end(), finder);
        if (it == m_clients.end())
            return;

        it->disp = nullptr;
        m_attached--;
    }

    if (m_uithread.joinable() && m_attached == 0)
        m_uithread.join();
}

display* sdl::create(u32 nr) {
    static sdl instance;
    return new sdl_display(nr, instance);
}

sdl_display::sdl_display(u32 nr, sdl& owner):
    display("sdl", nr), m_owner(owner) {
}

sdl_display::~sdl_display() {
    // nothing to do
}

void sdl_display::init(const videomode& mode, u8* fb) {
    display::init(mode, fb);
    m_owner.register_display(this);
}

void sdl_display::render(u32 x, u32 y, u32 w, u32 h) {
    // nothing to do
}

void sdl_display::render() {
    // nothing to do
}

void sdl_display::shutdown() {
    m_owner.unregister_display(this);
    display::shutdown();
}

} // namespace ui
} // namespace vcml
