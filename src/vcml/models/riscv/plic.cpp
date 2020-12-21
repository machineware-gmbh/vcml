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

#include "vcml/models/riscv/plic.h"

namespace vcml { namespace riscv {

    plic::context::context(const string& nm, unsigned int no):
        ENABLED(),
        THRESHOLD(mkstr("CTX%u_THRESHOLD", no).c_str(), BASE + no * SIZE + 0),
        CLAIM(mkstr("CTX%u_CLAIM", no).c_str(), BASE + no * SIZE + 4) {

        THRESHOLD.allow_read_write();
        THRESHOLD.tagged_write = &plic::write_THRESHOLD;
        THRESHOLD.tag = no;

        CLAIM.allow_read_write();
        CLAIM.tagged_read = &plic::read_CLAIM;
        CLAIM.tagged_write = &plic::write_COMPLETE;
        CLAIM.tag = no;

        for (unsigned int regno = 0; regno < NIRQ / 32; regno++) {
            string rnm = mkstr("CTX%u_ENABLED%u", no, regno);
            unsigned int gid = no * NIRQ / 32 + regno;
            ENABLED[regno] = new reg<plic, u32>(rnm.c_str(), 0x2000 + gid * 4);
            ENABLED[regno]->allow_read_write();
            ENABLED[regno]->tagged_write = &plic::write_ENABLED;
            ENABLED[regno]->tag = gid;
        }
    }

    plic::context::~context() {
        for (auto& reg : ENABLED)
            delete reg;
    }

    bool plic::is_pending(unsigned int irqno) const {
        VCML_ERROR_ON(irqno >= NIRQ, "invalid irq %d", irqno);

        if (irqno == 0)
            return false;

        if (!IRQS.exists(irqno))
            return false;

        return IRQS[irqno].read();
    }

    bool plic::is_claimed(unsigned int irqno) const {
        VCML_ERROR_ON(irqno >= NIRQ, "invalid irq %u", irqno);
        return m_claims[irqno] < NCTX;
    }

    bool plic::is_enabled(unsigned int irqno, unsigned int ctxno) const {
        VCML_ERROR_ON(irqno >= NIRQ, "invalid irq %u", irqno);
        VCML_ERROR_ON(ctxno >= NCTX, "invalid context %u", ctxno);

        if (irqno == 0)
            return false;

        if (m_contexts[ctxno] == nullptr)
            return false;

        unsigned int regno = irqno / 32;
        unsigned int shift = irqno % 32;

        return (m_contexts[ctxno]->ENABLED[regno]->get() >> shift) & 0b1;
    }

    u32 plic::irq_priority(unsigned int irqno) const {
        if (irqno == 0) {
            log_debug("attempt to read priority of invalid irq%u\n", irqno);
            return 0;
        }

        return PRIORITY.get(irqno);
    }

    u32 plic::ctx_threshold(unsigned int ctxno) const {
        context* ctx = m_contexts[ctxno];
        if (ctx == nullptr)
            return 0u;
        return ctx->THRESHOLD;
    }

    u32 plic::read_PENDING(unsigned int regno) {
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

    u32 plic::read_CLAIM(unsigned int ctxno) {
        unsigned int irq = 0;
        unsigned int threshold = ctx_threshold(ctxno);

        for (unsigned int irqno = 0; irqno < NIRQ; irqno++) {
            if (is_pending(irqno)
                && is_enabled(irqno, ctxno)
                && !is_claimed(irqno)
                && irq_priority(irqno) > threshold) {
                irq = irqno;
                threshold = irq_priority(irqno);
            }
        }

        if (irq > 0)
            m_claims[irq] = ctxno;

        log_debug("context %u claims irq %u", ctxno, irq);

        update();

        return irq;
    }

    u32 plic::write_PRIORITY(u32 value, unsigned int irqno) {
        PRIORITY.get(irqno) = value;
        update();
        return value;
    }

    u32 plic::write_ENABLED(u32 value, unsigned int regno) {
        unsigned int ctxno = regno / (NIRQ / 32);
        unsigned int subno = regno % (NIRQ / 32);
        m_contexts[ctxno]->ENABLED[subno]->set(value);
        update();
        return value;
    }

    u32 plic::write_THRESHOLD(u32 value, unsigned int ctxno) {
        m_contexts[ctxno]->THRESHOLD = value;
        update();
        return value;
    }

    u32 plic::write_COMPLETE(u32 value, unsigned int ctxno) {
        unsigned int irq = value;
        if (value >= NIRQ) {
            log_warn("context %u completes illegal irq %u", ctxno, irq);
            return value;
        }

        if (m_claims[irq] != ctxno)
            log_debug("context %u completes unclaimed irq %u", ctxno, value);

        m_claims[irq] = ~0u;
        update();

        return value;
    }

    void plic::irq_handler(unsigned int irqno) {
        VCML_ERROR_ON(!IRQS.exists(irqno), "invalid irq %u", irqno);

        bool state = IRQS[irqno].read();
        log_debug("irq %u %s", irqno, state ? "set" : "cleared");

        update();
    }

    void plic::update() {
        for (auto ctx : IRQT) {
            ctx.second->write(false);
            u32 th = ctx_threshold(ctx.first);

            for (auto irq : IRQS) {
                if (is_pending(irq.first)
                    && is_enabled(irq.first, ctx.first)
                    && !is_claimed(irq.first)
                    && irq_priority(irq.first) > th) {

                    ctx.second->write(true);
                    log_debug("forwarding irq %u to context %u", irq.first,
                              ctx.first);
                }
            }
        }
    }

    plic::plic(const sc_module_name& nm):
        peripheral(nm),
        m_claims(),
        m_contexts(),
        PRIORITY("PRIORITY", 0x0, 0),
        PENDING("PENDING", 0x1000, 0),
        IRQS("IRQS"),
        IRQT("IRQT"),
        IN("IN") {
        PRIORITY.allow_read_write();
        PRIORITY.tagged_write = &plic::write_PRIORITY;

        PENDING.allow_read_only();
        PENDING.tagged_read = &plic::read_PENDING;

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
        for (auto ctx : IRQT) {
            string nm = mkstr("CONTEXT%u", ctx.first);
            m_contexts[ctx.first] = new context(nm.c_str(), ctx.first);
        }

        for (auto irq : IRQS) {
            VCML_ERROR_ON(irq.first == 0, "irq0 must not be used");

            string nm = "irq_handler_" + to_string(irq.first);

            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(irq.second);
            opts.dont_initialize();
            sc_spawn(sc_bind(&plic::irq_handler, this, irq.first),
                     sc_gen_unique_name(nm.c_str()), &opts);
        }
    }

}}
