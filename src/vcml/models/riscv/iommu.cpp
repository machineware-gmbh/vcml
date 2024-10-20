/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/riscv/iommu.h"

namespace vcml {
namespace riscv {

enum address_bits : u64 {
    PHYS_BITS = 56,
    PAGE_BITS = 12,
    PPN_BITS = PHYS_BITS - PAGE_BITS,
    PAGE_SIZE = 1ull << PAGE_BITS,
    PAGE_MASK = bitmask(PAGE_BITS),
    PPN_MASK = bitmask(PPN_BITS, PAGE_BITS),
};

enum iommu_faults {
    IOMMU_FAULT_INST_FAULT = 1,
    IOMMU_FAULT_RD_ADDR_MISALIGNED = 4,
    IOMMU_FAULT_RD_FAULT = 5,
    IOMMU_FAULT_WR_ADDR_MISALIGNED = 6,
    IOMMU_FAULT_WR_FAULT = 7,
    IOMMU_FAULT_INST_FAULT_S = 12,
    IOMMU_FAULT_RD_FAULT_S = 13,
    IOMMU_FAULT_WR_FAULT_S = 15,
    IOMMU_FAULT_INST_FAULT_VS = 20,
    IOMMU_FAULT_RD_FAULT_VS = 21,
    IOMMU_FAULT_WR_FAULT_VS = 23,
    IOMMU_FAULT_DMA_DISABLED = 256,
    IOMMU_FAULT_DDT_LOAD_FAULT = 257,
    IOMMU_FAULT_DDT_INVALID = 258,
    IOMMU_FAULT_DDT_MISCONFIGURED = 259,
    IOMMU_FAULT_TTYPE_BLOCKED = 260,
    IOMMU_FAULT_MSI_LOAD_FAULT = 261,
    IOMMU_FAULT_MSI_INVALID = 262,
    IOMMU_FAULT_MSI_MISCONFIGURED = 263,
    IOMMU_FAULT_MRIF_FAULT = 264,
    IOMMU_FAULT_PDT_LOAD_FAULT = 265,
    IOMMU_FAULT_PDT_INVALID = 266,
    IOMMU_FAULT_PDT_MISCONFIGURED = 267,
    IOMMU_FAULT_DDT_CORRUPTED = 268,
    IOMMU_FAULT_PDT_CORRUPTED = 269,
    IOMMU_FAULT_MSI_PT_CORRUPTED = 270,
    IOMMU_FAULT_MRIF_CORRUIPTED = 271,
    IOMMU_FAULT_INTERNAL_DP_ERROR = 272,
    IOMMU_FAULT_MSI_WR_FAULT = 273,
    IOMMU_FAULT_PT_CORRUPTED = 274,
};

enum iommu_ttyp {
    IOMMU_TTYP_NONE = 0,
    IOMMU_TTYP_UXRX = 1,
    IOMMU_TTYP_UXRD = 2,
    IOMMU_TTYP_UXWR = 3,
    IOMMU_TTYP_TXRX = 5,
    IOMMU_TTYP_TXRD = 6,
    IOMMU_TTYP_TXWR = 7,
    IOMMU_TTYP_PCIE_ATS_REQ = 8,
    IOMMU_TTYP_PCIE_MSG_REQ = 9,
};

constexpr iommu_ttyp ttyp_from_tx(const tlm_generic_payload& tx,
                                  const tlm_sbi& info, bool ux) {
    if (tx.is_read()) {
        if (info.is_insn)
            return ux ? IOMMU_TTYP_UXRX : IOMMU_TTYP_TXRX;
        return ux ? IOMMU_TTYP_UXRD : IOMMU_TTYP_TXRD;
    }

    return ux ? IOMMU_TTYP_UXWR : IOMMU_TTYP_TXWR;
}

using CAPS_VERSION = field<0, 8, u64>;
using CAPS_IGS = field<28, 2, u64>;
using CAPS_PAS = field<32, 6, u64>;

enum caps_igs : u64 {
    CAPS_IGS_MSI = 0,
    CAPS_IGS_WSI = 1,
    CAPS_IGS_BOTH = 2,
};

enum caps_bits : u64 {
    CAPS_SV32 = bit(8),
    CAPS_SV39 = bit(9),
    CAPS_SV48 = bit(10),
    CAPS_SV57 = bit(11),
    CAPS_SVPBMT = bit(15),
    CAPS_SV32X4 = bit(16),
    CAPS_SV39X4 = bit(17),
    CAPS_SV48X4 = bit(18),
    CAPS_SV57X4 = bit(19),
    CAPS_AMO_MRIF = bit(21),
    CAPS_MSI_FLAT = bit(22),
    CAPS_MSI_MRIF = bit(23),
    CAPS_AMO_HWAD = bit(24),
    CAPS_ATS = bit(25),
    CAPS_T2GPA = bit(26),
    CAPS_END = bit(27),
    CAPS_HPM = bit(30),
    CAPS_DBG = bit(31),
    CAPS_PD8 = bit(38),
    CAPS_PD17 = bit(39),
    CAPS_PD20 = bit(40),
};

enum fctl_bits : u32 {
    FCTL_BE = bit(0),
    FCTL_WSI = bit(1),
    FCTL_GXL = bit(2),
};

using DDTP_IOMMU_MODE = field<0, 4, u64>;
using DDTP_IOMMU_PPN = field<PAGE_BITS, PPN_BITS, u64>;

enum ddtp_bits : u64 {
    DDTP_MODE_OFF = 0,
    DDTP_MODE_BARE = 1,
    DDTP_MODE_1LVL = 2,
    DDTP_MODE_2LVL = 3,
    DDTP_MODE_3LVL = 4,
    DDTP_BUSY = bit(4),
};

constexpr u64 mkctxid(u32 devid, u32 procid) {
    return (u64)procid << 32 | devid;
}

int iommu::fetch_context(u32 devid, u32 procid, bool dbg, bool dmi,
                         context& ctx) {
    u64 ctxid = mkctxid(devid, procid);
    if (mwr::stl_contains(m_contexts, ctxid)) {
        ctx = m_contexts[ctxid];
        return 0;
    }

    if (dmi)
        return -1;

    switch (ddtp.get_field<DDTP_IOMMU_MODE>()) {
    case DDTP_MODE_OFF:
        return IOMMU_FAULT_DMA_DISABLED;
    case DDTP_MODE_BARE:
        ctx.device_id = devid;
        ctx.process_id = procid;
        ctx.gatp = 0; // TODO: set mode bare
        ctx.satp = 0; // TODO: set mode bare
        ctx.ta = 0;
        ctx.tc = 0; // TODO: set valid bit!
        ctx.msiptp = 0;
        return 0;
    case DDTP_MODE_1LVL:
    case DDTP_MODE_2LVL:
    case DDTP_MODE_3LVL:
        break; // TODO
    default:
        return IOMMU_FAULT_DDT_MISCONFIGURED;
    }

    return 0;
}

int iommu::fetch_iotlb(context& ctx, u64 virt, bool dbg, bool dmi,
                       iotlb& entry) {
    if (!dbg)
        m_iotval2 = 0;

    return 0;
}

bool iommu::translate(const tlm_generic_payload& tx, const tlm_sbi& info,
                      bool dmi, iotlb& entry) {
    u32 devid = info.cpuid;
    u32 pasid = info.asid;
    u64 virt = tx.get_address() & ~PAGE_MASK;
    context ctx;

    if (int err = fetch_context(devid, pasid, info.is_debug, dmi, ctx)) {
        if (!info.is_debug && !dmi) {
            fault req{};
            req.cause = err;
            req.ttyp = ttyp_from_tx(tx, info, true);
            req.did = devid;
            req.pid = pasid;
            req.pv = !!pasid;
            req.priv = info.privilege > 0;
            req.iotval = virt;
            req.iotval2 = 0;
            report_fault(req);
        }

        return false;
    }

    if (int err = fetch_iotlb(ctx, virt, info.is_debug, dmi, entry)) {
        if (!info.is_debug && !dmi) {
            fault req{};
            req.cause = err;
            req.ttyp = ttyp_from_tx(tx, info, false);
            req.did = devid;
            req.pid = pasid;
            req.pv = !!pasid;
            req.priv = info.privilege > 0;
            req.iotval = virt;
            req.iotval2 = m_iotval2;
            report_fault(req);
        }

        return false;
    }

    return true;
}

void iommu::report_fault(const fault& req) {
    // TODO
}

void iommu::load_capabilities() {
    caps.set_field<CAPS_VERSION>(0x10); // version 1.0
    caps.set_field<CAPS_PAS>(PPN_BITS);

    if (cirq.is_stubbed() || firq.is_stubbed())
        caps.set_field<CAPS_IGS>(CAPS_IGS_MSI);
    else
        caps.set_field<CAPS_IGS>(CAPS_IGS_BOTH);

    caps.set_bit<CAPS_SV32>(sv32);
    caps.set_bit<CAPS_SV39>(sv39 || sv48 || sv57);
    caps.set_bit<CAPS_SV48>(sv48 || sv57);
    caps.set_bit<CAPS_SV57>(sv57);

    caps.set_bit<CAPS_SVPBMT>(svpbmt);

    caps.set_bit<CAPS_SV32X4>(sv32x4);
    caps.set_bit<CAPS_SV39X4>(sv39x4);
    caps.set_bit<CAPS_SV48X4>(sv48x4);
    caps.set_bit<CAPS_SV57X4>(sv57x4);

    caps.set_bit<CAPS_AMO_MRIF>(amo_mrif);
    caps.set_bit<CAPS_MSI_FLAT>(msi_flat);
    caps.set_bit<CAPS_MSI_MRIF>(msi_mrif);
    caps.set_bit<CAPS_AMO_HWAD>(amo_hwad);

    caps.set_bit<CAPS_T2GPA>(t2gpa);

    caps.set_bit<CAPS_ATS>(true);
    caps.set_bit<CAPS_HPM>(true);
    caps.set_bit<CAPS_DBG>(true);

    caps.set_bit<CAPS_PD8>(pd8);
    caps.set_bit<CAPS_PD17>(pd17);
    caps.set_bit<CAPS_PD20>(pd20);
}

void iommu::write_fctl(u32 val) {
    u32 mask = FCTL_GXL;
    if (!cirq.is_stubbed() && !firq.is_stubbed())
        mask |= FCTL_WSI;
    fctl = (fctl & ~mask) | (val & mask);
}

void iommu::write_ddtp(u64 val) {
    u64 mask = DDTP_IOMMU_MODE::MASK | DDTP_IOMMU_PPN::MASK;

    if (((ddtp ^ val) & mask) == 0)
        return; // no change

    ddtp |= DDTP_BUSY;

    m_contexts.clear();
    m_iotlb.clear();

    ddtp = (ddtp & ~mask) | (val & mask);
    ddtp &= ~DDTP_BUSY;
}

void iommu::worker() {
    while (true) {
        wait(m_workev);
    }
}

iommu::iommu(const sc_module_name& nm):
    peripheral(nm),
    m_contexts(),
    m_iotlb(),
    m_workev("workev"),
    m_iotval2(0),
    m_dmi_lo(~0ull),
    m_dmi_hi(0ull),
    sv32("sv32", true),
    sv39("sv39", true),
    sv48("sv48", true),
    sv57("sv57", true),
    svpbmt("svpbmt", true),
    sv32x4("sv32x4", true),
    sv39x4("sv39x4", true),
    sv48x4("sv48x4", true),
    sv57x4("sv57x4", true),
    amo_mrif("amo_mrif", true),
    msi_flat("msi_flat", true),
    msi_mrif("msi_mrif", true),
    amo_hwad("amo_hwad", true),
    t2gpa("t2gpa", true),
    pd8("pd8", true),
    pd17("pd17", true),
    pd20("pd20", true),
    caps("caps", 0x0, 0),
    fctl("fctl", 0x8, 0),
    ddtp("ddtp", 0x10, 0),
    cqb("cqb", 0x18, 0),
    cqh("cqh", 0x20, 0),
    cqt("cqt", 0x24, 0),
    fqb("fqb", 0x28, 0),
    fqh("fqh", 0x30, 0),
    fqt("fqt", 0x34, 0),
    pqb("pqb", 0x38, 0),
    pqh("pqh", 0x40, 0),
    pqt("pqt", 0x44, 0),
    cqcsr("cqcsr", 0x48, 0),
    fqcsr("fqcsr", 0x4c, 0),
    pqcsr("pqcsr", 0x50, 0),
    ipsr("ipsr", 0x54, 0),
    iocntovf("iocntovf", 0x58, 0),
    iocntinh("iocntinh", 0x5c, 0),
    iohpmcycles("iohpmcycles", 0x60, 0),
    iohpmctr("iohpmctr", 0x68, 0),
    iohpmevt("iohpmevt", 0x160, 0),
    tr_req_iova("tr_req_iova", 0x258, 0),
    tr_req_ctl("tr_req_ctl", 0x260, 0),
    tr_response("tr_response", 0x268, 0),
    icvec("icvec", 0x2f8, 0),
    msi_cfg_tbl("msi_cfg_tbl", 0x300, 0),
    out("out"),
    in("in", IOMMU_AS_DEFAULT),
    dma("dma", IOMMU_AS_DMA),
    cirq("cirq"),
    firq("firq"),
    pmirq("pmirq"),
    pirq("pirq") {
    caps.allow_read_only();
    caps.sync_never();

    fctl.allow_read_write();
    fctl.sync_always();
    fctl.on_write(&iommu::write_fctl);

    ddtp.allow_read_write();
    ddtp.sync_always();
    ddtp.on_write(&iommu::write_ddtp);

    load_capabilities();

    SC_HAS_PROCESS(iommu);
    SC_THREAD(worker);
}

iommu::~iommu() {
    // nothing to do
}

void iommu::reset() {
    peripheral::reset();
    load_capabilities();
    m_contexts.clear();
    m_iotlb.clear();
    invalidate_direct_mem_ptr(0ull, ~0ull);
}

unsigned int iommu::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                            address_space as) {
    if (as == IOMMU_AS_DEFAULT)
        return peripheral::receive(tx, info, as);

    iotlb entry{};
    u64 virt = tx.get_address();

    if (!translate(tx, info, false, entry)) {
        tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    if (tx.is_read() && !entry.r && !info.is_debug) {
        tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }

    if (tx.is_write() && !entry.w && !info.is_debug) {
        tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }

    tx.set_address((entry.ppn << PAGE_BITS) | (virt & PAGE_MASK));
    unsigned int n = out.send(tx, info);
    tx.set_address(virt);
    return n;
}

static bool no_dmi(tlm_dmi& dmi) {
    // see SystemC LRM 11.3.5 (n)
    dmi.allow_read_write();
    dmi.set_start_address(0);
    dmi.set_end_address(-1ull);
    return false;
}

bool iommu::get_direct_mem_ptr(tlm_target_socket& origin,
                               tlm_generic_payload& tx, tlm_dmi& dmi) {
    if (origin.as == IOMMU_AS_DEFAULT)
        return peripheral::get_direct_mem_ptr(origin, tx, dmi);

    iotlb entry{};
    u64 virt = tx.get_address();
    u64 page = virt & ~PAGE_MASK;

    const tlm_sbi& info = tx_get_sbi(tx);
    if (!translate(tx, info, true, entry))
        return no_dmi(dmi);

    u64 phys = entry.ppn << PAGE_BITS;
    range addr(phys, phys + PAGE_SIZE - 1);
    tx.set_address(phys);

    if (!out.dmi_cache().lookup(addr, VCML_ACCESS_NONE, dmi)) {
        if (!out->get_direct_mem_ptr(tx, dmi))
            return no_dmi(dmi);

        out.dmi_cache().insert(dmi);
    }

    tx.set_address(virt);

    int dmirw = dmi.get_granted_access();
    u64 dmilo = dmi.get_start_address();
    u64 dmihi = dmi.get_end_address();
    u8* dmiptr = dmi.get_dmi_ptr();

    if (dmihi < phys || dmilo >= phys + PAGE_SIZE)
        return no_dmi(dmi);

    if (!entry.r)
        dmirw &= ~tlm_dmi::DMI_ACCESS_READ;
    if (!entry.w)
        dmirw &= ~tlm_dmi::DMI_ACCESS_WRITE;
    if (dmirw == tlm_dmi::DMI_ACCESS_NONE)
        return no_dmi(dmi);

    m_dmi_lo = min(m_dmi_lo, phys);
    m_dmi_hi = max(m_dmi_hi, phys + PAGE_SIZE - 1);

    dmi.set_granted_access((tlm_dmi::dmi_access_e)dmirw);
    dmi.set_start_address(page);
    dmi.set_end_address(page + PAGE_SIZE - 1);
    dmi.set_dmi_ptr(dmiptr + phys - dmilo);
    dmi.set_read_latency(dmi.get_read_latency() + read_cycles());
    dmi.set_write_latency(dmi.get_write_latency() + read_cycles());
    return true;
}

void iommu::invalidate_direct_mem_ptr(tlm_initiator_socket&, u64 s, u64 e) {
    invalidate_direct_mem_ptr(s, e);
}

void iommu::invalidate_direct_mem_ptr(u64 start, u64 end) {
    if (m_dmi_hi < m_dmi_lo)
        return;

    if (start > m_dmi_hi || end < m_dmi_lo)
        return;

    m_dmi_lo = ~0ull;
    m_dmi_hi = 0;

    dma->invalidate_direct_mem_ptr(0ull, ~0ull);
}

VCML_EXPORT_MODEL(vcml::riscv::iommu, name, args) {
    return new iommu(name);
}

} // namespace riscv
} // namespace vcml
