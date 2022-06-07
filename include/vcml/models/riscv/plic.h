/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#ifndef VCML_RISCV_PLIC_H
#define VCML_RISCV_PLIC_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml {
namespace riscv {

class plic : public peripheral
{
public:
    static const size_t NIRQ = 1024;
    static const size_t NCTX = 15872;

private:
    struct context {
        static const u64 BASE = 0x200000;
        static const u64 SIZE = 0x001000;

        reg<u32>* enabled[NIRQ / 32];
        reg<u32> threshold;
        reg<u32> claim;

        context(size_t id);
        ~context();
    };

    u32 m_claims[NIRQ];
    context* m_contexts[NCTX];

    bool is_pending(size_t irqno) const;
    bool is_claimed(size_t irqno) const;
    bool is_enabled(size_t irqno, size_t ctxno) const;

    u32 irq_priority(size_t irqno) const;
    u32 ctx_threshold(size_t ctxno) const;

    u32 read_pending(size_t regno);
    u32 read_claim(size_t ctxno);

    void write_priority(u32 value, size_t irqno);
    void write_enabled(u32 value, size_t regno);
    void write_threshold(u32 value, size_t ctxno);
    void write_complete(u32 value, size_t ctxno);

    void update();

    // disabled
    plic();
    plic(const plic&);

public:
    reg<u32, NIRQ> priority;
    reg<u32, NIRQ / 32> pending;

    gpio_target_socket_array<NIRQ> irqs;
    gpio_initiator_socket_array<NCTX> irqt;

    tlm_target_socket in;

    plic(const sc_module_name& nm);
    virtual ~plic();
    VCML_KIND(riscv::plic);

    virtual void reset() override;

protected:
    virtual void end_of_elaboration() override;
    virtual void gpio_notify(const gpio_target_socket& socket) override;
};

} // namespace riscv
} // namespace vcml

#endif
