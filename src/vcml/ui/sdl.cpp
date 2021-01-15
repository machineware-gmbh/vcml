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

#include <linux/input.h>

namespace vcml { namespace ui {

    static int fbmode_format(const fbmode& mode) {
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

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    sc_stop();
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
                break;

            default:
                break;
            }
        }

        m_time_input = realtime_us();
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

        static bool inited = false;
        if (!inited) {
            if (SDL_Init(SDL_INIT_VIDEO) < 0)
                VCML_ERROR("cannot initialize SDL: %s", SDL_GetError());
            inited = true;
        }

        on_each_time_step(std::bind(&sdl::update, this));
    }

    sdl::~sdl() {
        VCML_ERROR_ON(m_window, "display %s not shut down", name());
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

        m_texture = SDL_CreateTexture(m_renderer, fbmode_format(mode),
                                      SDL_TEXTUREACCESS_STREAMING, w, h);
        if (m_texture == nullptr)
            VCML_ERROR("cannot create SDL texture: %s", SDL_GetError());
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

}}
