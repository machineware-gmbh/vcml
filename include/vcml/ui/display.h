/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_DISPLAY_H
#define VCML_UI_DISPLAY_H

#include "vcml/core/types.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/video.h"
#include "vcml/ui/keymap.h"
#include "vcml/ui/input.h"

namespace vcml {
namespace ui {

class display
{
public:
    using create_fn = function<display*(u32)>;

private:
    string m_name;
    string m_type;
    u32 m_dispno;
    videomode m_mode;
    u8* m_fb;
    u8* m_nullfb;

    vector<input*> m_inputs;

protected:
    static unordered_map<string, create_fn> types;
    static unordered_map<u32, shared_ptr<display>> displays;

public:
    mwr::logger log;

    u32 xres() const { return m_mode.xres; }
    u32 yres() const { return m_mode.yres; }

    u32 dispno() const { return m_dispno; }

    const char* type() const { return m_type.c_str(); }
    const char* name() const { return m_name.c_str(); }

    const videomode& mode() const { return m_mode; }

    u8* framebuffer() const { return m_fb; }
    u64 framebuffer_size() const { return m_mode.size; }
    bool has_framebuffer() const { return m_mode.size > 0; }

    display() = delete;
    display(const display&) = delete;
    display(const string& type, u32 nr);
    virtual ~display();

    virtual void init(const videomode& mode, u8* fbptr);
    virtual void render(u32 x, u32 y, u32 w, u32 h);
    virtual void render();
    virtual void shutdown();

    virtual void notify_key(u32 keysym, bool down);
    virtual void notify_btn(u32 button, bool down);
    virtual void notify_pos(u32 x, u32 y);

    virtual void handle_option(const string& option);

    void attach(input* device);
    void detach(input* device);

    static void define(const string& type, create_fn fn);
    static shared_ptr<display> lookup(const string& name);
};

#define VCML_DEFINE_UI_DISPLAY(name, fn)        \
    MWR_CONSTRUCTOR(define_ui_display_##name) { \
        vcml::ui::display::define(#name, fn);   \
    }

} // namespace ui
} // namespace vcml

#endif
