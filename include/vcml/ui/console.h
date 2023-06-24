/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_CONSOLE_H
#define VCML_UI_CONSOLE_H

#include "vcml/core/types.h"
#include "vcml/core/thctl.h"

#include "vcml/ui/video.h"
#include "vcml/ui/keymap.h"
#include "vcml/ui/input.h"
#include "vcml/ui/display.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

namespace vcml {
namespace ui {

class console
{
private:
    unordered_set<keyboard*> m_keyboards;
    unordered_set<pointer*> m_pointers;
    unordered_set<shared_ptr<display>> m_displays;

public:
    property<vector<string>> displays;

    bool has_display() const { return !m_displays.empty(); }

    u32 xres() const;
    u32 yres() const;

    console();
    virtual ~console();

    void notify(keyboard& kbd);
    void notify(pointer& ptr);

    void setup(const videomode& mode, u8* fbptr);
    void render(u32 x, u32 y, u32 w, u32 h);
    void render();
    void shutdown();
};

inline u32 console::xres() const {
    return m_displays.empty() ? 0u : (*m_displays.begin())->xres();
}

inline u32 console::yres() const {
    return m_displays.empty() ? 0u : (*m_displays.begin())->yres();
}

} // namespace ui
} // namespace vcml

#endif
