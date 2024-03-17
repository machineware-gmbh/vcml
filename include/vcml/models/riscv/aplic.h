/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RISCV_APLIC_H
#define VCML_RISCV_APLIC_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace riscv {

class aplic : public peripheral
{
public:
    static constexpr size_t NIRQ = 1023;
    static constexpr size_t NHART = 16384;

private:
    aplic* m_parent;
    vector<aplic*> m_children;

    bool is_mmode() const { return m_parent ? m_parent->is_mmode() : mmode; }
    bool is_smode() const { return !is_mmode(); }

    aplic* root() { return m_parent ? m_parent->root() : this; }

    bool is_msi() const;

    struct irqinfo {
        size_t idx;
        u32 sourcecfg;
        u32 targetcfg;
        bool connected;
        bool enabled;
        bool pending;
    };

    irqinfo m_irqs[NIRQ];

    void set_pending(irqinfo* irq, bool pending);
    void set_enabled(irqinfo* irq, bool enabled);

    u32 read_zero() { return 0; }
    u32 read_zero_idx(size_t idx) { return 0; }

    u32 read_sourcecfg(size_t idx);
    u32 read_targetcfg(size_t idx);
    u32 read_setip(size_t idx);
    u32 read_in(size_t idx);
    u32 read_setie(size_t idx);
    u32 read_genmsi();
    u32 read_topi(size_t idx);
    u32 read_claimi(size_t idx);

    void write_domaincfg(u32 val);
    void write_sourcecfg(u32 val, size_t idx);
    void write_targetcfg(u32 val, size_t idx);
    void write_mmsiaddrcfg(u32 val);
    void write_mmsiaddrcfgh(u32 val);
    void write_smsiaddrcfg(u32 val);
    void write_smsiaddrcfgh(u32 val);
    void write_setip(u32 val, size_t idx);
    void write_setipnum(u32 val);
    void write_clrip(u32 val, size_t idx);
    void write_clripnum(u32 val);
    void write_setie(u32 val, size_t idx);
    void write_setienum(u32 val);
    void write_clrie(u32 val, size_t idx);
    void write_clrienum(u32 val);
    void write_setipnum_le(u32 val);
    void write_setipnum_be(u32 val);
    void write_genmsi(u32 val);
    void write_idelivery(u32 val, size_t idx);
    void write_iforce(u32 val, size_t idx);
    void write_ithreshold(u32 val, size_t idx);

    void notify(size_t irq, bool level);

    void update();
    void update(irqinfo* irq);

    void send_msi(u32 hart, u32 guest, u32 eiid);
    void send_irq(u32 hart);

public:
    struct hartidc {
        static constexpr u64 offset(size_t i) { return 0x4000 + i * 32; }

        reg<u32> idelivery;
        reg<u32> iforce;
        reg<u32> ithreshold;
        reg<u32> topi;
        reg<u32> claimi;

        hartidc(size_t hart);
        ~hartidc();
    };

    property<bool> mmode;

    reg<u32> domaincfg;
    reg<u32, NIRQ> sourcecfg;

    reg<u32> mmsiaddrcfg;
    reg<u32> mmsiaddrcfgh;
    reg<u32> smsiaddrcfg;
    reg<u32> smsiaddrcfgh;

    reg<u32, NIRQ / 32 + 1> setip;
    reg<u32> setipnum;

    reg<u32, NIRQ / 32 + 1> in_clrip;
    reg<u32> clripnum;

    reg<u32, NIRQ / 32 + 1> setie;
    reg<u32> setienum;

    reg<u32, NIRQ / 32 + 1> clrie;
    reg<u32> clrienum;

    reg<u32> setipnum_le;
    reg<u32> setipnum_be;

    reg<u32> genmsi;

    reg<u32, NIRQ> targetcfg;

    hartidc* idcs[NHART];

    gpio_initiator_array irq_out;
    gpio_target_array irq_in;

    tlm_initiator_socket msi;
    tlm_target_socket in;

    aplic(const sc_module_name& name, aplic* parent = nullptr);
    aplic(const sc_module_name& nm, aplic& parent): aplic(nm, &parent) {}
    virtual ~aplic();
    VCML_KIND(riscv::aplic);

    virtual void reset() override;

protected:
    virtual void end_of_elaboration() override;
    virtual void gpio_notify(const gpio_target_socket& socket) override;
};

} // namespace riscv
} // namespace vcml

#endif
