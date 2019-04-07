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

#ifndef AVP_ARM_GICV2_H
#define AVP_ARM_GICV2_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/component.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

#define VCML_ARM_GICv2_NCPU (8)      // number of CPUs handled by the ARM GIC
#define VCML_ARM_GICv2_NIRQ (1020)   // number of possible interrupts
#define VCML_ARM_GICv2_NSPCL_IRQ (4) // number of special purpose IRQs
#define VCML_ARM_GICv2_NSGI (16)     // number of software generated interrupts
#define VCML_ARM_GICv2_NPPI (16)     // number of private peripheral interrupts
#define VCML_ARM_GICv2_REGS (VCML_ARM_GICv2_NIRQ + VCML_ARM_GICv2_NSPCL_IRQ)
#define VCML_ARM_GICv2_PRIV (VCML_ARM_GICv2_NSGI + VCML_ARM_GICv2_NPPI)

#define VCML_ARM_GICv2_IDLE_PRIO     (0xFF)
#define VCML_ARM_GICv2_SPURIOUS_IRQ  (1023)
#define VCML_ARM_GICv2_ALL_CPU_MASK  ((1 << VCML_ARM_GICv2_NCPU) - 1)

#define VCML_ARM_GICv2_CID 0xB105F00D // PrimeCell ID

namespace vcml { namespace arm {

    class gicv2: public peripheral
    {
    public:
        enum handling_model {
            N_N = 0, // all processors handle the interrupt
            N_1 = 1  // only one processor handles interrupt
        };

        enum trigger_mode {
            LEVEL = 0, // interrupt asserted when signal is active
            EDGE  = 1  // interrupt triggered on rising edge
        };

        enum cpu_mask {
            ALL_CPU = VCML_ARM_GICv2_ALL_CPU_MASK
        };

        struct irq_state {
            u8 enabled;
            u8 pending;
            u8 active;
            u8 level;

            handling_model model;
            trigger_mode trigger;

            irq_state();
        };

        class distif: public peripheral
        {
        private:
            gicv2* m_parent;

            u32 int_pending_mask(int cpu);
            u32 spi_pending_mask(int cpu);
            u16 ppi_enabled_mask(int cpu);

            u32 write_CTLR(u32 value);

            u32 read_ISER();
            u32 write_ISER(u32 value);

            u32 read_SSER(unsigned int idx);
            u32 write_SSER(u32 value, unsigned int idx);

            u32 read_ICER();
            u32 write_ICER(u32 value);

            u32 read_SCER(unsigned int idx);
            u32 write_SCER(u32 value, unsigned int idx);

            u32 read_ISPR();
            u32 write_ISPR(u32 value);

            u32 read_SSPR(unsigned int idx);
            u32 write_SSPR(u32 value, unsigned int idx);

            u32 read_ICPR();
            u32 write_ICPR(u32 value);

            u32 read_SCPR(unsigned int cpu_id);
            u32 write_SCPR(u32 value, unsigned int idx);

            u32 read_IACR();
            u32 read_SACR(unsigned int idx);

            u32 read_INTT(unsigned int idx);

            u32 write_CPPI(u32 value);
            u32 write_CSPI(u32 value, unsigned int idx);

            u32 write_SCTL(u32 value);
            u8  write_SGIS(u8 value, unsigned int idx);
            u8  write_SGIC(u8 value, unsigned int idx);

        public:
            enum ctlr_bits {
                CTLR_ENABLE = 1 << 0,
            };

            reg<distif, u32>     CTLR; // Distributor Control register
            reg<distif, u32>     ICTR; // IRQ Controller Type register

            reg<distif, u32>     ISER; // IRQ Set Enable register
            reg<distif, u32, 31> SSER; // SPI Set Enable register
            reg<distif, u32>     ICER; // IRQ Clear Enable register
            reg<distif, u32, 31> SCER; // SPI Clear Enable register

            reg<distif, u32>     ISPR; // IRQ Set Pending register
            reg<distif, u32, 31> SSPR; // SPI Set Pending register
            reg<distif, u32>     ICPR; // IRQ Clear Pending register
            reg<distif, u32, 31> SCPR; // SPI Clear Pending register

            reg<distif, u32>     IACR; // INT Active register
            reg<distif, u32, 31> SACR; // SPI Active register

            reg<distif, u8, 16>  SGIP; // SGI Priority register
            reg<distif, u8, 16>  PPIP; // PPI Priority register
            reg<distif, u8, 988> SPIP; // SPI Priority register

            reg<distif, u32, 8>  INTT; // INT Target register
            reg<distif, u8, 988> SPIT; // SPI Target register

            reg<distif, u32>     CSGI; // SGI Configuration register
            reg<distif, u32>     CPPI; // PPI Configuration register
            reg<distif, u32, 62> CSPI; // SPI Configuration register

            reg<distif, u32>     SCTL; // SGI Control register
            reg<distif, u8, 16>  SGIS; // SGI Set Pending register
            reg<distif, u8, 16>  SGIC; // SGI Clear Pending register

            reg<distif, u32, 4>  CIDR; // Component ID reg.

            slave_socket IN;

            distif(const sc_module_name& nm);
            virtual ~distif();
            VCML_KIND(gicv2::distif);

            virtual void reset() override;
            virtual void end_of_elaboration() override;

            void setup(unsigned int num_cpu, unsigned int num_irq);

            void set_sgi_pending(u8 value, unsigned int sgi, unsigned int cpu,
                                 bool set);
        };

        class cpuif: public peripheral
        {
        private:
            gicv2* m_parent;

            u32 m_curr_irq[VCML_ARM_GICv2_NCPU];
            u32 m_prev_irq[VCML_ARM_GICv2_REGS][VCML_ARM_GICv2_NCPU];

            void set_current_irq(unsigned int cpu_id, unsigned int irq);

            u32 write_CTLR(u32 val);
            u32 write_IPMR(u32 val);
            u32 write_BIPR(u32 val);
            u32 write_EOIR(u32 val);
            u32 read_IACK();

            // disabled
            cpuif();
            cpuif(const cpuif&);

        public:
            enum ctlr_bits {
                CTLR_ENABLE = 1 << 0,
            };

            reg<cpuif, u32> CTLR; // CPU Control register
            reg<cpuif, u32> IPMR; // IRQ Priority Mask register
            reg<cpuif, u32> BIPR; // Binary Point register
            reg<cpuif, u32> IACK; // Interrupt Acknowledge register
            reg<cpuif, u32> EOIR; // End Of Interrupt register
            reg<cpuif, u32> PRIO; // Running Priority register
            reg<cpuif, u32> PEND; // Highest Pending IRQ register
            reg<cpuif, u32> ABPR; // Alias Binary Point register

            reg<cpuif, u32, 4> CIDR; // Component ID register

            slave_socket IN;

            cpuif(const sc_module_name& nm);
            virtual ~cpuif();

            virtual void reset();
        };

        distif DISTIF;
        cpuif CPUIF;

        in_port_list  PPI_IN;
        in_port_list  SPI_IN;
        out_port_list FIQ_OUT;
        out_port_list IRQ_OUT;

        sc_in<bool>& ppi_in(unsigned int cpu, unsigned int irq);

        unsigned int get_irq_num() const { return m_irq_num; }
        unsigned int get_cpu_num() const { return m_cpu_num; }

        // interrupt state control
        void enable_irq(unsigned int irq, unsigned int mask);
        void disable_irq(unsigned int irq, unsigned int mask);
        bool is_irq_enabled(unsigned int irq, unsigned int mask);

        bool is_irq_pending(unsigned int irq, unsigned int mask);
        void set_irq_pending(unsigned int irq, bool p, unsigned int m);

        bool is_irq_active(unsigned int irq, unsigned int mask);
        void set_irq_active(unsigned int irq, bool a, unsigned int m);

        bool get_irq_level(unsigned int irq, unsigned int mask);
        void set_irq_level(unsigned int irq, bool l, unsigned int mask);

        handling_model get_irq_model(unsigned int irq);
        void set_irq_model(unsigned int irq, handling_model m);

        trigger_mode get_irq_trigger(unsigned int irq);
        void set_irq_trigger(unsigned int irq, trigger_mode t);
        bool is_edge_triggered(unsigned int irq) const;
        bool is_level_triggered(unsigned int irq) const;

        bool test_pending(unsigned int irq, unsigned int mask);

        // constructor / destructor
        gicv2(const sc_module_name& nm);
        virtual ~gicv2();

        u8 get_irq_priority(unsigned int cpu, unsigned int irq);

        void update();

        virtual void end_of_elaboration() override;

    private:
        unsigned int m_irq_num;
        unsigned int m_cpu_num;

        irq_state m_irq_state[VCML_ARM_GICv2_NIRQ];

        void ppi_handler(unsigned int cpu, unsigned int irq);
        void spi_handler(unsigned int irq);
    };

    inline sc_in<bool>& gicv2::ppi_in(unsigned int cpu, unsigned int irq) {
        return PPI_IN[cpu * VCML_ARM_GICv2_NPPI + irq];
    }

    inline void gicv2::enable_irq(unsigned int irq, unsigned int mask) {
        m_irq_state[irq].enabled |= mask;
    }

    inline void gicv2::disable_irq(unsigned int irq, unsigned int mask) {
        m_irq_state[irq].enabled &= ~mask;
    }

    inline bool gicv2::is_irq_enabled(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].enabled & mask) != 0;
    }

    inline void gicv2::set_irq_pending(unsigned int irq, bool pending,
                                       unsigned int mask) {
        if (pending)
            m_irq_state[irq].pending |= mask;
        else
            m_irq_state[irq].pending &= ~mask;
    }

    inline bool gicv2::is_irq_pending(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].pending & mask) != 0;
    }

    inline void gicv2::set_irq_active(unsigned int irq, bool active,
                                        unsigned int mask) {
        if (active)
            m_irq_state[irq].active |= mask;
        else
            m_irq_state[irq].active &= ~mask;
    }

    inline bool gicv2::is_irq_active(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].active & mask) != 0;
    }

    inline void gicv2::set_irq_level(unsigned int irq, bool level,
                                       unsigned int mask) {
        if (level)
            m_irq_state[irq].level |= mask;
        else
            m_irq_state[irq].level &= ~mask;
    }

    inline bool gicv2::get_irq_level(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].level & mask) != 0;
    }

    inline gicv2::handling_model gicv2::get_irq_model(unsigned int irq) {
        return m_irq_state[irq].model;
    }

    inline void gicv2::set_irq_model(unsigned int irq, handling_model m) {
        m_irq_state[irq].model = m;
    }

    inline gicv2::trigger_mode gicv2::get_irq_trigger(unsigned int irq) {
        return m_irq_state[irq].trigger;
    }

    inline void gicv2::set_irq_trigger(unsigned int irq, trigger_mode t) {
        m_irq_state[irq].trigger = t;
    }

    inline bool gicv2::test_pending(unsigned int irq, unsigned int mask) {
        return (is_irq_pending(irq, mask) ||
               (get_irq_trigger(irq) == LEVEL && get_irq_level(irq, mask)));
    }

}}

#endif
