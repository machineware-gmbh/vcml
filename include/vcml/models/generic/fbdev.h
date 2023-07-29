/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_FBDEV_H
#define VCML_GENERIC_FBDEV_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/ui/console.h"

namespace vcml {
namespace generic {

class fbdev : public component
{
private:
    ui::console m_console;
    ui::videomode m_mode;
    u8* m_vptr;

    void update();

public:
    u8* vptr() const { return m_vptr; }
    size_t size() const { return m_mode.size; }
    size_t stride() const { return m_mode.stride; }

    property<u64> addr;
    property<u32> xres;
    property<u32> yres;
    property<string> format;

    tlm_initiator_socket out;

    fbdev() = delete;
    fbdev(const sc_module_name& nm, u32 width = 1280, u32 height = 720);
    virtual ~fbdev();
    VCML_KIND(fbdev);

    virtual void reset() override;

protected:
    void end_of_elaboration() override;
    void end_of_simulation() override;
};

} // namespace generic
} // namespace vcml

#endif
