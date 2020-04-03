/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock, Lukas Juenger                         *
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

#ifndef AVP_ARM_GIC400_H
#define AVP_ARM_GIC400_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/bitops.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/component.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace arm {

    class gic400: public peripheral
    {
    public:
        static const unsigned int NCPU  = 8; // max supported CPUs
        static const unsigned int NVCPU = 8; // max supported virtual CPUs

        static const unsigned int NIRQ  = 1020;
        static const unsigned int NRES  = 4;
        static const unsigned int NSGI  = 16;
        static const unsigned int NPPI  = 16;
        static const unsigned int NREGS = NIRQ + NRES;
        static const unsigned int NPRIV = NSGI + NPPI;

        static const unsigned int NLR             = 64;
        static const unsigned int LR_PENDING_MASK = 0x10000000;
        static const unsigned int LR_ACTIVE_MASK  = 0x20000000;
        static const unsigned int VIRT_MIN_BPR    = 2;

        static const unsigned int IDLE_PRIO    = 0xFF;
        static const unsigned int SPURIOUS_IRQ = 1023;

        static const u32 PCID = 0xB105F00D;
        static const u32 IFID = 0x0202143B;

        enum handling_model {
            N_N = 0, // all processors handle the interrupt
            N_1 = 1  // only one processor handles interrupt
        };

        enum trigger_mode {
            LEVEL = 0, // interrupt asserted when signal is active
            EDGE  = 1  // interrupt triggered on rising edge
        };

        enum cpu_mask {
            ALL_CPU = ((1 << NCPU) - 1)
        };

        struct irq_state {
            u8 enabled;
            u8 pending;
            u8 active;
            u8 level;
            u8 signaled;

            handling_model model;
            trigger_mode trigger;

            irq_state();
        };

        struct lr {
            bool pending;
            bool active;
            bool hw;
            u8   prio;
            u16  virtual_id;
            u16  physical_id;
            u8   cpu_id;

            lr();
        };

        class distif: public peripheral
        {
        private:
            gic400* m_parent;

            u32 int_pending_mask(int cpu);
            u32 spi_pending_mask(int cpu);
            u16 ppi_enabled_mask(int cpu);

            u32 write_CTLR(u32 value);

            u32 read_TYPER();

            u32 read_ISENABLER_PPI();
            u32 write_ISENABLER_PPI(u32 value);

            u32 read_ISENABLER_SPI(unsigned int idx);
            u32 write_ISENABLER_SPI(u32 value, unsigned int idx);

            u32 read_ICENABLER_PPI();
            u32 write_ICENABLER_PPI(u32 value);

            u32 read_ICENABLER_SPI(unsigned int idx);
            u32 write_ICENABLER_SPI(u32 value, unsigned int idx);

            u32 read_ISPENDR_PPI();
            u32 write_ISPENDR_PPI(u32 value);

            u32 read_SSPR(unsigned int idx);
            u32 write_SSPR(u32 value, unsigned int idx);

            u32 read_ICPENDR_PPI();
            u32 write_ICPENDR_PPI(u32 value);

            u32 read_ICPENDR_SPI(unsigned int cpu_id);
            u32 write_ICPENDR_SPI(u32 value, unsigned int idx);

            u32 read_ISACTIVER_PPI();
            u32 read_ISACTIVER_SPI(unsigned int idx);

            u32 write_ICACTIVER_PPI(u32 value);
            u32 write_ICACTIVER_SPI(u32 value, unsigned int idx);

            u32 read_ITARGETS_PPI(unsigned int idx);

            u32 write_ICFGR(u32 value);
            u32 write_ICFGR_SPI(u32 value, unsigned int idx);

            u32 write_SGIR(u32 value);
            u8  write_SPENDSGIR(u8 value, unsigned int idx);
            u8  write_CPENDSGIR(u8 value, unsigned int idx);

        public:
            bitfield<0,1> CTLR_ENABLE;

            reg<distif, u32>     CTLR;  // Distributor Control register
            reg<distif, u32>     TYPER; // IRQ Controller Type register
            reg<distif, u32>     IIDR;  // Implementer Identification register

            reg<distif, u32>     ISENABLER_PPI; // IRQ Set Enable register
            reg<distif, u32, 31> ISENABLER_SPI; // SPI Set Enable register
            reg<distif, u32>     ICENABLER_PPI; // IRQ Clear Enable register
            reg<distif, u32, 31> ICENABLER_SPI; // SPI Clear Enable register

            reg<distif, u32>     ISPENDR_PPI; // IRQ Set Pending register
            reg<distif, u32, 31> ISPENDR_SPI; // SPI Set Pending register
            reg<distif, u32>     ICPENDR_PPI; // IRQ Clear Pending register
            reg<distif, u32, 31> ICPENDR_SPI; // SPI Clear Pending register

            reg<distif, u32>     ISACTIVER_PPI; // INT Active register
            reg<distif, u32, 31> ISACTIVER_SPI; // SPI Active register

            reg<distif, u32>     ICACTIVER_PPI; // INT Clear Active register
            reg<distif, u32, 31> ICACTIVER_SPI; // SPI Clear Active register

            reg<distif, u8, 16>  IPRIORITY_SGI; // SGI Priority register
            reg<distif, u8, 16>  IPRIORITY_PPI; // PPI Priority register
            reg<distif, u8, 988> IPRIORITY_SPI; // SPI Priority register

            reg<distif, u32, 8>  ITARGETS_PPI; // INT Target register
            reg<distif, u8, 988> ITARGETS_SPI; // SPI Target register

            reg<distif, u32>     ICFGR_SGI; // SGI Configuration register
            reg<distif, u32>     ICFGR_PPI; // PPI Configuration register
            reg<distif, u32, 62> ICFGR_SPI; // SPI Configuration register

            reg<distif, u32>     SGIR;      // SGI Control register
            reg<distif, u8, 16>  CPENDSGIR; // SGI Clear Pending register
            reg<distif, u8, 16>  SPENDSGIR; // SGI Set Pending register

            reg<distif, u32, 4>  CIDR; // Component ID register

            slave_socket IN;

            distif(const sc_module_name& nm);
            virtual ~distif();
            VCML_KIND(gic400::distif);

            virtual void reset() override;
            virtual void end_of_elaboration() override;

            void setup(unsigned int num_cpu, unsigned int num_irq);

            void set_sgi_pending(u8 value, unsigned int sgi, unsigned int cpu,
                                 bool set);
        };

        class cpuif: public peripheral
        {
        private:
            gic400* m_parent;

            u32 m_curr_irq[NCPU];
            u32 m_prev_irq[NREGS][NCPU];

            void set_current_irq(unsigned int cpu_id, unsigned int irq);

            u32 write_CTLR(u32 val);
            u32 write_PMR(u32 val);
            u32 write_BPR(u32 val);
            u32 write_EOIR(u32 val);
            u32 read_IAR();

            // disabled
            cpuif();
            cpuif(const cpuif&);

        public:
            bitfield<0,1> CTLR_ENABLE;

            reg<cpuif, u32>    CTLR;  // CPU Control register
            reg<cpuif, u32>    PMR;   // IRQ Priority Mask register
            reg<cpuif, u32>    BPR;   // Binary Point register
            reg<cpuif, u32>    IAR;   // Interrupt Acknowledge register
            reg<cpuif, u32>    EOIR;  // End Of Interrupt register
            reg<cpuif, u32>    RPR;   // Running Priority register
            reg<cpuif, u32>    HPPIR; // Highest Pending IRQ register
            reg<cpuif, u32>    ABPR;  // Alias Binary Point register
            reg<cpuif, u32, 4> APR;   // Active Priorities registers
            reg<cpuif, u32>    IIDR;  // Interface Identification register

            reg<cpuif, u32, 4> CIDR; // Component ID register
            reg<cpuif, u32> DIR;     // Deactivate interrupt register

            slave_socket IN;

            cpuif(const sc_module_name& nm);
            virtual ~cpuif();

            virtual void reset();
        };

        class vifctrl : public peripheral
        {
        private:
            gic400* m_parent;
            lr m_lr_state[NVCPU][NLR];

            u32 write_HCR(u32 val);
            u32 read_VTR();
            u32 write_LR(u32 val, unsigned int idx);
            u32 read_LR(unsigned int idx);
            u32 write_VMCR(u32 val);
            u32 read_VMCR();
            u32 write_APR(u32 val);

            // disabled
            vifctrl();
            vifctrl(const vifctrl&);

        public:
            reg<vifctrl, u32> HCR;     // Hypervisor Control register
            reg<vifctrl, u32> VTR;     // VGIC Type register
            reg<vifctrl, u32> VMCR;    // Virtual Machine Control register
            reg<vifctrl, u32> APR;     // Active Priorities Register
            reg<vifctrl, u32, 64> LR;  // List registers

            slave_socket IN;

            u8 get_irq_priority(unsigned int cpu, unsigned int irq);
            // list register state control
            bool is_lr_pending(u8 lr, u8 core_id);
            void set_lr_pending(u8 lr, u8 core_id, bool p);
            bool is_lr_active(u8 lr, u8 core_id);
            void set_lr_active(u8 lr, u8 core_id, bool p);
            void set_lr_cpuid(u8 lr, u8 core_id, u8 cpu_id);
            u8 get_lr_cpuid(u8 lr, u8 core_id);
            void set_lr_hw(u8 lr, u8 core_id, bool p);
            bool is_lr_hw(u8 lr, u8 core_id);
            u8 get_lr(unsigned int irq, u8 core_id);
            void set_lr_prio(u8 lr, u8 core_id, u32 prio);
            void set_lr_vid(u8 lr, u8 core_id, u16 virt_id);
            void set_lr_physid(u8 lr, u8 core_id, u16 phys_id);
            u16 get_lr_physid(u8 lr, u8 core_id);

            vifctrl(const sc_module_name& nm);
            virtual ~vifctrl();
        };

        class vcpuif : public peripheral
        {
        private:
            enum ctlr_bits {
                EnableGrp0 = 1 << 0,
            };

            gic400* m_parent;
            vifctrl* m_vifctrl;

            u32 write_BPR(u32 val);
            u32 write_CTLR(u32 val);
            u32 read_IAR();
            u32 write_EOIR(u32 val);

            // disabled
            vcpuif();
            vcpuif(const vcpuif&);
            vcpuif(const sc_module_name& nm);

        public:
            reg<vcpuif, u32>    CTLR;  // CPU Control register
            reg<vcpuif, u32>    PMR;   // IRQ Priority Mask register
            reg<vcpuif, u32>    BPR;   // Binary Point register
            reg<vcpuif, u32>    IAR;   // IRQ Acknowledge register
            reg<vcpuif, u32>    EOIR;  // End of Interrupt register
            reg<vcpuif, u32>    RPR;   // Running Priority register
            reg<vcpuif, u32>    HPPIR; // High. Priority Pending Interrupt reg.
            reg<vcpuif, u32, 4> APR;   // Active Priorities registers
            reg<vcpuif, u32>    IIDR;  // Interface Identification register

            slave_socket IN;

            vcpuif(const sc_module_name& nm, vifctrl* vifctrl);
            virtual ~vcpuif();

            virtual void reset();
        };

        distif DISTIF;
        cpuif CPUIF;
        vifctrl VIFCTRL;
        vcpuif VCPUIF;

        in_port_list<bool>  PPI_IN;
        in_port_list<bool>  SPI_IN;
        out_port_list<bool> FIQ_OUT;
        out_port_list<bool> IRQ_OUT;
        out_port_list<bool> VFIQ_OUT;
        out_port_list<bool> VIRQ_OUT;

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
        void set_irq_signaled(unsigned int irq, bool signaled, unsigned int m);
        bool irq_signaled(unsigned int irq, unsigned int mask);
        bool is_edge_triggered(unsigned int irq) const;
        bool is_level_triggered(unsigned int irq) const;

        bool test_pending(unsigned int irq, unsigned int mask);

        // constructor / destructor
        gic400(const sc_module_name& nm);
        virtual ~gic400();

        u8 get_irq_priority(unsigned int cpu, unsigned int irq);

        void update(bool virt = false);

        virtual void end_of_elaboration() override;

    private:
        unsigned int m_irq_num;
        unsigned int m_cpu_num;

        irq_state m_irq_state[NIRQ+NRES];

        void ppi_handler(unsigned int cpu, unsigned int irq);
        void spi_handler(unsigned int irq);
    };

    inline sc_in<bool>& gic400::ppi_in(unsigned int cpu, unsigned int irq) {
        return PPI_IN[cpu * NPPI + irq];
    }

    inline void gic400::enable_irq(unsigned int irq, unsigned int mask) {
        m_irq_state[irq].enabled |= mask;
    }

    inline void gic400::disable_irq(unsigned int irq, unsigned int mask) {
        m_irq_state[irq].enabled &= ~mask;
    }

    inline bool gic400::is_irq_enabled(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].enabled & mask) != 0;
    }

    inline void gic400::set_irq_pending(unsigned int irq, bool pending,
                                       unsigned int mask) {
        if (pending)
            m_irq_state[irq].pending |= mask;
        else
            m_irq_state[irq].pending &= ~mask;
    }

    inline bool gic400::is_irq_pending(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].pending & mask) != 0;
    }

    inline void gic400::set_irq_active(unsigned int irq, bool active,
                                        unsigned int mask) {
        if (active)
            m_irq_state[irq].active |= mask;
        else
            m_irq_state[irq].active &= ~mask;
    }

    inline bool gic400::is_irq_active(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].active & mask) != 0;
    }

    inline void gic400::set_irq_level(unsigned int irq, bool level,
                                       unsigned int mask) {
        if (level)
            m_irq_state[irq].level |= mask;
        else
            m_irq_state[irq].level &= ~mask;
    }

    inline bool gic400::get_irq_level(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].level & mask) != 0;
    }

    inline gic400::handling_model gic400::get_irq_model(unsigned int irq) {
        return m_irq_state[irq].model;
    }

    inline void gic400::set_irq_model(unsigned int irq, handling_model m) {
        m_irq_state[irq].model = m;
    }

    inline gic400::trigger_mode gic400::get_irq_trigger(unsigned int irq) {
        return m_irq_state[irq].trigger;
    }

    inline void gic400::set_irq_trigger(unsigned int irq, trigger_mode t) {
        m_irq_state[irq].trigger = t;
    }

    inline void gic400::set_irq_signaled(unsigned int irq, bool signaled,
                                         unsigned int mask) {
        if (signaled)
            m_irq_state[irq].signaled |= mask;
        else
            m_irq_state[irq].signaled &= ~mask;
    }

    inline bool gic400::irq_signaled(unsigned int irq, unsigned int mask) {
        return (m_irq_state[irq].signaled & mask) != 0;
    }

    inline bool gic400::test_pending(unsigned int irq, unsigned int mask) {
        return (is_irq_pending(irq, mask) ||
               (get_irq_trigger(irq) == LEVEL && get_irq_level(irq, mask) &&
                !irq_signaled(irq, mask)));
    }

    inline bool gic400::vifctrl::is_lr_pending(u8 lr, u8 core_id) {
        return m_lr_state[core_id][lr].pending;
    }

    inline void gic400::vifctrl::set_lr_pending(u8 lr, u8 core_id, bool p) {
        m_lr_state[core_id][lr].pending = p;
    }

    inline void gic400::vifctrl::set_lr_prio(u8 lr, u8 core_id, u32 prio) {
        m_lr_state[core_id][lr].prio = prio;
    }

    inline void gic400::vifctrl::set_lr_vid(u8 lr, u8 core_id, u16 virt_id) {
        m_lr_state[core_id][lr].virtual_id = virt_id;
    }

    inline void gic400::vifctrl::set_lr_physid(u8 lr, u8 core_id, u16 phys_id) {
        m_lr_state[core_id][lr].physical_id = phys_id;
    }

    inline u16 gic400::vifctrl::get_lr_physid(u8 lr, u8 core_id) {
        return m_lr_state[core_id][lr].physical_id;
    }

    inline bool gic400::vifctrl::is_lr_active(u8 lr, u8 core_id) {
        return m_lr_state[core_id][lr].active;
    }

    inline void gic400::vifctrl::set_lr_active(u8 lr, u8 core_id, bool p) {
        m_lr_state[core_id][lr].active = p;
    }

    inline void gic400::vifctrl::set_lr_cpuid(u8 lr, u8 core_id, u8 cpu_id) {
        m_lr_state[core_id][lr].cpu_id = cpu_id;
    }

    inline u8 gic400::vifctrl::get_lr_cpuid(u8 lr, u8 core_id) {
        return m_lr_state[core_id][lr].cpu_id;
    }

    inline void gic400::vifctrl::set_lr_hw(u8 lr, u8 core_id, bool p) {
        m_lr_state[core_id][lr].hw = p;
    }

    inline bool gic400::vifctrl::is_lr_hw(u8 lr, u8 core_id) {
        return m_lr_state[core_id][lr].hw;
    }

}}

#endif
