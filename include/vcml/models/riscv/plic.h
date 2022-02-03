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
#include "vcml/protocols/irq.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml { namespace riscv {

    class plic: public peripheral, public irq_target
    {
    public:
        static const int NIRQ = 1024;
        static const int NCTX = 15872;

    private:
        struct context {
            static const u64 BASE = 0x200000;
            static const u64 SIZE = 0x001000;

            reg<u32>* ENABLED[NIRQ / 32];
            reg<u32> THRESHOLD;
            reg<u32> CLAIM;

            context(const string& nm, size_t id);
            ~context();
        };

        u32 m_claims[NIRQ];
        context* m_contexts[NCTX];

        bool is_pending(size_t irqno) const;
        bool is_claimed(size_t irqno) const;
        bool is_enabled(size_t irqno, size_t ctxno) const;

        u32  irq_priority(size_t irqno) const;
        u32  ctx_threshold(size_t ctxno) const;

        u32  read_PENDING(size_t regno);
        u32  read_CLAIM(size_t ctxno);

        void write_PRIORITY(u32 value, size_t irqno);
        void write_ENABLED(u32 value, size_t regno);
        void write_THRESHOLD(u32 value, size_t ctxno);
        void write_COMPLETE(u32 value, size_t ctxno);

        void update();

        // disabled
        plic();
        plic(const plic&);

    public:
        reg<u32, NIRQ> PRIORITY;
        reg<u32, NIRQ / 32> PENDING;

        irq_target_socket_array<NIRQ> IRQS;
        irq_initiator_socket_array<NCTX> IRQT;

        tlm_target_socket IN;

        plic(const sc_module_name& nm);
        virtual ~plic();
        VCML_KIND(plic);

        virtual void reset() override;

    protected:
        virtual void end_of_elaboration() override;
        virtual void irq_transport(const irq_target_socket& socket,
            irq_payload& irq) override;
    };

}}

#endif
