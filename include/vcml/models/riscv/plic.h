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

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace riscv {

    class plic: public peripheral
    {
    public:
        static const int NIRQ = 1024;
        static const int NCTX = 15872;

    private:
        struct context {
            static const u64 BASE = 0x200000;
            static const u64 SIZE = 0x001000;

            reg<plic, u32>* ENABLED[NIRQ / 32];
            reg<plic, u32> THRESHOLD;
            reg<plic, u32> CLAIM;

            context(const string& nm, unsigned int id);
            ~context();
        };

        u32 m_claims[NIRQ];
        context* m_contexts[NCTX];

        bool is_pending(unsigned int irqno) const;
        bool is_claimed(unsigned int irqno) const;
        bool is_enabled(unsigned int irqno, unsigned int ctxno) const;

        u32 irq_priority(unsigned int irqno) const;
        u32 ctx_threshold(unsigned int ctxno) const;

        u32 read_PENDING(unsigned int regno);
        u32 read_CLAIM(unsigned int ctxno);

        u32 write_PRIORITY(u32 value, unsigned int irqno);
        u32 write_ENABLED(u32 value, unsigned int regno);
        u32 write_THRESHOLD(u32 value, unsigned int ctxno);
        u32 write_COMPLETE(u32 value, unsigned int ctxno);

        void irq_handler(unsigned int irqno);

        void update();

        // disabled
        plic();
        plic(const plic&);

    public:
        reg<plic, u32, NIRQ> PRIORITY;
        reg<plic, u32, NIRQ / 32> PENDING;

        in_port_list<bool>  IRQS;
        out_port_list<bool> IRQT;

        slave_socket IN;

        plic(const sc_module_name& nm);
        virtual ~plic();
        VCML_KIND(plic);

        virtual void reset() override;

    protected:
        virtual void end_of_elaboration() override;
    };

}}

#endif
