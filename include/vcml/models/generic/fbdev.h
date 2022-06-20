/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_FBDEV_H
#define VCML_GENERIC_FBDEV_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"

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
