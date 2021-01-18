/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/sdl.h"

namespace vcml { namespace ui {

    static const unordered_map<u32, u32> sdl_keysyms = {
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

        { SDLK_EXCLAIM,      KEYSYM_EXCLAIM },
        { SDLK_QUOTEDBL,     KEYSYM_DBLQUOTE },
        { SDLK_HASH,         KEYSYM_HASH },
        { SDLK_DOLLAR,       KEYSYM_DOLLAR },
        { SDLK_PERCENT,      KEYSYM_PERCENT },
        { SDLK_AMPERSAND,    KEYSYM_AMPERSAND },
        { SDLK_QUOTE,        KEYSYM_QUOTE },
        { SDLK_LEFTPAREN,    KEYSYM_LEFTPAR },
        { SDLK_RIGHTPAREN,   KEYSYM_RIGHTPAR },
        { SDLK_ASTERISK,     KEYSYM_ASTERISK },
        { SDLK_PLUS,         KEYSYM_PLUS },
        { SDLK_COMMA,        KEYSYM_COMMA },
        { SDLK_MINUS,        KEYSYM_MINUS },
        { SDLK_PERIOD,       KEYSYM_DOT },
        { SDLK_SLASH,        KEYSYM_SLASH },
        { SDLK_COLON,        KEYSYM_COLON },
        { SDLK_SEMICOLON,    KEYSYM_SEMICOLON },
        { SDLK_LESS,         KEYSYM_LESS },
        { SDLK_EQUALS,       KEYSYM_EQUAL },
        { SDLK_GREATER,      KEYSYM_GREATER },
        { SDLK_QUESTION,     KEYSYM_QUESTION },
        { SDLK_AT,           KEYSYM_AT },
        { SDLK_LEFTBRACKET,  KEYSYM_LEFTBRACKET },
        { SDLK_BACKSLASH,    KEYSYM_BACKSLASH },
        { SDLK_RIGHTBRACKET, KEYSYM_RIGHTBRACKET },
        { SDLK_CARET,        KEYSYM_CARET },
        { SDLK_UNDERSCORE,   KEYSYM_UNDERSCORE },
        { SDLK_BACKQUOTE,    KEYSYM_BACKQUOTE },
        { '{',               KEYSYM_LEFTBRACE },
        { '|',               KEYSYM_PIPE },
        { '}',               KEYSYM_RIGHTBRACE },
        { '~',               KEYSYM_TILDE },

        { SDLK_ESCAPE,       KEYSYM_ESC },
        { SDLK_RETURN,       KEYSYM_ENTER },
        { SDLK_BACKSPACE,    KEYSYM_BACKSPACE },
        { SDLK_SPACE,        KEYSYM_SPACE },
        { SDLK_TAB,          KEYSYM_TAB },
        { SDLK_LSHIFT,       KEYSYM_LEFTSHIFT },
        { SDLK_RSHIFT,       KEYSYM_RIGHTSHIFT },
        { SDLK_LCTRL,        KEYSYM_LEFTCTRL },
        { SDLK_RCTRL,        KEYSYM_RIGHTCTRL },
        { SDLK_LALT,         KEYSYM_LEFTALT },
        { SDLK_RALT,         KEYSYM_RIGHTALT },
        { SDLK_LGUI,         KEYSYM_LEFTMETA },
        { SDLK_RGUI,         KEYSYM_RIGHTMETA },
        { SDLK_MENU,         KEYSYM_MENU },
        { SDLK_CAPSLOCK,     KEYSYM_CAPSLOCK },

        { SDLK_F1,           KEYSYM_F1 },
        { SDLK_F2,           KEYSYM_F2 },
        { SDLK_F3,           KEYSYM_F3 },
        { SDLK_F4,           KEYSYM_F4 },
        { SDLK_F5,           KEYSYM_F5 },
        { SDLK_F6,           KEYSYM_F6 },
        { SDLK_F7,           KEYSYM_F7 },
        { SDLK_F8,           KEYSYM_F8 },
        { SDLK_F9,           KEYSYM_F9 },
        { SDLK_F10,          KEYSYM_F10 },
        { SDLK_F11,          KEYSYM_F11 },
        { SDLK_F12,          KEYSYM_F12 },

        { SDLK_PRINTSCREEN,  KEYSYM_PRINT },
        { SDLK_SCROLLLOCK,   KEYSYM_SCROLLOCK },
        { SDLK_PAUSE,        KEYSYM_PAUSE },

        { SDLK_INSERT,       KEYSYM_INSERT },
        { SDLK_DELETE,       KEYSYM_DELETE },
        { SDLK_HOME,         KEYSYM_HOME },
        { SDLK_END,          KEYSYM_END },
        { SDLK_PAGEUP,       KEYSYM_PAGEUP },
        { SDLK_PAGEDOWN,     KEYSYM_PAGEDOWN },

        { SDLK_LEFT,         KEYSYM_LEFT },
        { SDLK_RIGHT,        KEYSYM_RIGHT },
        { SDLK_UP,           KEYSYM_UP },
        { SDLK_DOWN,         KEYSYM_DOWN },

        { SDLK_NUMLOCKCLEAR, KEYSYM_NUMLOCK },
        { SDLK_KP_0,         KEYSYM_KP0 },
        { SDLK_KP_1,         KEYSYM_KP1 },
        { SDLK_KP_2,         KEYSYM_KP2 },
        { SDLK_KP_3,         KEYSYM_KP3 },
        { SDLK_KP_4,         KEYSYM_KP4 },
        { SDLK_KP_5,         KEYSYM_KP5 },
        { SDLK_KP_6,         KEYSYM_KP6 },
        { SDLK_KP_7,         KEYSYM_KP7 },
        { SDLK_KP_8,         KEYSYM_KP8 },
        { SDLK_KP_9,         KEYSYM_KP9 },
        { SDLK_KP_ENTER,     KEYSYM_KPENTER },
        { SDLK_KP_PLUS,      KEYSYM_KPPLUS },
        { SDLK_KP_MINUS,     KEYSYM_KPMINUS },
        { SDLK_KP_MULTIPLY,  KEYSYM_KPMUL },
        { SDLK_KP_DIVIDE,    KEYSYM_KPDIV },
        { SDLK_KP_COMMA,     KEYSYM_KPDOT },
    };

    static u32 sdl_keysym_to_vcml_keysym(u32 keysym) {
        const auto it = sdl_keysyms.find(keysym);
        if (it != sdl_keysyms.end())
            return it->second;
        return KEYSYM_NONE;
    }

    static u32 sdl_button_to_vcml_button(u32 button) {
        switch (button) {
        case SDL_BUTTON_LEFT: return BUTTON_LEFT;
        case SDL_BUTTON_MIDDLE: return BUTTON_MIDDLE;
        case SDL_BUTTON_RIGHT: return BUTTON_RIGHT;
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

    static int sdl_format_from_fbmode(const fbmode& mode) {
        switch (mode.format) {
        case FORMAT_ARGB32: return SDL_PIXELFORMAT_ARGB8888;
        case FORMAT_BGRA32: return SDL_PIXELFORMAT_BGRA8888;
        case FORMAT_RGB24: return SDL_PIXELFORMAT_RGB888;
        case FORMAT_BGR24: return SDL_PIXELFORMAT_BGR888;
        case FORMAT_RGB16: return SDL_PIXELFORMAT_RGB565;

        case FORMAT_GRAY8:
            VCML_ERROR("%s unsupported", pixelformat_to_str(mode.format));

        default:
            VCML_ERROR("unknown pixelformat %u", mode.format);
        }
    }

    void sdl::update() {
        if (!m_window)
            return;

        if (realtime_us() - m_time_input < 10000) // 10ms
            return;

        SDL_Event event = { 0 };
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    sc_stop();
                if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
                    SDL_RenderPresent(m_renderer);
                break;

            case SDL_KEYUP:
            case SDL_KEYDOWN:
                if (!sdl_sym_is_text(event.key.keysym))
                    notify_key(event.key.keysym.sym, event.key.state);
                break;

            case SDL_TEXTINPUT:
                for (const char* p = event.text.text; *p != '\0'; p++) {
                    notify_key((u32)*p, true);
                    notify_key((u32)*p, false);
                }
                break;

            case SDL_MOUSEMOTION:
                notify_pos(event.motion);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                notify_btn(event.button);
                break;

            default:
                break;
            }
        }

        m_time_input = realtime_us();
    }

    void sdl::notify_key(u32 keysym, bool down) {
        u32 symbol = sdl_keysym_to_vcml_keysym(keysym);
        if (symbol != KEYSYM_NONE)
            display::notify_key(symbol, down);
    }

    void sdl::notify_btn(SDL_MouseButtonEvent& event) {
        u32 button = sdl_button_to_vcml_button(event.button);
        if (button != BUTTON_NONE)
            display::notify_btn(button, event.state == SDL_PRESSED);
    }

    void sdl::notify_pos(SDL_MouseMotionEvent& event) {
        u32 x = event.x > 0 ? (u32)event.x : 0u;
        u32 y = event.y > 0 ? (u32)event.y : 0u;
        display::notify_pos(x, y);
    }

    sdl::sdl(u32 nr):
        display("sdl", nr),
        m_time_input(),
        m_time_frame(),
        m_time_sim(),
        m_frames(),
        m_window(nullptr),
        m_renderer(nullptr),
        m_texture(nullptr) {

        if (SDL_WasInit(SDL_INIT_VIDEO) != SDL_INIT_VIDEO) {
            if (SDL_Init(SDL_INIT_VIDEO) < 0)
                VCML_ERROR("cannot initialize SDL: %s", SDL_GetError());
        }

        on_each_time_step(std::bind(&sdl::update, this));
    }

    sdl::~sdl() {
        if (m_window)
            shutdown();
    }

    void sdl::init(const fbmode& mode, u8* fb) {
        shutdown();

        display::init(mode, fb);

        int w = (int)resx();
        int h = (int)resy();
        int x = SDL_WINDOWPOS_CENTERED;
        int y = SDL_WINDOWPOS_CENTERED;

        m_window = SDL_CreateWindow(name(), x, y, w, h, 0);
        if (m_window == nullptr)
            VCML_ERROR("cannot create SDL window: %s", SDL_GetError());

        m_renderer = SDL_CreateRenderer(m_window, -1, 0);
        if (m_renderer == nullptr)
            VCML_ERROR("cannot create SDL renderer: %s", SDL_GetError());

        if (SDL_RenderSetLogicalSize(m_renderer, w, h) < 0)
            VCML_ERROR("cannot set renderer size: %s", SDL_GetError());

        m_texture = SDL_CreateTexture(m_renderer, sdl_format_from_fbmode(mode),
                                      SDL_TEXTUREACCESS_STREAMING, w, h);
        if (m_texture == nullptr)
            VCML_ERROR("cannot create SDL texture: %s", SDL_GetError());

        SDL_RenderClear(m_renderer);
        SDL_RenderPresent(m_renderer);
    }

    void sdl::render() {
        SDL_Rect rect;

        rect.x = 0;
        rect.y = 0;
        rect.w = resx();
        rect.h = resy();

        int pitch = framebuffer_size() / resy();

        SDL_UpdateTexture(m_texture, &rect, framebuffer(), pitch);
        SDL_RenderClear(m_renderer);
        SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
        SDL_RenderPresent(m_renderer);

        m_frames++;

        // all times in microseconds
        const u64 update_interval = 1000000;
        u64 delta = realtime_us() - m_time_frame;
        if (delta >= update_interval) {
            u64 now = time_to_us(sc_time_stamp());
            double rtf = (double)(now - m_time_sim) / delta;
            double fps = (double)m_frames / (delta / 1e6);

            string caption = mkstr("%s %.1f fps %.2f rtf", name(), fps, rtf);
            SDL_SetWindowTitle(m_window, caption.c_str());

            m_time_frame = realtime_us();
            m_time_sim = now;
            m_frames = 0;
        }
    }

    void sdl::shutdown() {
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
            m_texture = nullptr;
        }

        if (m_renderer) {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    display* sdl::create(u32 nr) {
        return new sdl(nr);
    }

}}
