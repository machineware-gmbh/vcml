/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_SDL_H
#define VCML_UI_SDL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/logger.h"
#include "vcml/debugging/suspender.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/video.h"
#include "vcml/ui/display.h"

#include <SDL.h>

namespace vcml {
namespace ui {

struct sdl_client {
    display* disp;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    u32 window_id;
    u64 time_frame;
    u64 time_sim;
    u64 frames;
    bool grabbing;

    void notify_key(u32 keysym, bool down);
    void notify_btn(SDL_MouseButtonEvent& event);
    void notify_pos(SDL_MouseMotionEvent& event);
    void notify_wheel(SDL_MouseWheelEvent& event);

    void init_window();
    void exit_window();
    void draw_window();
};

class sdl
{
private:
    mutex m_attach_mtx;
    mutex m_client_mtx;
    thread m_uithread;
    atomic<int> m_attached;
    vector<sdl_client> m_clients;

    sdl_client* find_by_window_id(u32 id);

    void check_clients();
    void poll_events();
    void draw_windows();

    void ui_run();

    sdl() = default;
    sdl(const sdl&) = delete;

public:
    ~sdl();

    void register_display(display* disp);
    void unregister_display(display* disp);

    static display* create(u32 nr);
};

class sdl_display : public display
{
private:
    sdl& m_owner;

public:
    sdl_display(u32 nr, sdl& owner);
    virtual ~sdl_display();

    virtual void init(const videomode& mode, u8* fb) override;
    virtual void render(u32 x, u32 y, u32 w, u32 h) override;
    virtual void render() override;
    virtual void shutdown() override;
};

} // namespace ui
} // namespace vcml

#endif
