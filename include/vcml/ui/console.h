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
    u8* m_fbptr;
    videomode m_mode;

    unordered_set<input*> m_inputs;
    unordered_set<shared_ptr<display>> m_displays;

public:
    property<vector<string>> displays;

    bool has_display() const { return !m_displays.empty(); }

    const videomode& mode() const { return m_mode; }
    const u8* framebuffer() const { return m_fbptr; }
    u8* framebuffer() { return m_fbptr; }

    u32 xres() const { return m_mode.xres; }
    u32 yres() const { return m_mode.yres; }
    u32 read_pixel(u32 x, u32 y) const;

    console();
    virtual ~console();

    void notify(input& device);

    void setup(const videomode& mode, u8* fbptr);
    void render(u32 x, u32 y, u32 w, u32 h);
    void render();
    void shutdown();

    bool screenshot(const string& path) const;
};

inline u32 console::read_pixel(u32 x, u32 y) const {
    if (m_fbptr == nullptr || x >= xres() || y >= yres())
        return 0;

    const void* ptr = m_fbptr + y * m_mode.stride + x * m_mode.bpp;

    switch (m_mode.bpp) {
    case 1:
        return mwr::read_once<u8>(ptr);

    case 2: {
        u16 pixel = mwr::read_once<u16>(ptr);
        return m_mode.endian != host_endian() ? bswap(pixel) : pixel;
    }

    case 4: {
        u32 pixel = mwr::read_once<u32>(ptr);
        return m_mode.endian != host_endian() ? bswap(pixel) : pixel;
    }

    default:
        VCML_ERROR("invalid videomode: %zubpp", m_mode.bpp);
    }
}

} // namespace ui
} // namespace vcml

#endif
