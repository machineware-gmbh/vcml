/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/riscv/aplic.h"

namespace vcml {
namespace riscv {

enum domaincfg_bits : u32 {
    DOMAINCFG_BE = bit(0),
    DOMAINCFG_DM = bit(2),
    DOMAINCFG_IE = bit(8),
    DOMAINCFG_MASK = DOMAINCFG_BE | DOMAINCFG_DM | DOMAINCFG_IE,
};

enum source_mode : u32 {
    SM_INACTIVE = 0,
    SM_DETACHED = 1,
    SM_EDGE_RISE = 4,
    SM_EDGE_FALL = 5,
    SM_LEVEL_HI = 6,
    SM_LEVEL_LO = 7,
};

using SOURCECFG_CI = field<0, 10, u32>;
using SOURCECFG_SM = field<0, 3, u32>;

enum sourcecfg_bits : u32 {
    SOURCECFG_D = bit(10),
    SOURCECFG_D_MASK = SOURCECFG_D | SOURCECFG_CI::MASK,
    SOURCECFG_MASK = SOURCECFG_SM::MASK,
};

using TARGETCFG_HART = field<18, 14, u32>;
using TARGETCFG_PRIO = field<0, 8, u32>;
using TARGETCFG_GIDX = field<12, 6, u32>;
using TARGETCFG_EIID = field<0, 11, u32>;

enum targetcfg_bits : u32 {
    TARGETCFG_DM_MASK = TARGETCFG_HART::MASK | TARGETCFG_PRIO::MASK,
    TARGETCFG_MSI_MASK = TARGETCFG_HART::MASK | TARGETCFG_GIDX::MASK |
                         TARGETCFG_EIID::MASK,
};

using MSICFG_PPN = field<0, 12, u32>;
using MSICFG_LHXW = field<12, 4, u32>;
using MSICFG_HHXW = field<16, 3, u32>;
using MSICFG_LHXS = field<20, 3, u32>;
using MSICFG_HHXS = field<24, 5, u32>;

enum msicfg_bits : u32 {
    XMSIADDR_L = bit(31),
    XMSIADDR_MASK = MSICFG_PPN::MASK | MSICFG_LHXW::MASK | MSICFG_HHXW::MASK |
                    MSICFG_LHXS::MASK | MSICFG_HHXS::MASK | XMSIADDR_L,
};

using GENMSI_HART = field<18, 14, u32>;
using GENMSI_EIID = field<0, 11, u32>;

enum genmsi_bits {
    GENMSI_BUSY = bit(12),
    GENMSI_MASK = GENMSI_EIID::MASK | GENMSI_HART::MASK,
};

using TOPI_EIID = field<16, 10, u32>;
using TOPI_PRIO = field<0, 8, u32>;

bool aplic::is_msi() const {
    return m_parent ? m_parent->is_msi() : !(domaincfg & DOMAINCFG_DM);
}

void aplic::set_pending(irqinfo* irq, bool pending) {
    if (irq->sourcecfg & SOURCECFG_D)
        return;

    u32 sm = get_field<SOURCECFG_SM>(irq->sourcecfg);
    if (sm == SM_INACTIVE)
        return;

    if (sm == SM_LEVEL_LO || sm == SM_LEVEL_HI) {
        if (!is_msi() || !pending)
            return;
    }

    if (irq->pending == pending)
        return;

    irq->pending = pending;
    update(irq);
}

void aplic::set_enabled(irqinfo* irq, bool enabled) {
    if (irq->sourcecfg & SOURCECFG_D)
        return;

    if (irq->enabled == enabled)
        return;

    irq->enabled = enabled;
    update(irq);
}

u32 aplic::read_sourcecfg(size_t idx) {
    irqinfo* irq = m_irqs + idx;
    return irq->sourcecfg;
}

u32 aplic::read_targetcfg(size_t idx) {
    irqinfo* irq = m_irqs + idx;
    return irq->targetcfg;
}

u32 aplic::read_setip(size_t idx) {
    u32 val = 0;
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if (base > 0 && m_irqs[base - 1].pending)
            val |= bit(i);
    }

    return val;
}

u32 aplic::read_in(size_t idx) {
    u32 val = 0;
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if (irq_in.exists(i) && irq_in[i].read())
            val |= bit(i);
    }

    return val;
}

u32 aplic::read_setie(size_t idx) {
    u32 val = 0;
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if (base > 0 && m_irqs[base - 1].enabled)
            val |= bit(i);
    }

    return val;
}

u32 aplic::read_genmsi() {
    if (domaincfg & DOMAINCFG_DM)
        return 0;
    return genmsi;
}

u32 aplic::read_topi(size_t idx) {
    u32 best_irq = -1;
    u32 best_prio = -1;
    u32 threshold = idcs[idx]->ithreshold;

    for (irqinfo& irq : m_irqs) {
        u32 hart = get_field<TARGETCFG_HART>(irq.targetcfg);
        u32 prio = get_field<TARGETCFG_PRIO>(irq.targetcfg);
        if (irq.enabled && irq.pending && hart == idx && prio <= best_prio &&
            (threshold == 0 || prio < threshold)) {
            best_irq = irq.idx;
            best_prio = prio;
        }
    }

    if (best_irq > NIRQ)
        return 0;

    u32 topi = 0;
    set_field<TOPI_EIID>(topi, best_irq);
    set_field<TOPI_PRIO>(topi, best_prio);
    return topi;
}

u32 aplic::read_claimi(size_t idx) {
    u32 topi = read_topi(idx);
    if (!topi) {
        idcs[idx]->iforce = 0;
        return 0;
    }

    u32 eiid = get_field<TOPI_EIID>(topi);
    m_irqs[eiid - 1].pending = false;
    send_irq(idx);
    return topi;
}

void aplic::write_domaincfg(u32 val) {
    u32 mask = DOMAINCFG_MASK;
    if (irq_out.count() == 0)
        mask &= ~DOMAINCFG_DM;

    val = (domaincfg & ~mask) | (val & mask);
    if (val == domaincfg)
        return;

    domaincfg = val;

    if (domaincfg & DOMAINCFG_BE)
        set_big_endian();
    else
        set_little_endian();

    update();
}

void aplic::write_sourcecfg(u32 val, size_t idx) {
    irqinfo* irq = m_irqs + idx;
    if (!irq->connected)
        return;

    if (val & SOURCECFG_D) {
        irq->sourcecfg = val & SOURCECFG_D_MASK;
        irq->targetcfg = 0;
    } else {
        irq->sourcecfg = val & SOURCECFG_MASK;
    }

    update(irq);
}

void aplic::write_targetcfg(u32 val, size_t idx) {
    irqinfo* irq = m_irqs + idx;
    if (!irq->connected)
        return;

    if (irq->sourcecfg & SOURCECFG_D)
        irq->targetcfg = 0;
    else if (domaincfg & DOMAINCFG_DM)
        irq->targetcfg = val & TARGETCFG_DM_MASK;
    else
        irq->targetcfg = val & TARGETCFG_MSI_MASK;

    update(irq);
}

void aplic::write_mmsiaddrcfg(u32 val) {
    if (!(mmsiaddrcfgh & XMSIADDR_L))
        mmsiaddrcfg = val;
}

void aplic::write_mmsiaddrcfgh(u32 val) {
    if (!(mmsiaddrcfgh & XMSIADDR_L))
        mmsiaddrcfgh = val & XMSIADDR_MASK;
}

void aplic::write_smsiaddrcfg(u32 val) {
    if (!(smsiaddrcfgh & XMSIADDR_L))
        smsiaddrcfg = val;
}

void aplic::write_smsiaddrcfgh(u32 val) {
    if (!(smsiaddrcfgh & XMSIADDR_L))
        smsiaddrcfgh = val & XMSIADDR_MASK;
}

void aplic::write_setip(u32 val, size_t idx) {
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if ((base > 0) && (val & bit(i)))
            set_pending(m_irqs + base, true);
    }
}

void aplic::write_setipnum(u32 val) {
    if (val == 0 || val > NIRQ) {
        log_warn("setipnum: invalid irq (%u)", val);
        return;
    }

    set_pending(m_irqs + val - 1, true);
}

void aplic::write_clrip(u32 val, size_t idx) {
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if ((base > 0) && (val & bit(i)))
            set_pending(m_irqs + base, false);
    }
}

void aplic::write_clripnum(u32 val) {
    if (val == 0 || val > NIRQ) {
        log_warn("clripnum: invalid irq (%u)", val);
        return;
    }

    set_pending(m_irqs + val - 1, false);
}

void aplic::write_setie(u32 val, size_t idx) {
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if ((base > 0) && (val & bit(i)))
            set_enabled(m_irqs + base - 1, true);
    }
}

void aplic::write_setienum(u32 val) {
    if (val == 0 || val > NIRQ) {
        log_warn("setienum: invalid irq (%u)", val);
        return;
    }

    set_enabled(m_irqs + val - 1, true);
}

void aplic::write_clrie(u32 val, size_t idx) {
    size_t base = idx * 32;
    for (size_t i = 0; i < 32; i++, base++) {
        if ((base > 0) && (val & bit(i)))
            set_enabled(m_irqs + base - 1, false);
    }
}

void aplic::write_clrienum(u32 val) {
    if (val == 0 || val > NIRQ) {
        log_warn("clrienum: invalid irq (%u)", val);
        return;
    }

    set_enabled(m_irqs + val - 1, false);
}

void aplic::write_setipnum_le(u32 val) {
    if (is_big_endian())
        val = bswap(val);

    if (val == 0 || val > NIRQ) {
        log_warn("setipnum_le: invalid irq (%u)", val);
        return;
    }

    set_pending(m_irqs + val - 1, true);
}

void aplic::write_setipnum_be(u32 val) {
    if (is_little_endian())
        val = bswap(val);

    if (val == 0 || val > NIRQ) {
        log_warn("setipnum_be: invalid irq (%u)", val);
        return;
    }

    set_pending(m_irqs + val - 1, true);
}

void aplic::write_genmsi(u32 val) {
    genmsi = val & GENMSI_MASK;

    u32 hart = genmsi.get_field<GENMSI_HART>();
    u32 eiid = genmsi.get_field<GENMSI_EIID>();

    genmsi |= GENMSI_BUSY;
    send_msi(hart, 0, eiid);
    genmsi &= ~GENMSI_BUSY;
}

void aplic::write_idelivery(u32 val, size_t idx) {
    idcs[idx]->idelivery = val & 1;
    update();
}

void aplic::write_iforce(u32 val, size_t idx) {
    idcs[idx]->iforce = val & 1;
    update();
}

void aplic::write_ithreshold(u32 val, size_t idx) {
    idcs[idx]->ithreshold = val & 0xff;
    update();
}

void aplic::notify(size_t idx, bool level) {
    VCML_ERROR_ON(idx == 0 || idx >= NIRQ, "invalid irq: %zu", idx);

    irqinfo* irq = m_irqs + idx - 1;
    if (irq->sourcecfg & SOURCECFG_D) {
        u32 cidx = get_field<SOURCECFG_CI>(irq->sourcecfg);
        if (cidx < m_children.size())
            m_children[cidx]->notify(idx, level);
        else
            log_warn("invalid sourcecfg: 0x%08x", irq->sourcecfg);
        return;
    }

    if (irq->pending)
        return;

    switch (get_field<SOURCECFG_SM>(irq->sourcecfg)) {
    case SM_EDGE_RISE:
    case SM_LEVEL_HI:
        if (level) {
            irq->pending = true;
            update(irq);
        }
        break;

    case SM_EDGE_FALL:
    case SM_LEVEL_LO:
        if (!level) {
            irq->pending = true;
            update(irq);
        }
        break;

    default:
        break;
    }
}

void aplic::update() {
    for (irqinfo& irq : m_irqs) {
        if (irq.connected)
            update(&irq);
    }
}

void aplic::update(irqinfo* irq) {
    if (!irq->enabled || !irq->pending)
        return;

    if (is_msi()) {
        irq->pending = false;

        u32 hart = get_field<TARGETCFG_HART>(irq->targetcfg);
        u32 gidx = get_field<TARGETCFG_GIDX>(irq->targetcfg);
        u32 eiid = get_field<TARGETCFG_EIID>(irq->targetcfg);

        send_msi(hart, gidx, eiid);
    } else {
        u32 hart = get_field<TARGETCFG_HART>(irq->targetcfg);
        send_irq(hart);
    }
}

void aplic::send_msi(u32 target, u32 guest, u32 eiid) {
    aplic* r = root();

    u32 msicfglo, msicfghi;
    if (mmode) {
        msicfglo = r->mmsiaddrcfg;
        msicfghi = r->mmsiaddrcfgh;
    } else {
        msicfglo = r->smsiaddrcfg;
        msicfghi = r->smsiaddrcfgh;
    }

    if (mmode && guest > 0) {
        log_warn("guests not allowed on m-level");
        guest = 0;
    }

    u32 lhxs = get_field<MSICFG_LHXS>(msicfghi);
    u32 lhxw = get_field<MSICFG_LHXW>(msicfghi);
    u32 hhxs = get_field<MSICFG_HHXS>(msicfghi);
    u32 hhxw = get_field<MSICFG_HHXW>(msicfghi);

    u32 group = (target >> lhxw) & bitmask(hhxw);
    u32 hart = target & bitmask(lhxw);

    u64 ppn = msicfglo;
    ppn |= (u64)get_field<MSICFG_PPN>(msicfghi) << 32;
    ppn |= (u64)group << (hhxs + 12);
    ppn |= (u64)hart << lhxs;
    ppn |= (u64)guest;

    u64 addr = ppn << 12;

    if (failed(msi.writew(addr, eiid))) {
        log_warn("error sending msi %u to 0x%llx (group:%u hart:%u guest:%u)",
                 eiid, addr, group, hart, guest);
    }
}

void aplic::send_irq(u32 hart) {
    hartidc* idc = idcs[hart];
    if (!idc || !irq_out.exists(hart))
        return;

    u32 enabled = domaincfg & DOMAINCFG_IE;
    u32 itop = read_topi(hart);
    u32 idelivery = idc->idelivery;
    u32 iforce = idc->iforce;

    irq_out[hart] = enabled && idelivery && (iforce || itop);
}

aplic::hartidc::hartidc(size_t hart):
    idelivery(mkstr("idelivery%zu", hart), offset(hart) + 0x00, 0),
    iforce(mkstr("iforce%zu", hart), offset(hart) + 0x04, 0),
    ithreshold(mkstr("ithreshold%zu", hart), offset(hart) + 0x08, 0),
    topi(mkstr("topi%zu", hart), offset(hart) + 0x18, 0),
    claimi(mkstr("claimi%zu", hart), offset(hart) + 0x1c, 0) {
    idelivery.tag = hart;
    idelivery.sync_always();
    idelivery.allow_read_write();
    idelivery.on_write(&aplic::write_idelivery);

    iforce.tag = hart;
    iforce.sync_always();
    iforce.allow_read_write();
    iforce.on_write(&aplic::write_iforce);

    ithreshold.tag = hart;
    ithreshold.sync_always();
    ithreshold.allow_read_write();
    ithreshold.on_write(&aplic::write_ithreshold);

    topi.tag = hart;
    topi.sync_always();
    topi.allow_read_write();
    topi.on_read(&aplic::read_topi);

    claimi.tag = hart;
    claimi.sync_always();
    claimi.allow_read_write();
    claimi.on_read(&aplic::read_claimi);
}

aplic::hartidc::~hartidc() {
    // nothing to do
}

aplic::aplic(const sc_module_name& nm, aplic* parent):
    peripheral(nm),
    m_parent(parent),
    m_children(),
    m_irqs(),
    mmode("mmode", parent == nullptr),
    domaincfg("domaincfg", 0x0000, 0x80000000),
    sourcecfg("sourcecfg", 0x0004, 0),
    mmsiaddrcfg("mmsiaddrcfg", 0x1bc0, 0),
    mmsiaddrcfgh("mmsiaddrcfgh", 0x1bc4, 0),
    smsiaddrcfg("smsiaddrcfg", 0x1bc8, 0),
    smsiaddrcfgh("smsiaddrcfgh", 0x1bcc, 0),
    setip("setip", 0x1c00, 0),
    setipnum("setipnum", 0x1cdc, 0),
    in_clrip("in_clrip", 0x1d00, 0),
    clripnum("clripnum", 0x1ddc, 0),
    setie("setie", 0x1e00, 0),
    setienum("setienum", 0x1edc, 0),
    clrie("clrie", 0x1f00, 0),
    clrienum("clrienum", 0x1fdc, 0),
    setipnum_le("setipnum_le", 0x2000, 0),
    setipnum_be("setipnum_be", 0x2004, 0),
    genmsi("genmsi", 0x3000, 0),
    targetcfg("targetcfg", 0x3004, 0),
    idcs(),
    irq_out("irq_out"),
    irq_in("irq_in"),
    msi("msi"),
    in("in") {
    domaincfg.sync_always();
    domaincfg.allow_read_write();
    domaincfg.natural_accesses_only();
    domaincfg.on_write(&aplic::write_domaincfg);

    sourcecfg.sync_always();
    sourcecfg.allow_read_write();
    sourcecfg.natural_accesses_only();
    sourcecfg.on_read(&aplic::read_sourcecfg);
    sourcecfg.on_write(&aplic::write_sourcecfg);

    mmsiaddrcfg.sync_always();
    mmsiaddrcfg.natural_accesses_only();
    mmsiaddrcfg.on_write(&aplic::write_mmsiaddrcfg);

    mmsiaddrcfgh.sync_always();
    mmsiaddrcfgh.natural_accesses_only();
    mmsiaddrcfgh.on_write(&aplic::write_mmsiaddrcfgh);

    smsiaddrcfg.sync_always();
    smsiaddrcfg.natural_accesses_only();
    smsiaddrcfg.on_write(&aplic::write_smsiaddrcfg);

    smsiaddrcfgh.sync_always();
    smsiaddrcfgh.natural_accesses_only();
    smsiaddrcfgh.on_write(&aplic::write_smsiaddrcfgh);

    if (mmode) {
        mmsiaddrcfg.allow_read_write();
        mmsiaddrcfgh.allow_read_write();
        smsiaddrcfg.allow_read_write();
        smsiaddrcfgh.allow_read_write();
    } else {
        mmsiaddrcfg.deny_access();
        mmsiaddrcfgh.deny_access();
        smsiaddrcfg.deny_access();
        smsiaddrcfgh.deny_access();
    }

    setip.sync_always();
    setip.allow_read_write();
    setip.natural_accesses_only();
    setip.on_read(&aplic::read_setip);
    setip.on_write(&aplic::write_setip);

    setipnum.sync_always();
    setipnum.allow_read_write();
    setipnum.natural_accesses_only();
    setipnum.on_read(&aplic::read_zero);
    setipnum.on_write(&aplic::write_setipnum);

    in_clrip.sync_always();
    in_clrip.allow_read_write();
    in_clrip.natural_accesses_only();
    in_clrip.on_read(&aplic::read_in);
    in_clrip.on_write(&aplic::write_clrip);

    clripnum.sync_always();
    clripnum.allow_read_write();
    clripnum.natural_accesses_only();
    clripnum.on_read(&aplic::read_zero);
    clripnum.on_write(&aplic::write_clripnum);

    setie.sync_always();
    setie.allow_read_write();
    setie.natural_accesses_only();
    setie.on_read(&aplic::read_setie);
    setie.on_write(&aplic::write_setie);

    setienum.sync_always();
    setienum.allow_read_write();
    setienum.natural_accesses_only();
    setienum.on_read(&aplic::read_zero);
    setienum.on_write(&aplic::write_setienum);

    clrie.sync_always();
    clrie.allow_read_write();
    clrie.natural_accesses_only();
    clrie.on_read(&aplic::read_zero_idx);
    clrie.on_write(&aplic::write_clrie);

    clrienum.sync_always();
    clrienum.allow_read_write();
    clrienum.natural_accesses_only();
    clrienum.on_read(&aplic::read_zero);
    clrienum.on_write(&aplic::write_clrienum);

    setipnum_le.sync_always();
    setipnum_le.allow_read_write();
    setipnum_le.natural_accesses_only();
    setipnum_le.on_read(&aplic::read_zero);
    setipnum_le.on_write(&aplic::write_setipnum_le);

    setipnum_be.sync_always();
    setipnum_be.allow_read_write();
    setipnum_be.natural_accesses_only();
    setipnum_be.on_read(&aplic::read_zero);
    setipnum_be.on_write(&aplic::write_setipnum_be);

    genmsi.sync_always();
    genmsi.allow_read_write();
    genmsi.natural_accesses_only();
    genmsi.on_read(&aplic::read_genmsi);
    genmsi.on_write(&aplic::write_genmsi);

    targetcfg.sync_always();
    targetcfg.allow_read_write();
    targetcfg.natural_accesses_only();
    targetcfg.on_read(&aplic::read_targetcfg);
    targetcfg.on_write(&aplic::write_targetcfg);

    if (m_parent)
        m_parent->m_children.push_back(this);
}

aplic::~aplic() {
    for (auto* idc : idcs)
        delete idc;
}

void aplic::reset() {
    peripheral::reset();

    for (size_t i = 0; i < NIRQ; i++) {
        m_irqs[i].idx = i + 1;
        m_irqs[i].sourcecfg = 0;
        m_irqs[i].targetcfg = 0;
        m_irqs[i].connected = root()->irq_in.exists(i + 1);
        m_irqs[i].enabled = false;
        m_irqs[i].pending = false;
    }
}

void aplic::end_of_elaboration() {
    for (auto& [hart, port] : irq_out)
        idcs[hart] = new hartidc(hart);
}

void aplic::gpio_notify(const gpio_target_socket& socket) {
    size_t irq = irq_in.index_of(socket);
    log_debug("irq %zu %s", irq, socket.read() ? "set" : "cleared");
    notify(irq, socket.read());
}

} // namespace riscv
} // namespace vcml
