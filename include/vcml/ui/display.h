/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
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

class display_if
{
public:
    virtual ~display_if() = default;

    virtual void display_setup(const videomode& mode, u8* fbptr) = 0;
    virtual void display_render(u32 x, u32 y, u32 w, u32 h) = 0;
};

class display : public sc_object
{
private:
    mutable mutex m_mtx;
    vector<display_if*> m_ifs;

    u8* m_fbptr;
    videomode m_mode;

public:
    display(const string& nm = "display");
    virtual ~display();

    u32 xres() const { return m_mode.xres; }
    u32 yres() const { return m_mode.yres; }

    u32 read_pixel(u32 x, u32 y) const;

    void setup(const videomode& mode, u8* fbptr);
    void render(u32 x, u32 y, u32 w, u32 h);
    void render();

    void attach(display_if* dif);
    void detach(display_if* dif);

    bool screenshot(const string& path) const;
};

inline u32 display::read_pixel(u32 x, u32 y) const {
    if (m_fbptr == nullptr || x >= xres() || y >= yres())
        return 0;

    const void* ptr = m_fbptr + y * m_mode.stride + x * m_mode.bpp;

    switch (m_mode.bpp) {
    case 1:
        return mwr::read_once<u8>(ptr);
    case 2:
        return mwr::read_once<u16>(ptr);
    case 3:
        return mwr::extract(mwr::read_once<u32>(ptr), 0, 24);
    case 4:
        return mwr::read_once<u32>(ptr);
    default:
        VCML_ERROR("invalid videomode: %zubpp", m_mode.bpp);
    }
}

inline void display::render() {
    render(0, 0, xres(), yres());
}

} // namespace ui
} // namespace vcml

#endif
