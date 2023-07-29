/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RISCV_PLIC_H
#define VCML_RISCV_PLIC_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

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

    gpio_target_array irqs;
    gpio_initiator_array irqt;

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
