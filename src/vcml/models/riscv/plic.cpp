/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/riscv/plic.h"

namespace vcml {
namespace riscv {

plic::context::context(size_t no):
    enabled(),
    threshold(mkstr("ctx%zu_threshold", no), BASE + no * SIZE + 0),
    claim(mkstr("ctx%zu_claim", no), BASE + no * SIZE + 4) {
    threshold.allow_read_write();
    threshold.on_write(&plic::write_threshold);
    threshold.tag = no;

    claim.allow_read_write();
    claim.on_read(&plic::read_claim);
    claim.on_write(&plic::write_complete);
    claim.tag = no;

    for (size_t regno = 0; regno < NIRQ / 32; regno++) {
        const string rnm = mkstr("ctx%zu_enabled%zu", no, regno);
        unsigned int gid = no * NIRQ / 32 + regno;

        enabled[regno] = new reg<u32>(rnm, 0x2000 + gid * 4);
        enabled[regno]->allow_read_write();
        enabled[regno]->on_write(&plic::write_enabled);
        enabled[regno]->tag = gid;
    }
}

plic::context::~context() {
    for (auto& reg : enabled)
        delete reg;
}

bool plic::is_pending(size_t irqno) const {
    VCML_ERROR_ON(irqno >= NIRQ, "invalid irq %zu", irqno);

    if (irqno == 0)
        return false;

    if (!irqs.exists(irqno))
        return false;

    return irqs[irqno].read();
}

bool plic::is_claimed(size_t irqno) const {
    VCML_ERROR_ON(irqno >= NIRQ, "invalid irq %zu", irqno);
    return m_claims[irqno] < NCTX;
}

bool plic::is_enabled(size_t irqno, size_t ctxno) const {
    VCML_ERROR_ON(irqno >= NIRQ, "invalid irq %zu", irqno);
    VCML_ERROR_ON(ctxno >= NCTX, "invalid context %zu", ctxno);

    if (irqno == 0)
        return false;

    if (m_contexts[ctxno] == nullptr)
        return false;

    unsigned int regno = irqno / 32;
    unsigned int shift = irqno % 32;

    return (m_contexts[ctxno]->enabled[regno]->get() >> shift) & 0b1;
}

u32 plic::irq_priority(size_t irqno) const {
    if (irqno == 0) {
        log_debug("attempt to read priority of invalid irq%zu\n", irqno);
        return 0;
    }

    return priority.get(irqno);
}

u32 plic::ctx_threshold(size_t ctxno) const {
    context* ctx = m_contexts[ctxno];
    if (ctx == nullptr)
        return 0u;
    return ctx->threshold;
}

u32 plic::read_pending(size_t regno) {
    unsigned int irqbase = regno * 32;

    u32 pending = 0u;
    for (unsigned int irqno = 0; irqno < 32; irqno++) {
        if (is_pending(irqbase + irqno) && !is_claimed(irqno))
            pending |= (1u << irqno);
    }

    if (regno == 0)
        pending |= ~1u;

    return pending;
}

u32 plic::read_claim(size_t ctxno) {
    unsigned int irq = 0;
    unsigned int threshold = ctx_threshold(ctxno);

    for (unsigned int irqno = 0; irqno < NIRQ; irqno++) {
        if (is_pending(irqno) && is_enabled(irqno, ctxno) &&
            !is_claimed(irqno) && irq_priority(irqno) > threshold) {
            irq = irqno;
            threshold = irq_priority(irqno);
        }
    }

    if (irq > 0)
        m_claims[irq] = ctxno;

    log_debug("context %zu claims irq %u", ctxno, irq);

    update();

    return irq;
}

void plic::write_priority(u32 value, size_t irqno) {
    priority[irqno] = value;
    update();
}

void plic::write_enabled(u32 value, size_t regno) {
    unsigned int ctxno = regno / (NIRQ / 32);
    unsigned int subno = regno % (NIRQ / 32);
    m_contexts[ctxno]->enabled[subno]->set(value);
    update();
}

void plic::write_threshold(u32 value, size_t ctxno) {
    m_contexts[ctxno]->threshold = value;
    update();
}

void plic::write_complete(u32 value, size_t ctxno) {
    unsigned int irq = value;
    if (value >= NIRQ) {
        log_warn("context %zu completes illegal irq %u", ctxno, irq);
        return;
    }

    if (m_claims[irq] != ctxno)
        log_debug("context %zu completes unclaimed irq %u", ctxno, value);

    m_claims[irq] = ~0u;
    update();
}

void plic::update() {
    for (auto ctx : irqt) {
        ctx.second->write(false);
        u32 th = ctx_threshold(ctx.first);

        for (auto irq : irqs) {
            if (is_pending(irq.first) && is_enabled(irq.first, ctx.first) &&
                !is_claimed(irq.first) && irq_priority(irq.first) > th) {
                ctx.second->write(true);
                log_debug("forwarding irq %zu to context %zu", irq.first,
                          ctx.first);
            }
        }
    }
}

plic::plic(const sc_module_name& nm):
    peripheral(nm),
    m_claims(),
    m_contexts(),
    priority("priority", 0x0, 0),
    pending("pending", 0x1000, 0),
    irqs("irqs", NIRQ),
    irqt("irqt", NCTX),
    in("in") {
    priority.allow_read_write();
    priority.on_write(&plic::write_priority);

    pending.allow_read_only();
    pending.on_read(&plic::read_pending);

    for (unsigned int ctx = 0; ctx < NCTX; ctx++)
        m_contexts[ctx] = nullptr;

    for (unsigned int irq = 0; irq < NIRQ; irq++)
        m_claims[irq] = ~0u;
}

plic::~plic() {
    for (auto ctx : m_contexts)
        delete ctx;
}

void plic::reset() {
    peripheral::reset();

    for (unsigned int irq = 0; irq < NIRQ; irq++)
        m_claims[irq] = ~0u;
}

void plic::end_of_elaboration() {
    for (auto ctx : irqt)
        m_contexts[ctx.first] = new context(ctx.first);

    VCML_ERROR_ON(irqs.exists(0), "irq0 must not be used");
}

void plic::gpio_notify(const gpio_target_socket& socket) {
    unsigned int irqno = irqs.index_of(socket);
    log_debug("irq %u %s", irqno, socket.read() ? "set" : "cleared");
    update();
}

VCML_EXPORT_MODEL(vcml::riscv::plic, name, args) {
    return new plic(name);
}

} // namespace riscv
} // namespace vcml
