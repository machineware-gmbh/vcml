/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#ifndef VCML_OPENCORES_OCKBD_H
#define VCML_OPENCORES_OCKBD_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/thctl.h"
#include "vcml/core/peripheral.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/console.h"

#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace opencores {

class ockbd : public peripheral
{
private:
    queue<u8> m_key_fifo;

    ui::keyboard m_keyboard;
    ui::console m_console;

    void update();
    void key_event(u32 key, u32 down);

    u8 read_khr();

    // disabled
    ockbd();
    ockbd(const ockbd&);

public:
    reg<u8> khr;

    gpio_initiator_socket irq;
    tlm_target_socket in;

    property<string> keymap;
    property<size_t> fifosize;

    ockbd(const sc_module_name& name);
    virtual ~ockbd();
    VCML_KIND(opencores::ockbd);

protected:
    virtual void end_of_simulation() override;
};

} // namespace opencores
} // namespace vcml

#endif
