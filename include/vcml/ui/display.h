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

#ifndef VCML_UI_DISPLAY_H
#define VCML_UI_DISPLAY_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
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
    virtual void notify_pos(u32 x, u32 y);

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
