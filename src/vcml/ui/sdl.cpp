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

    void sdl::update() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                sc_stop();
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    sc_stop();
                break;

            default:
                break;
            }
        }
    }

    sdl::sdl(u32 no):
        display("sdl", no),
        m_window() {

        static bool inited = false;
        if (!inited) {
            if (SDL_Init(SDL_INIT_VIDEO) < 0)
                VCML_ERROR("cannot initialize SDL: %s", SDL_GetError());
            inited = true;
        }

        init(fbmode_argb32(640, 480), NULL);

        on_each_time_step(std::bind(&sdl::update, this));
    }

    sdl::~sdl() {
        VCML_ERROR_ON(m_window, "display %s not shut down", name());
    }

    void sdl::init(const fbmode& mode, u8* fb) {
        shutdown();

        display::init(mode, fb);

        int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED;
        m_window = SDL_CreateWindow(name(), x, y, resx(), resy(), 0);
        if (m_window == nullptr)
            VCML_ERROR("cannot create SDL window: %s", SDL_GetError());
    }

    void sdl::render() {
        // ToDo
    }

    void sdl::shutdown() {
        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
    }

}}
