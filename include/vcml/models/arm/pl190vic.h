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

#ifndef VCML_ARM_PL190VIC_H
#define VCML_ARM_PL190VIC_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace arm {

class pl190vic : public peripheral
{
private:
    u32 m_ext_irq;
    u32 m_current_irq;
    bool m_vect_int;

    void update();

    void write_inte(u32 val);
    void write_iecr(u32 val);
    void write_sint(u32 val);
    void write_sicr(u32 val);
    void write_addr(u32 val);
    void write_vctrl(u32 val, size_t idx);

public:
    enum vctrl_bits : u32 {
        VCTRL_ENABLED = 1 << 5,
        VCTRL_SOURCE_M = 0x1f,
        VCTRL_M = 0x3f,
    };

    enum vic_params : u32 {
        NVEC = 16,
        NIRQ = 32,
        AMBA_PID = 0x00041190, // Peripheral ID
        AMBA_CID = 0xb105f00d, // PrimeCell ID
    };

    reg<u32> irqs; // IRQ Status register
    reg<u32> fiqs; // FIQ Status register
    reg<u32> risr; // Raw Interrupt Status register
    reg<u32> ints; // Interrupt Select register
    reg<u32> inte; // Interrupt Enable register
    reg<u32> iecr; // Interrupt Enable Clear register
    reg<u32> sint; // Software Interrupt register
    reg<u32> sicr; // Software Interrupt Clear register
    reg<u32> prot; // Protection register
    reg<u32> addr; // Vector Address register
    reg<u32> defa; // Default Vector Address register

    reg<u32, NVEC> vaddr; // Vector Addresses
    reg<u32, NVEC> vctrl; // Vector Controls

    reg<u32, 4> pid; // Peripheral ID registers
    reg<u32, 4> cid; // Cell ID registers

    tlm_target_socket in;

    gpio_target_socket_array<NIRQ> irq_in;
    gpio_initiator_socket_array<> irq_out;
    gpio_initiator_socket_array<> fiq_out;

    pl190vic(const sc_module_name& nm);
    virtual ~pl190vic();
    VCML_KIND(arm::pl190vic);

    virtual void reset() override;
    virtual void gpio_notify(const gpio_target_socket& socket) override;
};

} // namespace arm
} // namespace vcml

#endif
