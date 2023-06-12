/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_VNC_H
#define VCML_UI_VNC_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/video.h"
#include "vcml/ui/display.h"

#include <rfb/rfb.h>
#include <rfb/keysym.h>

namespace vcml {
namespace ui {

class vnc : public display
{
private:
    u16 m_port;
    u32 m_buttons;
    u32 m_ptr_x;
    u32 m_ptr_y;
    atomic<bool> m_running;
    mutex m_mutex;
    rfbScreenInfo* m_screen;
    thread m_thread;

    void run();

public:
    u16 port() const { return m_port; }

    vnc(u32 nr);
    virtual ~vnc();

    virtual void init(const videomode& mode, u8* fb) override;
    virtual void render(u32 x, u32 y, u32 w, u32 h) override;
    virtual void render() override;
    virtual void shutdown() override;

    void key_event(u32 sym, bool down);
    void ptr_event(u32 mask, u32 x, u32 y);

    static display* create(u32 nr);
};

} // namespace ui
} // namespace vcml

#endif
