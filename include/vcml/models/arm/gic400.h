/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ARM_GIC400_H
#define VCML_ARM_GIC400_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/model.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace arm {

class gic400 : public peripheral
{
public:
    using cpu_mask_t = u8;

    using BPR_P = field<0, 3, u32>;
    using ABPR_P = field<0, 3, u32>;
    using PMR_PR = field<0, 8, u32>;

    enum reg_configs : u32 {
        BPR_MIN = 0,
        BPR_MAX = 3,
        ABPR_MIN = BPR_MIN + 1,
        ABPR_MAX = 4,

        VIRT_BPR_MIN = 2,
        VIRT_BPR_MAX = 3,
        VIRT_ABPR_MIN = VIRT_BPR_MIN + 1,
        VIRT_ABPR_MAX = 4,
    };

    enum irq_as : address_space {
        IRQ_AS_SGI,
        IRQ_AS_PPI,
        IRQ_AS_SPI,
    };

    enum gic400_params : size_t {
        NCPU = 8,  // max supported CPUs
        NVCPU = 8, // max supported virtual CPUs

        NIRQ = 1020,
        NRES = 4,
        NSGI = 16,
        NPPI = 16,
        NSPI = 988,
        NREGS = NIRQ + NRES,
        NPRIV = NSGI + NPPI,

        NLR = 64,
        LR_PENDING_MASK = 0x10000000,
        LR_ACTIVE_MASK = 0x20000000,

        IDLE_PRIO = 0xff,

        ACKCTL_DISABLED_IRQ = 1022,
        SPURIOUS_IRQ = 1023,
    };

    enum amba_ids : u32 {
        AMBA_PCID = 0xb105f00d,
        AMBA_IFID = 0x0202143b,
    };

    enum handling_model {
        N_N = 0, // all processors handle the interrupt
        N_1 = 1  // only one processor handles interrupt
    };

    enum trigger_mode {
        LEVEL = 0, // interrupt asserted when signal is active
        EDGE = 1   // interrupt triggered on rising edge
    };

    enum cpu_mask : cpu_mask_t {
        ALL_CPU = bitmask(NCPU),
    };

    enum group_mode : cpu_mask_t { GRP0 = 0, GRP1 = 1 };

    struct irq_state {
        cpu_mask_t enabled;
        cpu_mask_t pending;
        cpu_mask_t active;
        cpu_mask_t level;
        cpu_mask_t signaled;
        cpu_mask_t group;

        handling_model model;
        trigger_mode trigger;

        irq_state();
    };

    struct list_entry {
        bool pending;
        bool active;
        bool hw;
        u8 prio;
        group_mode group;
        u16 virtual_id;
        u16 physical_id;
        cpu_mask_t cpu_id;

        list_entry();
    };

    class distif : public peripheral
    {
    private:
        gic400* m_parent;

        u32 int_pending_mask(size_t cpu);
        u32 spi_pending_mask(size_t cpu);
        u16 ppi_enabled_mask(size_t cpu);

        void write_ctlr(u32 value);

        u32 read_typer();

        u32 read_igroupr(size_t idx);
        void write_igroupr(u32 value, size_t idx);

        u32 read_isenabler_ppi();
        void write_isenabler_ppi(u32 value);

        u32 read_isenabler_spi(size_t idx);
        void write_isenabler_spi(u32 value, size_t idx);

        u32 read_icenabler_ppi();
        void write_icenabler_ppi(u32 value);

        u32 read_icenabler_spi(size_t idx);
        void write_icenabler_spi(u32 value, size_t idx);

        u32 read_ispendr_ppi();
        void write_ispendr_ppi(u32 value);

        u32 read_sspr(size_t idx);
        void write_sspr(u32 value, size_t idx);

        u32 read_icpendr_ppi();
        void write_icpendr_ppi(u32 value);

        u32 read_icpendr_spi(size_t cpu_id);
        void write_icpendr_spi(u32 value, size_t idx);

        u32 read_isactiver_ppi();
        u32 read_isactiver_spi(size_t idx);

        void write_icactiver_ppi(u32 value);
        void write_icactiver_spi(u32 value, size_t idx);

        u32 read_itargets_ppi(size_t idx);

        void write_icfgr(u32 value);
        void write_icfgr_spi(u32 value, size_t idx);

        void write_sgir(u32 value);
        void write_spendsgir(u8 value, size_t idx);
        void write_cpendsgir(u8 value, size_t idx);

    public:
        reg<u32> ctlr;  // Distributor Control register
        reg<u32> typer; // IRQ Controller Type register
        reg<u32> iidr;  // Implementer Identification register

        reg<u32, 31> igroupr; // Interrupt Group register

        reg<u32> isenabler_ppi;     // IRQ Set Enable register
        reg<u32, 31> isenabler_spi; // SPI Set Enable register
        reg<u32> icenabler_ppi;     // IRQ Clear Enable register
        reg<u32, 31> icenabler_spi; // SPI Clear Enable register

        reg<u32> ispendr_ppi;     // IRQ Set Pending register
        reg<u32, 31> ispendr_spi; // SPI Set Pending register
        reg<u32> icpendr_ppi;     // IRQ Clear Pending register
        reg<u32, 31> icpendr_spi; // SPI Clear Pending register

        reg<u32> isactiver_ppi;     // INT Active register
        reg<u32, 31> isactiver_spi; // SPI Active register

        reg<u32> icactiver_ppi;     // INT Clear Active register
        reg<u32, 31> icactiver_spi; // SPI Clear Active register

        reg<u8, 16> ipriority_sgi;  // SGI Priority register
        reg<u8, 16> ipriority_ppi;  // PPI Priority register
        reg<u8, 988> ipriority_spi; // SPI Priority register

        reg<u32, 8> itargets_ppi;  // INT Target register
        reg<u8, 988> itargets_spi; // SPI Target register

        reg<u32> icfgr_sgi;     // SGI Configuration register
        reg<u32> icfgr_ppi;     // PPI Configuration register
        reg<u32, 62> icfgr_spi; // SPI Configuration register

        reg<u32> sgir;         // SGI Control register
        reg<u8, 16> cpendsgir; // SGI Clear Pending register
        reg<u8, 16> spendsgir; // SGI Set Pending register

        reg<u32, 4> cidr; // Component ID register

        tlm_target_socket in;

        distif(const sc_module_name& nm);
        virtual ~distif();
        VCML_KIND(arm::gic400::distif);

        virtual void reset() override;
        virtual void end_of_elaboration() override;

        void set_sgi_pending(u8 value, size_t sgi, size_t cpu, bool set);
    };

    class cpuif : public peripheral
    {
    private:
        gic400* m_parent;

        u32 m_curr_irq[NCPU];
        u32 m_prev_irq[NREGS][NCPU];

        void set_current_irq(size_t cpu, size_t irq);

        void write_ctlr(u32 val);
        void write_bpr(u32 val);
        template <bool ALIAS>
        u32 read_iar();
        template <bool ALIAS>
        void write_eoir(u32 val);
        void write_abpr(u32 val);
        u32 read_aiar();

        // disabled
        cpuif();
        cpuif(const cpuif&);

    public:
        reg<u32> ctlr;   // CPU Control register
        reg<u32> pmr;    // IRQ Priority Mask register
        reg<u32> bpr;    // Binary Point register
        reg<u32> iar;    // Interrupt Acknowledge register
        reg<u32> eoir;   // End Of Interrupt register
        reg<u32> rpr;    // Running Priority register
        reg<u32> hppir;  // Highest Pending IRQ register
        reg<u32> abpr;   // Alias Binary Point register
        reg<u32> aiar;   // Alias Interrupt Acknowledge register
        reg<u32> aeoir;  // Alias End Of Interrupt register
        reg<u32> ahppir; // Alias Highest Pending IRQ register
        reg<u32, 4> apr; // Active Priorities registers
        reg<u32> iidr;   // Interface Identification register

        reg<u32, 4> cidr; // Component ID register
        reg<u32> dir;     // Deactivate interrupt register

        tlm_target_socket in;

        cpuif(const sc_module_name& nm);
        virtual ~cpuif();
        VCML_KIND(arm::gic400::cpuif);

        virtual void reset() override;
    };

    class vifctrl : public peripheral
    {
    private:
        gic400* m_parent;
        list_entry m_lr_state[NVCPU][NLR];

        void write_hcr(u32 val);
        u32 read_vtr();
        void write_lr(u32 val, size_t idx);
        u32 read_lr(size_t idx);
        void write_vmcr(u32 val);
        u32 read_vmcr();
        void write_apr(u32 val);

        // disabled
        vifctrl();
        vifctrl(const vifctrl&);

    public:
        reg<u32> hcr;    // Hypervisor Control register
        reg<u32> vtr;    // VGIC Type register
        reg<u32> vmcr;   // Virtual Machine Control register
        reg<u32> apr;    // Active Priorities Register
        reg<u32, 64> lr; // List registers

        tlm_target_socket in;

        u8 get_irq_priority(size_t cpu, size_t irq);
        // list register state control
        bool is_lr_pending(size_t lr, size_t cpu);
        void set_lr_pending(size_t lr, size_t cpu, bool p);
        bool is_lr_active(size_t lr, size_t cpu);
        void set_lr_active(size_t lr, size_t cpu, bool p);
        void set_lr_cpuid(size_t lr, size_t cpu, cpu_mask_t cpu_id);
        u8 get_lr_cpuid(size_t lr, size_t cpu);
        void set_lr_hw(size_t lr, size_t cpu, bool p);
        bool is_lr_hw(size_t lr, size_t cpu);
        u8 get_lr(size_t irq, size_t cpu);
        void set_lr_prio(size_t lr, size_t cpu, u32 prio);
        void set_lr_vid(size_t lr, size_t cpu, u16 virt_id);
        void set_lr_physid(size_t lr, size_t cpu, u16 phys_id);
        u16 get_lr_physid(size_t lr, size_t cpu);
        group_mode get_lr_group(size_t lr, size_t cpu);

        vifctrl(const sc_module_name& nm);
        virtual ~vifctrl();
        VCML_KIND(arm::gic400::vifctrl);
    };

    class vcpuif : public peripheral
    {
    private:
        gic400* m_parent;
        vifctrl* m_vifctrl;

        void write_bpr(u32 val);
        void write_ctlr(u32 val);
        template <bool ALIAS>
        u32 read_iar();
        template <bool ALIAS>
        void write_eoir(u32 val);
        void write_abpr(u32 val);

        // disabled
        vcpuif();
        vcpuif(const vcpuif&);

    public:
        reg<u32> ctlr;   // CPU Control register
        reg<u32> pmr;    // IRQ Priority Mask register
        reg<u32> bpr;    // Binary Point register
        reg<u32> iar;    // IRQ Acknowledge register
        reg<u32> eoir;   // End of Interrupt register
        reg<u32> rpr;    // Running Priority register
        reg<u32> hppir;  // High. Priority Pending Interrupt reg.
        reg<u32> abpr;   // Alias Binary Point register
        reg<u32> aiar;   // Alias Interrupt Acknowledge register
        reg<u32> aeoir;  // Alias End of Interrupt register
        reg<u32> ahppir; // Alias Highest Priority Pending Interrupt reg.
        reg<u32, 4> apr; // Active Priorities registers
        reg<u32> iidr;   // Interface Identification register

        tlm_target_socket in;

        vcpuif(const sc_module_name& nm, vifctrl* ctrl);
        virtual ~vcpuif();
        VCML_KIND(arm::gic400::vcpuif);

        virtual void reset() override;
    };

    distif distif;
    cpuif cpuif;
    vifctrl vifctrl;
    vcpuif vcpuif;

    gpio_target_array ppi_in;
    gpio_target_array spi_in;

    gpio_initiator_array fiq_out;
    gpio_initiator_array irq_out;

    gpio_initiator_array vfiq_out;
    gpio_initiator_array virq_out;

    gpio_target_socket& ppi(size_t cpu, size_t irq);

    size_t get_irq_num() const { return m_irq_num; }
    size_t get_cpu_num() const { return m_cpu_num; }

    // interrupt state control
    void enable_irq(size_t irq, cpu_mask_t mask);
    void disable_irq(size_t irq, cpu_mask_t mask);
    bool is_irq_enabled(size_t irq, cpu_mask_t mask);

    bool is_irq_pending(size_t irq, cpu_mask_t mask);
    void set_irq_pending(size_t irq, bool p, cpu_mask_t m);

    bool is_irq_active(size_t irq, cpu_mask_t mask);
    void set_irq_active(size_t irq, bool a, cpu_mask_t m);

    bool get_irq_level(size_t irq, cpu_mask_t mask);
    void set_irq_level(size_t irq, bool l, cpu_mask_t mask);

    handling_model get_irq_model(size_t irq);
    void set_irq_model(size_t irq, handling_model m);

    trigger_mode get_irq_trigger(size_t irq);
    void set_irq_trigger(size_t irq, trigger_mode t);

    group_mode get_irq_group(size_t irq, cpu_mask_t m);
    void set_irq_group(size_t irq, group_mode g, cpu_mask_t m);

    void set_irq_signaled(size_t irq, bool signaled, u8 m);
    bool irq_signaled(size_t irq, cpu_mask_t mask);

    bool test_pending(size_t irq, cpu_mask_t mask);

    gic400(const sc_module_name& nm);
    virtual ~gic400();
    VCML_KIND(arm::gic400);

    u8 get_irq_priority(size_t cpu, size_t irq);

    void update(bool virt = false);

    virtual void end_of_elaboration() override;
    virtual void gpio_notify(const gpio_target_socket& socket) override;

    void handle_ppi(size_t cpu, size_t idx, bool state);
    void handle_spi(size_t idx, bool state);

private:
    size_t m_irq_num;
    cpu_mask_t m_cpu_num;

    irq_state m_irq_state[NIRQ + NRES];

    pair<size_t, u32> get_highest_pend_irq(size_t cpu, bool virt);
    u8 get_prio_mask(u32 n, bool alias, bool virt);
    pair<bool, bool> update_excp_state(size_t cpu, size_t& irq, bool virt);
};

inline gpio_target_socket& gic400::ppi(size_t cpu, size_t irq) {
    return ppi_in[cpu * NPPI + irq];
}

inline void gic400::enable_irq(size_t irq, cpu_mask_t mask) {
    if (m_irq_state[irq].enabled == 0 && mask)
        log_debug("enabled irq %zu", irq);
    m_irq_state[irq].enabled |= mask;
}

inline void gic400::disable_irq(size_t irq, cpu_mask_t mask) {
    if (m_irq_state[irq].enabled && mask == 0)
        log_debug("disabled irq %zu", irq);
    m_irq_state[irq].enabled &= ~mask;
}

inline bool gic400::is_irq_enabled(size_t irq, cpu_mask_t mask) {
    return (m_irq_state[irq].enabled & mask) != 0;
}

inline void gic400::set_irq_pending(size_t irq, bool pending,
                                    cpu_mask_t mask) {
    if (pending)
        m_irq_state[irq].pending |= mask;
    else
        m_irq_state[irq].pending &= ~mask;
}

inline bool gic400::is_irq_pending(size_t irq, cpu_mask_t mask) {
    return (m_irq_state[irq].pending & mask) != 0;
}

inline void gic400::set_irq_active(size_t irq, bool active, cpu_mask_t mask) {
    if (active)
        m_irq_state[irq].active |= mask;
    else
        m_irq_state[irq].active &= ~mask;
}

inline bool gic400::is_irq_active(size_t irq, cpu_mask_t mask) {
    return (m_irq_state[irq].active & mask) != 0;
}

inline void gic400::set_irq_level(size_t irq, bool level, cpu_mask_t mask) {
    if (level)
        m_irq_state[irq].level |= mask;
    else
        m_irq_state[irq].level &= ~mask;
}

inline bool gic400::get_irq_level(size_t irq, cpu_mask_t mask) {
    return (m_irq_state[irq].level & mask) != 0;
}

inline gic400::handling_model gic400::get_irq_model(size_t irq) {
    return m_irq_state[irq].model;
}

inline void gic400::set_irq_model(size_t irq, handling_model m) {
    m_irq_state[irq].model = m;
}

inline gic400::trigger_mode gic400::get_irq_trigger(size_t irq) {
    return m_irq_state[irq].trigger;
}

inline gic400::group_mode gic400::get_irq_group(size_t irq, cpu_mask_t m) {
    return (m_irq_state[irq].group & m) ? GRP1 : GRP0;
}

inline void gic400::set_irq_group(size_t irq, group_mode g, cpu_mask_t m) {
    if (g == GRP0)
        m_irq_state[irq].group &= ~m;
    else
        m_irq_state[irq].group |= m;
}

inline void gic400::set_irq_trigger(size_t irq, trigger_mode t) {
    m_irq_state[irq].trigger = t;
}

inline void gic400::set_irq_signaled(size_t irq, bool signaled, u8 mask) {
    if (signaled)
        m_irq_state[irq].signaled |= mask;
    else
        m_irq_state[irq].signaled &= ~mask;
}

inline bool gic400::irq_signaled(size_t irq, u8 mask) {
    return (m_irq_state[irq].signaled & mask) != 0;
}

inline bool gic400::test_pending(size_t irq, u8 mask) {
    return (is_irq_pending(irq, mask) ||
            (get_irq_trigger(irq) == LEVEL && get_irq_level(irq, mask) &&
             !irq_signaled(irq, mask)));
}

inline bool gic400::vifctrl::is_lr_pending(size_t lr, size_t cpu) {
    return m_lr_state[cpu][lr].pending;
}

inline void gic400::vifctrl::set_lr_pending(size_t lr, size_t cpu, bool p) {
    m_lr_state[cpu][lr].pending = p;
}

inline void gic400::vifctrl::set_lr_prio(size_t lr, size_t cpu, u32 prio) {
    m_lr_state[cpu][lr].prio = prio;
}

inline void gic400::vifctrl::set_lr_vid(size_t lr, size_t cpu, u16 virt_id) {
    m_lr_state[cpu][lr].virtual_id = virt_id;
}

inline void gic400::vifctrl::set_lr_physid(size_t lr, size_t cpu,
                                           u16 phys_id) {
    m_lr_state[cpu][lr].physical_id = phys_id;
}

inline u16 gic400::vifctrl::get_lr_physid(size_t lr, size_t cpu) {
    return m_lr_state[cpu][lr].physical_id;
}

inline bool gic400::vifctrl::is_lr_active(size_t lr, size_t cpu) {
    return m_lr_state[cpu][lr].active;
}

inline void gic400::vifctrl::set_lr_active(size_t lr, size_t cpu, bool p) {
    m_lr_state[cpu][lr].active = p;
}

inline void gic400::vifctrl::set_lr_cpuid(size_t lr, size_t cpu,
                                          cpu_mask_t cpu_id) {
    m_lr_state[cpu][lr].cpu_id = cpu_id;
}

inline u8 gic400::vifctrl::get_lr_cpuid(size_t lr, size_t cpu) {
    return m_lr_state[cpu][lr].cpu_id;
}

inline void gic400::vifctrl::set_lr_hw(size_t lr, size_t cpu, bool p) {
    m_lr_state[cpu][lr].hw = p;
}

inline bool gic400::vifctrl::is_lr_hw(size_t lr, size_t cpu) {
    return m_lr_state[cpu][lr].hw;
}

inline gic400::group_mode gic400::vifctrl::get_lr_group(size_t lr,
                                                        size_t cpu) {
    return m_lr_state[cpu][lr].group;
}

} // namespace arm
} // namespace vcml

#endif
