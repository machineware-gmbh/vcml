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
#include "vcml/core/thctl.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/video.h"
#include "vcml/ui/keymap.h"
#include "vcml/ui/input.h"

namespace vcml {
namespace ui {

class display
{
private:
    string m_name;
    string m_type;
    u32 m_dispno;
    videomode m_mode;
    u8* m_fb;
    u8* m_nullfb;

    vector<keyboard*> m_keyboards;
    vector<pointer*> m_pointers;

protected:
    static unordered_map<string, function<display*(u32)>> types;
    static unordered_map<string, shared_ptr<display>> displays;

    static display* create(u32 nr);

    display() = delete;
    display(const display&) = delete;
    display(const string& type, u32 nr);

public:
    u32 xres() const { return m_mode.xres; }
    u32 yres() const { return m_mode.yres; }

    u32 dispno() const { return m_dispno; }

    const char* type() const { return m_type.c_str(); }
    const char* name() const { return m_name.c_str(); }

    const videomode& mode() const { return m_mode; }

    u8* framebuffer() const { return m_fb; }
    u64 framebuffer_size() const { return m_mode.size; }
    bool has_framebuffer() const { return m_mode.size > 0; }

    virtual ~display();

    virtual void init(const videomode& mode, u8* fbptr);
    virtual void render(u32 x, u32 y, u32 w, u32 h);
    virtual void render();
    virtual void shutdown();

    virtual void notify_key(u32 keysym, bool down);
    virtual void notify_btn(u32 button, bool down);
    virtual void notify_rel(i32 x, i32 y, i32 w);

    virtual void add_keyboard(keyboard* kb);
    virtual void add_pointer(pointer* ptr);

    virtual void remove_keyboard(keyboard* kb);
    virtual void remove_pointer(pointer* ptr);

    static shared_ptr<display> lookup(const string& name);
    static void register_display_type(const string& type,
                                      function<display*(u32)> create);
};

} // namespace ui
} // namespace vcml

#endif
