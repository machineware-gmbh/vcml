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

#ifndef VCML_UI_SDL_H
#define VCML_UI_SDL_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/fbmode.h"
#include "vcml/ui/display.h"

#include <SDL.h>

namespace vcml { namespace ui {

    class sdl: public display
    {
    private:
        u64 m_time_input;
        u64 m_time_frame;
        u64 m_time_sim;
        u64 m_frames;

        SDL_Window*   m_window;
        SDL_Renderer* m_renderer;
        SDL_Texture*  m_texture;

        void update();

        void notify_key(u32 keysym, bool down);
        void notify_btn(SDL_MouseButtonEvent& event);
        void notify_pos(SDL_MouseMotionEvent& event);

    public:
        sdl(u32 nr);
        virtual ~sdl();

        virtual void init(const fbmode& mode, u8* fb) override;
        virtual void render() override;
        virtual void shutdown() override;
    };

}}

#endif
