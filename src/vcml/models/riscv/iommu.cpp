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

enum proc_ids : u32 {
    PASID_NONE = ~0u,
};

enum address_bits : u64 {
    PHYS_BITS = 56,
    PAGE_BITS = 12,
    PPN_BITS = PHYS_BITS - PAGE_BITS,
    PAGE_SIZE = 1ull << PAGE_BITS,
    PAGE_MASK = bitmask(PAGE_BITS),
    PPN_MASK = bitmask(PPN_BITS, PAGE_BITS),
};

using PTE_PPN = field<10, PPN_BITS, u64>;
using PTE_PBMT = field<61, 2, u64>;

enum pte_bits : u32 {
    PTE_V = bit(0),
    PTE_R = bit(1),
    PTE_W = bit(2),
    PTE_X = bit(3),
    PTE_U = bit(4),
    PTE_G = bit(5),
    PTE_A = bit(6),
    PTE_D = bit(7),
    PTE_RWX = PTE_R | PTE_W | PTE_X,
};

enum iotval2_bits : u64 {
    IOTVAL2_INDIRECT = bit(0),
    IOTVAL2_WRITE = bit(1),
};

using DDTE_PPN = field<10, PPN_BITS, u64>;

enum ddte_bits : u64 {
    DDTE_V = bit(0),
    DDTE_MASK = DDTE_V | DDTE_PPN::MASK,
};

using PDTE_PPN = field<10, PPN_BITS, u64>;

enum pdte_bits : u64 {
    PDTE_V = bit(0),
    PDTE_MASK = DDTE_V | DDTE_PPN::MASK,
};

enum tc_bits : u64 {
    TC_V = bit(0),
    TC_EN_ATS = bit(1),
    TC_EN_PRI = bit(2),
    TC_T2GPA = bit(3),
    TC_DTF = bit(4),
    TC_PDTV = bit(5),
    TC_PRPR = bit(6),
    TC_GADE = bit(7),
    TC_SADE = bit(8),
    TC_DPE = bit(9),
    TC_SBE = bit(10),
    TC_SXL = bit(11),
};

using TA_PSCID = field<12, 20, u64>;

enum ta_bits : u64 {
    TA_V = bit(0),
    TA_ENS = bit(1),
    TA_SUM = bit(2),
};

using IOHGATP_PPN = field<0, PPN_BITS, u64>;
using IOHGATP_GSCID = field<44, 16, u64>;
using IOHGATP_MODE = field<60, 4, u64>;

enum iohgatp_modes : u64 {
    IOHGATP_BARE = 0,
    IOHGATP_SV32X4 = 8,
    IOHGATP_SV39X4 = 8,
    IOHGATP_SV48X4 = 9,
    IOHGATP_SV57X4 = 10,
};

using IOSATP_PPN = field<0, PPN_BITS, u64>;
using IOSATP_MODE = field<60, 4, u64>;

enum iosatp_modes : u64 {
    IOSATP_BARE = 0,
    IOSATP_SV32 = 8,
    IOSATP_SV39 = 8,
    IOSATP_SV48 = 9,
    IOSATP_SV57 = 10,
};

using PDTP_PPN = field<0, PPN_BITS, u64>;
using PDTP_MODE = field<60, 4, u64>;

enum pdtp_modes : u64 {
    PDTP_BARE = 0,
    PDTP_PD8 = 1,
    PDTP_PD17 = 2,
    PDTP_PD20 = 3,
};

using MSI_PPN = field<0, PPN_BITS, u64>;
using MSI_MODE = field<60, 4, u64>;

enum msi_modes : u64 {
    MSI_OFF = 0,
    MSI_FLAT = 1,
    MSI_MASK = bitmask(52),
    MSI_PATTERN = bitmask(52),
};

enum tablewalk_errors {
    TWALK_FAULT_NONE = 0,
    TWALK_FAULT_G_STAGE,
    TWALK_FAULT_PTE_FETCH,
    TWALK_FAULT_PTE_INVALID,
    TWALK_FAULT_PTE_PROT_U,
    TWALK_FAULT_PTE_PROT_S,
    TWALK_FAULT_PTE_CORRUPTED,
    TWALK_FAULT_PPN_MISALIGNED,
    TWALK_FAULT_AD_UPDATE,
};

enum iommu_faults {
    IOMMU_ACCESS_FAULT_X = 1,
    IOMMU_MISALIGNED_FAULT_R = 4,
    IOMMU_ACCESS_FAULT_R = 5,
    IOMMU_MISALIGNED_FAULT_W = 6,
    IOMMU_ACCESS_FAULT_W = 7,
    IOMMU_PAGE_FAULT_X = 12,
    IOMMU_PAGE_FAULT_R = 13,
    IOMMU_PAGE_FAULT_W = 15,
    IOMMU_GUEST_PAGE_FAULT_X = 20,
    IOMMU_GUEST_PAGE_FAULT_R = 21,
    IOMMU_GUEST_PAGE_FAULT_W = 23,
    IOMMU_FAULT_DMA_DISABLED = 256,
    IOMMU_FAULT_DDT_LOAD_FAULT = 257,
    IOMMU_FAULT_DDT_INVALID = 258,
    IOMMU_FAULT_DDT_MISCONFIGURED = 259,
    IOMMU_FAULT_TTYPE_BLOCKED = 260,
    IOMMU_FAULT_MSI_LOAD_FAULT = 261,
    IOMMU_FAULT_MSI_INVALID = 262,
    IOMMU_FAULT_MSI_MISCONFIGURED = 263,
    IOMMU_FAULT_MRIF_ACCESS = 264,
    IOMMU_FAULT_PDT_LOAD_FAULT = 265,
    IOMMU_FAULT_PDT_INVALID = 266,
    IOMMU_FAULT_PDT_MISCONFIGURED = 267,
    IOMMU_FAULT_DDT_CORRUPTED = 268,
    IOMMU_FAULT_PDT_CORRUPTED = 269,
    IOMMU_FAULT_MSI_PT_CORRUPTED = 270,
    IOMMU_FAULT_MRIF_CORRUPTED = 271,
    IOMMU_FAULT_INTERNAL_DP_ERROR = 272,
    IOMMU_FAULT_MSI_WR_FAULT = 273,
    IOMMU_FAULT_PT_CORRUPTED = 274,
};

constexpr const char* iommu_fault_str(u32 fault) {
    switch (fault) {
    case IOMMU_ACCESS_FAULT_X:
        return "IOMMU_ACCESS_FAULT_X";
    case IOMMU_MISALIGNED_FAULT_R:
        return "IOMMU_MISALIGNED_FAULT_R";
    case IOMMU_ACCESS_FAULT_R:
        return "IOMMU_ACCESS_FAULT_R";
    case IOMMU_MISALIGNED_FAULT_W:
        return "IOMMU_MISALIGNED_FAULT_W";
    case IOMMU_ACCESS_FAULT_W:
        return "IOMMU_ACCESS_FAULT_W";
    case IOMMU_PAGE_FAULT_X:
        return "IOMMU_FAULT_X";
    case IOMMU_PAGE_FAULT_R:
        return "IOMMU_FAULT_R";
    case IOMMU_PAGE_FAULT_W:
        return "IOMMU_FAULT_W";
    case IOMMU_GUEST_PAGE_FAULT_X:
        return "IOMMU_GUEST_PAGE_FAULT_X";
    case IOMMU_GUEST_PAGE_FAULT_R:
        return "IOMMU_GUEST_PAGE_FAULT_R";
    case IOMMU_GUEST_PAGE_FAULT_W:
        return "IOMMU_GUEST_PAGE_FAULT_W";
    case IOMMU_FAULT_DMA_DISABLED:
        return "IOMMU_FAULT_DMA_DISABLED";
    case IOMMU_FAULT_DDT_LOAD_FAULT:
        return "IOMMU_FAULT_DDT_LOAD_FAULT";
    case IOMMU_FAULT_DDT_INVALID:
        return "IOMMU_FAULT_DDT_INVALID";
    case IOMMU_FAULT_DDT_MISCONFIGURED:
        return "IOMMU_FAULT_DDT_MISCONFIGURED";
    case IOMMU_FAULT_TTYPE_BLOCKED:
        return "IOMMU_FAULT_TTYPE_BLOCKED";
    case IOMMU_FAULT_MSI_LOAD_FAULT:
        return "IOMMU_FAULT_MSI_LOAD_FAULT";
    case IOMMU_FAULT_MSI_INVALID:
        return "IOMMU_FAULT_MSI_INVALID";
    case IOMMU_FAULT_MSI_MISCONFIGURED:
        return "IOMMU_FAULT_MSI_MISCONFIGURED";
    case IOMMU_FAULT_MRIF_ACCESS:
        return "IOMMU_FAULT_MRIF_ACCESS";
    case IOMMU_FAULT_PDT_LOAD_FAULT:
        return "IOMMU_FAULT_PDT_LOAD_FAULT";
    case IOMMU_FAULT_PDT_INVALID:
        return "IOMMU_FAULT_PDT_INVALID";
    case IOMMU_FAULT_PDT_MISCONFIGURED:
        return "IOMMU_FAULT_PDT_MISCONFIGURED";
    case IOMMU_FAULT_DDT_CORRUPTED:
        return "IOMMU_FAULT_DDT_CORRUPTED";
    case IOMMU_FAULT_PDT_CORRUPTED:
        return "IOMMU_FAULT_PDT_CORRUPTED";
    case IOMMU_FAULT_MSI_PT_CORRUPTED:
        return "IOMMU_FAULT_MSI_PT_CORRUPTED";
    case IOMMU_FAULT_MRIF_CORRUPTED:
        return "IOMMU_FAULT_MRIF_CORRUPTED";
    case IOMMU_FAULT_INTERNAL_DP_ERROR:
        return "IOMMU_FAULT_INTERNAL_DP_ERROR";
    case IOMMU_FAULT_MSI_WR_FAULT:
        return "IOMMU_FAULT_MSI_WR_FAULT";
    case IOMMU_FAULT_PT_CORRUPTED:
        return "IOMMU_FAULT_PT_CORRUPTED";
    default:
        return "IOMMU_FAULT_UNKNOWN";
    }
}

constexpr int iommu_access_fault(bool wnr) {
    return wnr ? IOMMU_ACCESS_FAULT_W : IOMMU_ACCESS_FAULT_R;
}

constexpr int iommu_misaligned_fault(bool wnr) {
    return wnr ? IOMMU_MISALIGNED_FAULT_W : IOMMU_MISALIGNED_FAULT_R;
}

constexpr int iommu_page_fault(bool wnr) {
    return wnr ? IOMMU_PAGE_FAULT_W : IOMMU_PAGE_FAULT_R;
}

constexpr int iommu_guest_page_fault(bool wnr) {
    return wnr ? IOMMU_GUEST_PAGE_FAULT_W : IOMMU_GUEST_PAGE_FAULT_R;
}

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

constexpr const char* iommu_ttyp_str(u32 ttyp) {
    switch (ttyp) {
    case IOMMU_TTYP_NONE:
        return "IOMMU_TTYP_NONE";
    case IOMMU_TTYP_UXRX:
        return "IOMMU_TTYP_UXRX";
    case IOMMU_TTYP_UXRD:
        return "IOMMU_TTYP_UXRD";
    case IOMMU_TTYP_UXWR:
        return "IOMMU_TTYP_UXWR";
    case IOMMU_TTYP_TXRX:
        return "IOMMU_TTYP_TXRX";
    case IOMMU_TTYP_TXRD:
        return "IOMMU_TTYP_TXRD";
    case IOMMU_TTYP_TXWR:
        return "IOMMU_TTYP_TXWR";
    case IOMMU_TTYP_PCIE_ATS_REQ:
        return "IOMMU_TTYP_PCIE_ATS_REQ";
    case IOMMU_TTYP_PCIE_MSG_REQ:
        return "IOMMU_TTYP_PCIE_MSG_REQ";
    default:
        return "IOMMU_TTYP_UNKNOWN";
    }
}

static iommu_ttyp ttyp_from_tx(const tlm_generic_payload& tx,
                               const tlm_sbi& info, bool txr, bool ats) {
    if (ats)
        return IOMMU_TTYP_PCIE_ATS_REQ;

    if (tx.is_read()) {
        if (info.is_insn)
            return txr ? IOMMU_TTYP_TXRX : IOMMU_TTYP_UXRX;
        return txr ? IOMMU_TTYP_TXRD : IOMMU_TTYP_UXRD;
    }

    return txr ? IOMMU_TTYP_TXWR : IOMMU_TTYP_UXWR;
}

enum iommu_work : u32 {
    IOMMU_WORK_COMMAND = bit(0),
    IOMMU_WORK_FAULT = bit(1),
    IOMMU_WORK_PGREQ = bit(2),
    IOMMU_WORK_TRREQ = bit(3),
};

enum iommu_opcode : u64 {
    IOMMU_OPCODE_IOTINVAL = 1,
    IOMMU_OPCODE_IOFENCE = 2,
    IOMMU_OPCODE_IOTDIR = 3,
    IOMMU_OPCODE_ATS = 4,
};

using IOTINVAL_PSCID = field<2, 20, u64>;
using IOTINVAL_GSCID = field<34, 16, u64>;

enum iotinval_bits : u64 {
    IOTINVAL_AV = bit(0),
    IOTINVAL_PSCV = bit(22),
    IOTINVAL_GV = bit(23),
};

enum iotinval_func3 : u64 {
    IOTINVAL_VMA = 0,
    IOTINVAL_GVMA = 1,
};

using IOFENCE_DATA = field<22, 32, u64>;

enum iotfence_bits : u64 {
    IOFENCE_AV = bit(0),
    IOFENCE_WSI = bit(1),
    IOFENCE_PR = bit(2),
    IOFENCE_PW = bit(3),
};

enum iofence_func3 : u64 {
    IOFENCE_C = 0,
};

using IODIR_PID = field<2, 20, u64>;
using IODIR_DID = field<30, 16, u64>;

enum iodir_bits : u64 {
    IODIR_DV = bit(23),
};

enum iodir_func3 : u64 {
    IODIR_DDT = 0,
    IODIR_PDT = 1,
};

using ATS_PID = field<2, 20, u64>;
using ATS_RID = field<30, 16, u64>;
using ATS_DSEG = field<46, 8, u64>;

enum ats_bits : u64 {
    ATS_PV = bit(22),
    ATS_DSV = bit(23),
};

enum ats_func3 : u64 {
    ATS_INVAL = 0,
    ATS_PRGR = 1,
};

enum iommu_event : u32 {
    IOMMU_EVENT_NONE = 0,
    IOMMU_EVENT_UX_REQ = 1,
    IOMMU_EVENT_TX_REQ = 2,
    IOMMU_EVENT_ATS_REQ = 3,
    IOMMU_EVENT_TLB_MISS = 4,
    IOMMU_EVENT_DD_WALK = 5,
    IOMMU_EVENT_PD_WALK = 6,
    IOMMU_EVENT_VS_WALK = 7,
    IOMMU_EVENT_GS_WALK = 8,
    IOMMU_EVENT_MAX,
};

constexpr u32 iommu_event_req(bool tx) {
    return tx ? IOMMU_EVENT_TX_REQ : IOMMU_EVENT_UX_REQ;
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

using DDTP_MODE = field<0, 4, u64>;
using DDTP_PPN = field<10, PPN_BITS, u64>;

enum ddtp_bits : u64 {
    DDTP_MODE_OFF = 0,
    DDTP_MODE_BARE = 1,
    DDTP_MODE_1LVL = 2,
    DDTP_MODE_2LVL = 3,
    DDTP_MODE_3LVL = 4,
    DDTP_BUSY = bit(4),
};

using CQB_LOG2SZ = field<0, 5, u64>;
using CQB_PPN = field<10, 44, u64>;

using FQB_LOG2SZ = field<0, 5, u64>;
using FQB_PPN = field<10, 44, u64>;

using PQB_LOG2SZ = field<0, 5, u64>;
using PQB_PPN = field<10, 44, u64>;

enum cqcsr_bits : u32 {
    CQCSR_CQEN = bit(0),
    CQCSR_CIE = bit(1),
    CQCSR_CQMF = bit(8),
    CQCSR_CMDTO = bit(9),
    CQCSR_CMDILL = bit(10),
    CQCSR_FENCE_W_IP = bit(11),
    CQCSR_CQON = bit(16),
    CQCSR_BUSY = bit(17),
    CQCSR_PENDING = CQCSR_CQMF | CQCSR_CMDTO | CQCSR_CMDILL | CQCSR_FENCE_W_IP,
};

enum fqcsr_bits : u32 {
    FQCSR_FQEN = bit(0),
    FQCSR_FIE = bit(1),
    FQCSR_FQMF = bit(8),
    FQCSR_FQOF = bit(9),
    FQCSR_FQON = bit(16),
    FQCSR_BUSY = bit(17),
    FQCSR_PENDING = FQCSR_FQMF | FQCSR_FQOF,
};

enum pqcsr_bits : u32 {
    PQCSR_PQEN = bit(0),
    PQCSR_PIE = bit(1),
    PQCSR_PQMF = bit(8),
    PQCSR_PQOF = bit(9),
    PQCSR_PQON = bit(16),
    PQCSR_BUSY = bit(17),
    PQCSR_PENDING = PQCSR_PQMF | PQCSR_PQOF,
};

enum ipsr_bits : u32 {
    IPSR_CIP = bit(0),
    IPSR_FIP = bit(1),
    IPSR_PMIP = bit(2),
    IPSR_PIP = bit(3),
};

enum counter_bits : u64 {
    COUNTER_INH = bit(0),
    COUNTER_OVF = bit(0),
    COUNTER_OV = bit(63),
};

using IOHPMEVT_EVENTID = field<0, 15, u64>;
using IOHPMEVT_PID_PSCID = field<16, 20, u64>;
using IOHPMEVT_DID_GSCID = field<36, 24, u64>;

enum iohpmctr_bits : u64 {
    IOHPMEVT_DMASK = bit(15),
    IOHPMEVT_PV_PSCV = bit(60),
    IOHPMEVT_DV_GSCV = bit(61),
    IOHPMEVT_IDT = bit(62),
    IOHPMEVT_OF = bit(63),
};

using TR_REQ_CTL_PID = field<12, 20, u64>;
using TR_REQ_CTL_DID = field<40, 24, u64>;

enum tr_req_ctl_bits : u64 {
    TR_REQ_CTL_BUSY = bit(0),
    TR_REQ_CTL_PRIV = bit(1),
    TR_REQ_CTL_EXE = bit(2),
    TR_REQ_CTL_NW = bit(3),
    TR_REQ_CTL_PV = bit(32),
    TR_REQ_CTL_MASK = TR_REQ_CTL_BUSY | TR_REQ_CTL_PRIV | TR_REQ_CTL_EXE |
                      TR_REQ_CTL_NW | TR_REQ_CTL_PID::MASK | TR_REQ_CTL_PV |
                      TR_REQ_CTL_DID::MASK,
};

using TR_RESPONSE_PBMT = field<7, 2, u64>;
using TR_RESPONSE_PPN = field<10, PPN_BITS, u64>;

enum tr_response_bits : u64 {
    TR_RESPONSE_FAULT = bit(0),
    TR_RESPONSE_S = bit(9),
};

enum msi_type : u32 {
    MSI_CQ = 0,
    MSI_FQ = 1,
    MSI_PM = 2,
    MSI_PQ = 3,
};

using MSI_VECTOR_DATA = field<0, 32, u64>;
using MSI_VECTOR_CTRL = field<32, 32, u64>;

enum msi_vector_bits : u64 {
    MSI_VECTOR_CTRL_M = bit(0),
    MSI_VECTOR_ADDR = bitmask(54, 2),
};

enum msipte_bits : u64 {
    MSIPTE_V = bit(0),
    MSIPTE_C = bit(63),
};

using MSIPTE_M = field<1, 2, u64>;
using MSIPTE_PPN = field<10, 44, u64>;
using MSIPTE_MRIF = field<7, 47, u64>;
using MSIPTE_NPPN = field<10, 44, u64>;

enum msipte_mode : u64 {
    MSIPTE_MODE_MRIF = 1,
    MSIPTE_MODE_FLAT = 3,
};

constexpr u64 msipte_nid(u64 dword1) {
    return extract(dword1, 60, 1) << 10 | (dword1 & bitmask(10));
}

constexpr u64 mkctxid(u32 devid, u32 pasid) {
    return (u64)pasid << 32 | devid;
}

constexpr u32 sbi_get_pasid(const tlm_sbi& info) {
    if (info.asid == SBI_ASID_GLOBAL)
        return PASID_NONE;
    return (u32)info.asid;
}

constexpr u64 msi_extract(u64 val, u64 mask) {
    u64 result = 0;

    for (int i = 64; i > 0; i--) {
        if (mask & bit(i - 1)) {
            result <<= 1;
            result |= !!(val & bit(i - 1));
        }
    }

    return result;
}

tlm_response_status iommu::dma_read(u64 addr, void* data, size_t size,
                                    bool excl, bool dbg) {
    if (excl && size <= 8) {
        auto rw = dbg ? VCML_ACCESS_READ : VCML_ACCESS_READ_WRITE;
        u8* dmi_ptr = out.lookup_dmi_ptr(addr, size, rw);
        if (dmi_ptr) {
            memcpy(data, dmi_ptr, size);
            if (dbg)
                return TLM_OK_RESPONSE;

            m_dma_addr = addr;
            m_dma_xptr = dmi_ptr;
            m_dma_xval = 0;
            memcpy(&m_dma_xval, dmi_ptr, size);
            return TLM_OK_RESPONSE;
        }
    }

    tlm_sbi info;
    if (dbg)
        info = SBI_DEBUG;
    else if (excl)
        info = SBI_EXCL;
    else
        info = SBI_NONE;

    ddtp |= DDTP_BUSY;
    auto res = out.read(addr, data, size, info);
    ddtp &= ~DDTP_BUSY;
    return res;
}

tlm_response_status iommu::dma_write(u64 addr, void* data, size_t size,
                                     bool* excl, bool debug) {
    if (debug)
        return TLM_OK_RESPONSE;

    if (excl && size <= 8 && m_dma_xptr && addr == m_dma_addr &&
        mwr::atomic_cas(m_dma_xptr, &m_dma_xval, data, size)) {
        m_dma_addr = ~0ull;
        m_dma_xptr = nullptr;
        m_dma_xval = 0;
        *excl = true;
        return TLM_OK_RESPONSE;
    }

    unsigned int nbytes = 0;
    tlm_sbi info = excl ? SBI_EXCL : SBI_NONE;

    ddtp |= DDTP_BUSY;
    auto res = out.write(addr, data, size, info, &nbytes);
    ddtp &= ~DDTP_BUSY;

    if (excl)
        *excl = nbytes == size;

    return res;
}

bool iommu::check_context(const context& ctx) const {
    if (!(caps & CAPS_ATS) && (ctx.tc & (TC_EN_ATS | TC_EN_PRI | TC_PRPR)))
        return false;
    if (!(ctx.tc & TC_EN_ATS) && (ctx.tc & (TC_T2GPA | TC_EN_PRI)))
        return false;
    if (!(ctx.tc & TC_EN_PRI) && (ctx.tc & TC_PRPR))
        return false;
    if (!(caps & CAPS_T2GPA) && (ctx.tc & TC_T2GPA))
        return false;
    if ((ctx.tc & TC_T2GPA) &&
        (get_field<IOHGATP_MODE>(ctx.gatp) == IOHGATP_BARE))
        return false;
    if (ctx.tc & TC_PDTV) {
        switch (get_field<PDTP_MODE>(ctx.satp)) {
        case PDTP_PD8:
            if (!(caps & CAPS_PD8))
                return false;
        case PDTP_PD17:
            if (!(caps & CAPS_PD17))
                return false;
        case PDTP_PD20:
            if (!(caps & CAPS_PD20))
                return false;
        default:
            break;
        }
    } else {
        if (ctx.tc & TC_DPE)
            return false;
        if (ctx.tc & TC_SXL) { // 32bit S-mode
            switch (get_field<IOSATP_MODE>(ctx.satp)) {
            case IOSATP_BARE:
                break;
            case IOSATP_SV32:
                if (!(caps & CAPS_SV32))
                    return false;
                break;
            default:
                return false;
            }
        } else { // 64bit S-mode
            switch (get_field<IOSATP_MODE>(ctx.satp)) {
            case IOSATP_BARE:
                break;
            case IOSATP_SV39:
                if (!(caps & CAPS_SV39))
                    return false;
                break;
            case IOSATP_SV48:
                if (!(caps & CAPS_SV48))
                    return false;
                break;
            case IOSATP_SV57:
                if (!(caps & CAPS_SV57))
                    return false;
                break;
            default:
                return false;
            }
        }
    }

    if (fctl & FCTL_GXL) { // 32bit G-mode
        switch (get_field<IOHGATP_MODE>(ctx.gatp)) {
        case IOHGATP_BARE:
            break;
        case IOHGATP_SV32X4:
            if (!(caps & CAPS_SV32X4))
                return false;
            break;
        default:
            return false;
        }
    } else { // 64bit S-mode
        switch (get_field<IOHGATP_MODE>(ctx.gatp)) {
        case IOHGATP_BARE:
            break;
        case IOHGATP_SV39X4:
            if (!(caps & CAPS_SV39X4))
                return false;
            break;
        case IOHGATP_SV48X4:
            if (!(caps & CAPS_SV48X4))
                return false;
            break;
        case IOHGATP_SV57X4:
            if (!(caps & CAPS_SV57X4))
                return false;
            break;
        default:
            return false;
        }
    }

    if (get_field<IOHGATP_MODE>(ctx.gatp) != IOHGATP_BARE &&
        get_field<IOHGATP_PPN>(ctx.gatp) & 0xf) // gatp must be 16kB aligned
        return false;

    if (!(caps & CAPS_AMO_HWAD) && (ctx.tc & (TC_SADE | TC_GADE)))
        return false;
    if (!(caps & CAPS_END) && (!!(fctl & FCTL_BE) != !!(ctx.tc & TC_SBE)))
        return false;
    if ((fctl & FCTL_GXL) && !(ctx.tc & TC_SXL))
        return false;

    return true;
}

bool iommu::check_msi(const context& ctx, u64 addr) const {
    if (!msi_flat)
        return false;

    switch (get_field<MSI_MODE>(ctx.msiptp)) {
    case MSIPTE_MODE_FLAT:
        if (!msi_flat)
            return false;
        break;
    case MSIPTE_MODE_MRIF:
        if (!msi_mrif)
            return false;
        break;
    default:
        return false;
    }

    return !(((addr >> 12) ^ ctx.msi_addr_pattern) & ~ctx.msi_addr_mask);
}

int iommu::fetch_context(const tlm_sbi& info, bool dmi, context& ctx) {
    bool dbg = info.is_debug || dmi;
    bool ats = info.atype != SBI_ATYPE_UX;
    bool super = info.privilege > 0;

    u32 devid = info.cpuid;
    u32 pasid = sbi_get_pasid(info);

    ctx = {};
    ctx.device_id = devid;
    ctx.process_id = pasid;

    u64 mode = ddtp.get_field<DDTP_MODE>();
    if (mode == DDTP_MODE_OFF)
        return IOMMU_FAULT_DMA_DISABLED;
    if (mode == DDTP_MODE_BARE && ats)
        return IOMMU_FAULT_TTYPE_BLOCKED;

    u64 ctxid = mkctxid(devid, pasid);
    if (stl_contains(m_contexts, ctxid)) {
        ctx = m_contexts[ctxid];
        return 0;
    }

    size_t depth = 0;
    size_t ddidx[3] = { 0, 0, 0 };

    if (msi_flat) {
        ddidx[0] = extract(devid, 0, 6);
        ddidx[1] = extract(devid, 6, 9);
        ddidx[2] = extract(devid, 15, 9);
    } else {
        ddidx[0] = extract(devid, 0, 7);
        ddidx[1] = extract(devid, 7, 9);
        ddidx[2] = extract(devid, 16, 8);
    }

    switch (mode) {
    case DDTP_MODE_BARE:
        set_field<IOSATP_MODE>(ctx.satp, IOSATP_BARE);
        set_field<IOHGATP_MODE>(ctx.gatp, IOHGATP_BARE);
        ctx.tc = TC_V | TC_EN_ATS;
        return 0;
    case DDTP_MODE_1LVL:
        if (ddidx[2] || ddidx[1])
            return IOMMU_FAULT_TTYPE_BLOCKED;
        depth = 0;
        break;
    case DDTP_MODE_2LVL:
        if (ddidx[2])
            return IOMMU_FAULT_TTYPE_BLOCKED;
        depth = 1;
        break;
    case DDTP_MODE_3LVL:
        depth = 2;
        break;
    default:
        return IOMMU_FAULT_DDT_MISCONFIGURED;
    }

    u64 addr = ddtp.get_field<DDTP_PPN>() << PAGE_BITS;

    for (; depth > 0; depth--) {
        if (!dbg)
            increment_counter(ctx, IOMMU_EVENT_DD_WALK);

        u64 ddte = 0;
        u64 ddte_addr = addr + ddidx[depth] * sizeof(ddte);
        if (failed(dma_readw(ddte_addr, ddte, false, dbg)))
            return IOMMU_FAULT_DDT_LOAD_FAULT;

        if (!(ddte & DDTE_V) || (ddte & ~DDTE_MASK))
            return IOMMU_FAULT_DDT_INVALID;

        addr = get_field<DDTE_PPN>(ddte) << PAGE_BITS;
    }

    if (!dbg)
        increment_counter(ctx, IOMMU_EVENT_DD_WALK);

    u64 rawctx[8];
    memset(rawctx, 0, sizeof(rawctx));
    size_t ctxsz = (msi_flat ? 8 : 4) * sizeof(u64);
    u64 ctxaddr = addr + ddidx[0] * ctxsz;
    if (failed(dma_read(ctxaddr, rawctx, ctxsz, false, dbg)))
        return IOMMU_FAULT_DDT_LOAD_FAULT;

    ctx.tc = rawctx[0];
    ctx.gatp = rawctx[1];
    ctx.ta = rawctx[2];
    ctx.satp = rawctx[3];

    if (msi_flat) {
        ctx.msiptp = rawctx[4];
        ctx.msi_addr_mask = rawctx[5];
        ctx.msi_addr_pattern = rawctx[6];
    }

    if (!(ctx.tc & TC_V))
        return IOMMU_FAULT_DDT_INVALID;

    if (!check_context(ctx))
        return IOMMU_FAULT_DDT_MISCONFIGURED;

    if ((ctx.tc & TC_DPE) && (pasid == PASID_NONE))
        pasid = 0;

    if (!(ctx.tc & TC_PDTV) && (pasid != PASID_NONE))
        return IOMMU_FAULT_TTYPE_BLOCKED;

    if (ctx.tc & TC_PDTV) {
        mode = get_field<IOHGATP_MODE>(ctx.satp);
        addr = get_field<IOHGATP_PPN>(ctx.satp) << PAGE_BITS;

        size_t pdidx[3];
        pdidx[0] = extract(pasid, 0, 8);
        pdidx[1] = extract(pasid, 8, 9);
        pdidx[2] = extract(pasid, 17, 2);

        switch (mode) {
        case PDTP_BARE:
            return 0;
        case PDTP_PD8:
            if (pdidx[2] || pdidx[1])
                return IOMMU_FAULT_TTYPE_BLOCKED;
            depth = 0;
            break;
        case PDTP_PD17:
            if (pdidx[2])
                return IOMMU_FAULT_TTYPE_BLOCKED;
            depth = 1;
            break;
        case PDTP_PD20:
            depth = 2;
            break;
        default:
            return IOMMU_FAULT_PDT_MISCONFIGURED;
        }

        for (; depth > 0; depth--) {
            if (!dbg)
                increment_counter(ctx, IOMMU_EVENT_PD_WALK);

            u64 pdte = 0;
            u64 phys = 0;
            u64 virt = addr + pdidx[depth] * sizeof(pdte);
            if (int fault = translate_g(ctx, virt, false, false, dbg, phys))
                return fault;

            if (failed(dma_readw(phys, pdte, false, dbg)))
                return IOMMU_FAULT_PDT_LOAD_FAULT;

            if (!(pdte & PDTE_V) || (pdte & ~PDTE_MASK))
                return IOMMU_FAULT_PDT_INVALID;

            addr = get_field<DDTE_PPN>(pdte) << PAGE_BITS;
        }

        if (!dbg)
            increment_counter(ctx, IOMMU_EVENT_PD_WALK);

        ctxsz = 2 * sizeof(u64);
        memset(rawctx, 0, sizeof(rawctx));

        u64 phys = 0;
        u64 virt = addr + pdidx[0] * ctxsz;
        if (int fault = translate_g(ctx, virt, false, false, dbg, phys))
            return fault;

        if (failed(dma_read(phys, rawctx, ctxsz, false, dbg)))
            return IOMMU_FAULT_PDT_LOAD_FAULT;

        ctx.ta = rawctx[0];
        ctx.satp = rawctx[1];

        if (super && !(ctx.ta & TA_ENS))
            return IOMMU_FAULT_TTYPE_BLOCKED;
    }

    m_contexts[ctxid] = ctx;
    return 0;
}

int iommu::fetch_iotlb(context& ctx, tlm_generic_payload& tx,
                       const tlm_sbi& info, bool dmi, iotlb& entry) {
    bool wnr = tx.is_write();
    bool dbg = info.is_debug || dmi;
    bool pgreq = info.atype == SBI_ATYPE_RQ;
    bool txreq = info.atype == SBI_ATYPE_TX;
    bool super = info.privilege > 0;

    u64 virt = tx.get_address();
    u64 vpn = virt >> PAGE_BITS;

    u64 gscid = get_field<IOHGATP_GSCID>(ctx.gatp);
    u64 pscid = get_field<TA_PSCID>(ctx.ta);

    if (pgreq && !dbg) {
        if (!(ctx.tc & TC_EN_ATS))
            return IOMMU_FAULT_TTYPE_BLOCKED;
        if (!(ctx.tc & TC_EN_PRI))
            return IOMMU_FAULT_TTYPE_BLOCKED;
    }

    if (txreq && !dbg) {
        if (!(ctx.tc & TC_EN_ATS))
            return IOMMU_FAULT_TTYPE_BLOCKED;

        u64 phys = virt;
        if (check_msi(ctx, phys) && tx.get_data_length() > 0)
            return translate_msi(ctx, tx, info, phys, entry);
        if (ctx.tc & TC_T2GPA && translate_g(ctx, virt, wnr, false, dbg, phys))
            return iommu_page_fault(wnr);

        entry.vpn = vpn;
        entry.ppn = phys >> PAGE_BITS;
        entry.gscid = gscid;
        entry.pscid = pscid;
        entry.r = !wnr;
        entry.w = wnr;
        entry.pbmt = 0;
        return 0;
    }

    if (!pgreq && stl_contains(m_iotlb_s, vpn)) {
        iotlb& cache = m_iotlb_s[vpn];
        if (cache.gscid == gscid && cache.pscid == pscid &&
            (cache.w || !wnr)) {
            entry = cache;
            return 0;
        }
    }

    if (!dbg) {
        m_iotval2 = 0;
        increment_counter(ctx, IOMMU_EVENT_TLB_MISS);
    }

    iotlb iotlb_s;
    int fault = tablewalk(ctx, virt, false, super, wnr, false, dbg, iotlb_s);
    if (fault == TWALK_FAULT_G_STAGE)
        return IOMMU_PAGE_FAULT_R;
    if (fault != TWALK_FAULT_NONE)
        return iommu_guest_page_fault(wnr);

    if (pgreq && (ctx.tc & TC_T2GPA)) {
        entry = iotlb_s;
        return 0;
    }

    u64 gpa = (iotlb_s.ppn << PAGE_BITS) | (virt & PAGE_MASK);
    if (!pgreq && !dbg && check_msi(ctx, gpa))
        return translate_msi(ctx, tx, info, gpa, entry);

    if (stl_contains(m_iotlb_g, iotlb_s.ppn)) {
        iotlb& cache = m_iotlb_g[iotlb_s.ppn];
        if (cache.gscid == gscid && cache.pscid == pscid &&
            (cache.w || !wnr)) {
            entry = cache;
            return 0;
        }
    }

    iotlb iotlb_g;
    if (tablewalk(ctx, gpa, true, false, wnr, false, dbg, iotlb_g))
        return iommu_page_fault(wnr);

    if (!dbg)
        m_iotlb_g[iotlb_g.vpn] = iotlb_g;

    entry.vpn = iotlb_s.vpn;
    entry.ppn = iotlb_g.ppn;
    entry.r = iotlb_s.r && iotlb_g.r;
    entry.w = iotlb_s.w && iotlb_g.w;
    entry.gscid = gscid;
    entry.pscid = pscid;
    entry.pbmt = iotlb_s.pbmt | iotlb_g.pbmt;

    if (!dbg)
        m_iotlb_s[vpn] = entry;

    return 0;
}

iommu::vmcfg iommu::get_vm_config(const context& ctx, bool g) {
    vmcfg cfg;

    if (g) {
        cfg.root = get_field<IOHGATP_PPN>(ctx.gatp) << PAGE_BITS;
        cfg.adue = ctx.tc & TC_GADE;
        cfg.sum = false;
        if (ctx.tc & TC_SXL) {
            cfg.ptesize = 4;
            cfg.pbmt = false;
            switch (get_field<IOHGATP_MODE>(ctx.gatp)) {
            case IOHGATP_SV32X4:
                cfg.levels = 2;
                cfg.vpnbits = 10;
                break;
            case IOHGATP_BARE:
            default:
                cfg.levels = 0;
                cfg.vpnbits = 0;
                break;
            }
        } else {
            cfg.ptesize = 8;
            cfg.pbmt = svpbmt;
            switch (get_field<IOHGATP_MODE>(ctx.gatp)) {
            case IOHGATP_SV39X4:
                cfg.levels = 3;
                cfg.vpnbits = 9;
                break;
            case IOHGATP_SV48X4:
                cfg.levels = 4;
                cfg.vpnbits = 9;
                break;
            case IOHGATP_SV57X4:
                cfg.levels = 5;
                cfg.vpnbits = 9;
                break;
            case IOHGATP_BARE:
            default:
                cfg.levels = 0;
                cfg.vpnbits = 0;
                break;
            }
        }
    } else {
        cfg.root = get_field<IOSATP_PPN>(ctx.satp) << PAGE_BITS;
        cfg.adue = ctx.tc & TC_SADE;
        cfg.sum = (ctx.tc & TC_PDTV) && (ctx.ta & TA_SUM);
        if (ctx.tc & TC_SXL) {
            cfg.ptesize = 4;
            cfg.pbmt = false;
            switch (get_field<IOSATP_MODE>(ctx.satp)) {
            case IOSATP_SV32:
                cfg.levels = 2;
                cfg.vpnbits = 10;
                break;
            case IOSATP_BARE:
            default:
                cfg.levels = 0;
                cfg.vpnbits = 0;
                break;
            }
        } else {
            cfg.ptesize = 8;
            cfg.pbmt = svpbmt;
            switch (get_field<IOSATP_MODE>(ctx.satp)) {
            case IOSATP_SV39:
                cfg.levels = 3;
                cfg.vpnbits = 9;
                break;
            case IOSATP_SV48:
                cfg.levels = 4;
                cfg.vpnbits = 9;
                break;
            case IOSATP_SV57:
                cfg.levels = 5;
                cfg.vpnbits = 9;
                break;
            case IOSATP_BARE:
            default:
                cfg.levels = 0;
                cfg.vpnbits = 0;
                break;
            }
        }
    }

    return cfg;
}

int iommu::tablewalk(context& ctx, u64 virt, bool g, bool super, bool wnr,
                     bool ind, bool dbg, iotlb& entry) {
    auto vm = get_vm_config(ctx, g);

    if (vm.levels == 0) {
        entry.vpn = virt >> PAGE_BITS;
        entry.ppn = virt >> PAGE_BITS;
        entry.r = true;
        entry.w = true;
        entry.pbmt = 0;
        return 0;
    }

    if (g) {
        m_iotval2 = virt & ~3ull;
        if (ind)
            m_iotval2 |= IOTVAL2_INDIRECT | (wnr ? IOTVAL2_WRITE : 0);
    }

retry:
    u64 pgbase = vm.root;

    for (size_t i = 0; i < vm.levels; i++) {
        increment_counter(ctx, g ? IOMMU_EVENT_GS_WALK : IOMMU_EVENT_VS_WALK);

        u64 pgshift = (vm.levels - i - 1) * vm.vpnbits;
        u64 vpnmask = bitmask(vm.vpnbits);
        u64 ppnmask = bitmask(pgshift);

        u64 pte = 0;
        u64 idx = (virt >> (pgshift + PAGE_BITS)) & vpnmask;
        u64 pteaddr = pgbase + idx * vm.ptesize;

        if (!g && translate_g(ctx, pteaddr, vm.adue, true, dbg, pteaddr))
            return TWALK_FAULT_G_STAGE;

        if (failed(dma_read(pteaddr, &pte, vm.ptesize, vm.adue, dbg)))
            return TWALK_FAULT_PTE_FETCH;

        if (!(pte & PTE_V))
            return TWALK_FAULT_PTE_INVALID;

        u64 ppn = get_field<PTE_PPN>(pte);
        u64 rwx = pte & PTE_RWX;
        if (rwx == 0) {
            pgbase = ppn << PAGE_BITS;
            continue;
        }

        if (rwx == PTE_W || rwx == (PTE_W | PTE_X))
            return TWALK_FAULT_PTE_CORRUPTED;

        if ((g || !super) && !(pte & PTE_U) && !dbg)
            return TWALK_FAULT_PTE_PROT_U;
        if ((super && !g) && (pte & PTE_U) && !vm.sum && !dbg)
            return TWALK_FAULT_PTE_PROT_S;

        if (ppn & ppnmask)
            return TWALK_FAULT_PPN_MISALIGNED;

        if (!dbg) {
            u64 newpte = pte | PTE_A | (wnr ? PTE_D : 0);
            if (newpte != pte) {
                if (!vm.adue)
                    return TWALK_FAULT_AD_UPDATE;

                bool atomic = false;
                if (failed(dma_write(pteaddr, &newpte, vm.ptesize, &atomic,
                                     false))) {
                    return TWALK_FAULT_G_STAGE;
                }

                if (!atomic)
                    goto retry;
            }
        }

        entry.vpn = virt >> PAGE_BITS;
        entry.ppn = get_field<PTE_PPN>(pte);
        entry.r = !!(pte & PTE_R);
        entry.w = !!(pte & PTE_W);
        entry.pbmt = 0;

        if (svpbmt)
            entry.pbmt = get_field<PTE_PBMT>(pte);

        return 0;
    }

    return TWALK_FAULT_PTE_CORRUPTED;
}

int iommu::translate_g(context& ctx, u64 virt, bool wnr, bool ind, bool dbg,
                       u64& phys) {
    if (get_field<IOHGATP_MODE>(ctx.gatp) == IOHGATP_BARE) {
        phys = virt;
        return 0;
    }

    iotlb entry;
    if (tablewalk(ctx, virt & ~PAGE_MASK, true, false, wnr, ind, dbg, entry))
        return iommu_page_fault(wnr);

    if (wnr && !entry.w)
        return IOMMU_PAGE_FAULT_W;
    if (!wnr && !entry.r)
        return IOMMU_PAGE_FAULT_R;

    phys = (entry.ppn << PAGE_BITS) | (virt & PAGE_MASK);
    return 0;
}

int iommu::translate_msi(context& ctx, tlm_generic_payload& tx,
                         const tlm_sbi& info, u64 gpa, iotlb& entry) {
    if (!check_msi(ctx, gpa))
        return IOMMU_FAULT_MSI_MISCONFIGURED;

    u64 msipte[2];
    u64 irqid = msi_extract(gpa >> 12, ctx.msi_addr_mask);
    u64 base = get_field<MSI_PPN>(ctx.msiptp) << PAGE_BITS;

    u64 addr = base + irqid * sizeof(msipte);
    if (failed(dma_read(addr, msipte, sizeof(msipte), false, info.is_debug)))
        return IOMMU_FAULT_MSI_LOAD_FAULT;

    if (!(msipte[0] & MSIPTE_V))
        return IOMMU_FAULT_MSI_INVALID;

    if (msipte[0] & MSIPTE_C)
        return IOMMU_FAULT_MSI_MISCONFIGURED;

    switch (get_field<MSIPTE_M>(msipte[0])) {
    case MSIPTE_MODE_FLAT: {
        entry.vpn = tx.get_address() >> PAGE_BITS;
        entry.ppn = get_field<MSIPTE_PPN>(msipte[0]);
        entry.gscid = get_field<IOHGATP_GSCID>(ctx.gatp);
        entry.pscid = get_field<TA_PSCID>(ctx.ta);
        entry.pbmt = 0;
        entry.r = true;
        entry.w = true;
        return 0;
    }

    case MSIPTE_MODE_MRIF:
        return transmit_mrif(ctx, tx, info, msipte);

    default:
        return IOMMU_FAULT_MSI_MISCONFIGURED;
    }
}

int iommu::transmit_mrif(context& ctx, tlm_generic_payload& tx,
                         const tlm_sbi& info, u64 msipte[2]) {
    if (!tx.is_write() || tx.get_address() & 3 || tx.get_data_length() != 4) {
        tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return IOMMU_FAULT_MRIF_ACCESS;
    }

    u32 msi = *(u32*)tx.get_data_ptr() & bitmask(11);
    u64 base = get_field<MSIPTE_MRIF>(msipte[0]) << 9;
    u64 naddr = get_field<MSIPTE_NPPN>(msipte[1]) << 12;
    u32 nid = msipte_nid(msipte[1]);

    u64 msi_idx = msi / 64;
    u64 msi_off = msi % 64;

    u64 mask = bit(msi_off);
    u64 addr = base + msi_idx * 16;

    bool excl;
    tlm_response_status rs;

    do {
        excl = amo_mrif;
        u64 pending = 0;
        if (failed(rs = dma_readw(addr, pending, excl, false))) {
            tx.set_response_status(rs);
            log_debug("failed to read mrif pending at 0x%llx", addr);
            return IOMMU_FAULT_MRIF_ACCESS;
        }

        pending |= mask;

        if (failed(rs = dma_writex(addr, pending, excl, false))) {
            tx.set_response_status(rs);
            log_debug("failed to write mrif pending at 0x%llx", addr);
            return IOMMU_FAULT_MRIF_ACCESS;
        }
    } while (amo_mrif && !excl);

    u64 enabled = 0;
    addr = addr + 8;

    if (failed(rs = dma_readw(addr, enabled, false, false))) {
        tx.set_response_status(rs);
        log_debug("failed to read mrif enabled at 0x%llx", addr);
        return IOMMU_FAULT_MRIF_ACCESS;
    }

    if (enabled & mask) {
        if (failed(rs = dma_writew(naddr, nid, false))) {
            tx.set_response_status(rs);
            log_debug("failed to write mrif notify at 0x%llx", naddr);
            return IOMMU_FAULT_MRIF_ACCESS;
        }
    }

    // mark transaction completed: no forwarding
    tx.set_response_status(TLM_OK_RESPONSE);
    return 0;
}

bool iommu::translate(tlm_generic_payload& tx, const tlm_sbi& info, bool dmi,
                      iotlb& entry) {
    bool dbg = info.is_debug;
    bool txr = info.atype == SBI_ATYPE_TX; // pretranslated address
    bool ats = info.atype == SBI_ATYPE_RQ; // address translation request
    bool super = info.privilege > 0;

    u32 devid = info.cpuid;
    u32 pasid = sbi_get_pasid(info);

    context ctx;
    int err;

    err = fetch_context(info, dmi, ctx);
    if (err) {
        if (!dbg && !dmi && !(ctx.tc & TC_DTF)) {
            fault req{};
            req.cause = err;
            req.ttyp = ttyp_from_tx(tx, info, txr, ats);
            req.pv = pasid != PASID_NONE;
            req.pid = req.pv ? pasid : 0;
            req.did = devid;
            req.priv = super && req.pv;
            req.iotval = tx.get_address();
            req.iotval2 = m_iotval2;
            report_fault(req);
        }

        return false;
    }

    if (!dbg && !dmi)
        increment_counter(ctx, iommu_event_req(txr));

    err = fetch_iotlb(ctx, tx, info, dmi, entry);
    if (ats) {
        if (err) {
            pgreq req{};
            req.pv = pasid != PASID_NONE;
            req.pid = req.pv ? pasid : 0;
            req.did = devid;
            req.priv = super && req.pv;
            req.exec = info.is_insn;
            req.r = tx.is_read();
            req.w = tx.is_write();
            req.pgaddr = tx.get_address() & PAGE_MASK;
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            report_pgreq(req);
            return 0;
        }

        tx.set_response_status(TLM_OK_RESPONSE);
        return true;
    }

    if (err) {
        if (!dbg && !dmi && !(ctx.tc & TC_DTF)) {
            fault req{};
            req.cause = err;
            req.ttyp = ttyp_from_tx(tx, info, txr, ats);
            req.pv = pasid != PASID_NONE;
            req.pid = req.pv ? pasid : 0;
            req.did = devid;
            req.priv = super && req.pv;
            req.iotval = tx.get_address();
            req.iotval2 = m_iotval2;
            report_fault(req);
        }

        return false;
    }

    return true;
}

void iommu::restart_counter(u64 val) {
    m_counter_val = val;
    m_counter_start = sc_time_stamp();
    m_counter_ovev.cancel();
    m_counter_ovev.notify(clock_cycles(mwr::U64_MAX - val));
}

void iommu::increment_counter(context& ctx, u32 event) {
    for (int i = 1; i < 32; i++) {
        u32 cntmask = bit(i);
        if (iocntinh & cntmask)
            continue;

        u64 hpmevt = iohpmevt[i - 1];
        if (event != get_field<IOHPMEVT_EVENTID>(hpmevt))
            continue;

        u32 did_gscid = get_field<IOHPMEVT_DID_GSCID>(hpmevt);
        u32 pid_pscid = get_field<IOHPMEVT_PID_PSCID>(hpmevt);

        u32 did = ctx.device_id;
        u32 pid = ctx.process_id;

        if (hpmevt & IOHPMEVT_IDT) {
            did = get_field<IOHGATP_GSCID>(ctx.gatp);
            pid = get_field<TA_PSCID>(ctx.ta);
        }

        if (hpmevt & IOHPMEVT_DV_GSCV) {
            u32 mask = ~0u;
            if (hpmevt & IOHPMEVT_DMASK)
                mask = ~(mask ^ (mask + 1));

            if ((did & mask) != (did_gscid & mask))
                continue;
        }

        if (hpmevt & IOHPMEVT_PV_PSCV) {
            if (pid != pid_pscid)
                continue;
        }

        iohpmctr[i - 1]++;

        if (iohpmctr[i - 1] == 0) { // overflow
            iohpmevt[i - 1] |= IOHPMEVT_OF;
            iocntovf |= cntmask;
            update_ipsr(IPSR_PIP, 0);
        }
    }
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

u64 iommu::read_iohpmcycles() {
    u64 ovf = m_counter_val & COUNTER_OV;
    u64 val = m_counter_val & ~COUNTER_OV;

    if (!(iocntinh & COUNTER_INH)) {
        sc_time delta = sc_time_stamp() - m_counter_start;
        val += delta.to_seconds() * clock_hz();
    }

    return val | ovf;
}

void iommu::write_fctl(u32 val) {
    u32 mask = FCTL_GXL;
    if (!cirq.is_stubbed() && !firq.is_stubbed())
        mask |= FCTL_WSI;
    fctl = (fctl & ~mask) | (val & mask);
}

void iommu::write_ddtp(u64 val) {
    u64 mask = DDTP_MODE::MASK | DDTP_PPN::MASK;

    if (((ddtp ^ val) & mask) == 0)
        return; // no change

    if (ddtp & DDTP_BUSY)
        return;

    m_contexts.clear();
    m_iotlb_s.clear();

    ddtp = (ddtp & ~mask) | (val & mask);
}

void iommu::write_cqt(u32 val) {
    if (cqcsr & CQCSR_BUSY)
        return;

    u32 mask = (2u << cqb.get_field<CQB_LOG2SZ>()) - 1;
    cqt = val & mask;

    if (cqcsr & CQCSR_CQEN) {
        m_work |= IOMMU_WORK_COMMAND;
        m_workev.notify(SC_ZERO_TIME);
    }
}

void iommu::write_fqh(u32 val) {
    if (fqcsr & FQCSR_BUSY)
        return;

    u32 mask = (2u << fqb.get_field<FQB_LOG2SZ>()) - 1;
    fqh = val & mask;
}

void iommu::write_pqh(u32 val) {
    if (pqcsr & PQCSR_BUSY)
        return;

    u32 mask = (2u << fqb.get_field<PQB_LOG2SZ>()) - 1;
    pqh = val & mask;
}

void iommu::write_cqcsr(u32 val) {
    u32 rwmask = CQCSR_CQEN | CQCSR_CIE;
    u32 wcmask = CQCSR_CQMF | CQCSR_CMDTO | CQCSR_CMDILL | CQCSR_FENCE_W_IP;

    if (cqcsr & CQCSR_BUSY)
        return;

    if ((cqcsr ^ val) & CQCSR_CQEN) {
        m_work |= IOMMU_WORK_COMMAND;
        m_workev.notify(SC_ZERO_TIME);

        if (val & CQCSR_CQEN) {
            cqh = 0;
            cqcsr &= ~wcmask;
            cqcsr |= CQCSR_BUSY;
        }
    }

    cqcsr = ((cqcsr & ~rwmask) | (val & rwmask)) & ~(val & wcmask);
}

void iommu::write_fqcsr(u32 val) {
    u32 rwmask = FQCSR_FQEN | FQCSR_FIE;
    u32 wcmask = FQCSR_FQMF | FQCSR_FQOF;

    if (fqcsr & FQCSR_BUSY)
        return;

    if ((fqcsr ^ val) & FQCSR_FQEN) {
        m_work |= IOMMU_WORK_FAULT;
        m_workev.notify(SC_ZERO_TIME);

        if (val & FQCSR_FQEN) {
            fqt = 0;
            fqcsr &= ~wcmask;
            fqcsr |= FQCSR_BUSY;
        }
    }

    fqcsr = ((fqcsr & ~rwmask) | (val & rwmask)) & ~(val & wcmask);
}

void iommu::write_pqcsr(u32 val) {
    u32 rwmask = PQCSR_PQEN | PQCSR_PIE;
    u32 wcmask = PQCSR_PQMF | PQCSR_PQOF;

    if (pqcsr & PQCSR_BUSY)
        return;

    if ((pqcsr ^ val) & PQCSR_PQEN) {
        m_work |= IOMMU_WORK_PGREQ;
        m_workev.notify(SC_ZERO_TIME);

        if (val & PQCSR_PQEN) {
            pqt = 0;
            pqcsr &= ~wcmask;
            pqcsr |= PQCSR_BUSY;
        }
    }

    pqcsr = ((pqcsr & ~rwmask) | (val & rwmask)) & ~(val & wcmask);
}

void iommu::write_ipsr(u32 val) {
    u32 rw1c = val & (IPSR_CIP | IPSR_FIP | IPSR_PMIP | IPSR_PIP);
    update_ipsr(0, rw1c);
}

void iommu::write_iocntinh(u32 val) {
    // re-starting counter
    if ((iocntinh & COUNTER_INH) && !(val & COUNTER_INH))
        restart_counter(m_counter_val);

    // pausing counter
    if (!(iocntinh & COUNTER_INH) && (val & COUNTER_INH)) {
        m_counter_val = read_iohpmcycles();
        m_counter_ovev.cancel();
    }

    iocntinh = val;
}

void iommu::write_iohpmcycles(u64 val) {
    if (iocntinh & COUNTER_INH)
        m_counter_val = val;
    else
        restart_counter(val);

    if (!(val & COUNTER_OV))
        iocntovf &= ~COUNTER_OVF;
}

void iommu::write_iohpmevt(u64 val, size_t idx) {
    if (get_field<IOHPMEVT_EVENTID>(val) >= IOMMU_EVENT_MAX)
        set_field<IOHPMEVT_EVENTID>(val, IOMMU_EVENT_NONE);

    iohpmevt[idx] = val;

    if (!(val & IOHPMEVT_OF))
        iocntovf &= ~bit(idx + 1);
}

void iommu::write_tr_req_iova(u64 val) {
    if (tr_req_ctl & TR_REQ_CTL_BUSY)
        return;

    tr_req_iova = val & ~PAGE_MASK;
}

void iommu::write_tr_req_ctl(u64 val) {
    if (tr_req_ctl & TR_REQ_CTL_BUSY)
        return;

    tr_req_ctl = (tr_req_ctl & ~TR_REQ_CTL_MASK) | (val & TR_REQ_CTL_MASK);

    if (val & TR_REQ_CTL_BUSY) {
        m_work |= IOMMU_WORK_TRREQ;
        m_workev.notify(SC_ZERO_TIME);
    }
}

template <typename MAP>
inline void invalidate_all(MAP& map) {
    map.clear();
}

template <typename MAP, typename KEY>
inline void invalidate_key(MAP& map, const KEY& key) {
    map.erase(key);
}

template <typename MAP, typename PRED>
inline void invalidate_some(MAP& map, PRED&& pred) {
    for (auto it = map.begin(); it != map.end();) {
        if (pred(it->second))
            it = map.erase(it);
        else
            it++;
    }
}

void iommu::handle_iotinval(const command& cmd) {
    bool inval_s = false;
    bool inval_g = false;

    switch (cmd.func3) {
    case IOTINVAL_GVMA:
        inval_g = true;
        // fallthrough
    case IOTINVAL_VMA:
        inval_s = true;
        break;
    default:
        update_cqcsr(CQCSR_CMDILL);
        return;
    }

    bool av = cmd.operands0 & IOTINVAL_AV;
    bool gv = cmd.operands0 & IOTINVAL_GV;
    bool pscv = cmd.operands0 & IOTINVAL_PSCV;

    u64 vpn = cmd.operands1 >> 10;
    u64 pscid = get_field<IOTINVAL_PSCID>(cmd.operands0);
    u64 gscid = get_field<IOTINVAL_GSCID>(cmd.operands0);

    if (!gv && !pscv) {
        if (av) {
            if (inval_s)
                invalidate_key(m_iotlb_s, vpn);
            if (inval_g)
                invalidate_key(m_iotlb_g, vpn);
        } else {
            if (inval_s)
                invalidate_all(m_iotlb_s);
            if (inval_g)
                invalidate_all(m_iotlb_g);
        }
    } else {
        auto filter = [=](const iotlb& entry) -> bool {
            if (av && entry.vpn != vpn)
                return false;
            if (pscv && entry.pscid != pscid)
                return false;
            if (gv && entry.gscid != gscid)
                return false;
            return true;
        };

        if (inval_s)
            invalidate_some(m_iotlb_s, filter);
        if (inval_g)
            invalidate_some(m_iotlb_g, filter);
    }
}

void iommu::handle_iofence(const command& cmd) {
    if (cmd.func3 != IOFENCE_C) {
        update_cqcsr(CQCSR_CMDILL);
        return;
    }

    if (cmd.operands0 & IOFENCE_AV) {
        u32 data = get_field<IOFENCE_DATA>(cmd.operands0);
        u64 addr = cmd.operands1 << 2;
        if (failed(out.writew(addr, data))) {
            update_cqcsr(CQCSR_CQMF);
            return;
        }
    }

    if ((cmd.operands0 & IOFENCE_WSI) && (fctl & FCTL_WSI))
        update_cqcsr(CQCSR_FENCE_W_IP);
}

void iommu::handle_iodir(const command& cmd) {
    bool dv = cmd.operands0 & IODIR_DV;
    u64 pid = get_field<IODIR_PID>(cmd.operands0);
    u64 did = get_field<IODIR_DID>(cmd.operands0);

    if (cmd.operands1 > 0) {
        update_cqcsr(CQCSR_CMDILL);
        return;
    }

    switch (cmd.func3) {
    case IODIR_PDT: {
        if (!dv) {
            update_cqcsr(CQCSR_CMDILL);
            break;
        }

        invalidate_some(m_contexts, [did, pid](const context& ctx) {
            u64 gscid = get_field<IOHGATP_GSCID>(ctx.gatp);
            u64 pscid = get_field<TA_PSCID>(ctx.ta);
            return gscid == did && pscid == pid;
        });

        break;
    }

    case IODIR_DDT: {
        if (!dv) {
            invalidate_all(m_contexts);
        } else {
            invalidate_some(m_contexts, [did](const context& ctx) {
                u64 gscid = get_field<IOHGATP_GSCID>(ctx.gatp);
                return gscid == did;
            });
        }

        break;
    }

    default:
        update_cqcsr(CQCSR_CMDILL);
        break;
    }
}

void iommu::handle_ats(const command& cmd) {
    bool pv = cmd.operands0 & ATS_PV;
    bool dsv = cmd.operands0 & ATS_DSV;
    u32 rid = get_field<ATS_RID>(cmd.operands0);
    u32 pid = pv ? get_field<ATS_PID>(cmd.operands0) : 0;
    u32 dseg = dsv ? get_field<ATS_DSEG>(cmd.operands0) : 0;

    switch (cmd.func3) {
    case ATS_INVAL: {
        u64 page = cmd.operands1 & ~PAGE_MASK;
        bool g = cmd.operands1 & bit(0);
        bool s = cmd.operands1 & bit(11);
        log_debug("received ATS invalidation request");
        log_debug("  devid: %u", rid);
        if (pv)
            log_debug("  pasid: %u", pid);
        log_debug("  page:  0x%llx", page);
        log_debug("  flags: %s%s", s ? "s" : "", g ? "g" : "");
        dma.dmi_cache().invalidate(page, page + PAGE_MASK);
        dma->invalidate_direct_mem_ptr(page, page + PAGE_MASK);
        break;
    }

    case ATS_PRGR: {
        u32 prgi = extract(cmd.operands1, 32, 8);
        u32 code = extract(cmd.operands1, 44, 4);
        u32 dest = extract(cmd.operands1, 48, 16);
        log_debug("received ATS page request group response:");
        log_debug("  devid: %u", rid);
        if (pv)
            log_debug("  pasid: %u", pid);
        log_debug("  group: %u", prgi);
        log_debug("  dest:  0x%x", dest);
        if (dsv)
            log_debug("  seg:   0x%x", dseg);
        log_debug("  code:  0x%x", code);
        break;
    }

    default:
        update_cqcsr(CQCSR_CMDILL);
        break;
    }
}

void iommu::handle_command() {
    if (cqcsr & CQCSR_CQEN)
        cqcsr |= CQCSR_CQON;

    if (cqcsr & CQCSR_CMDILL)
        return;

    u32 mask = (2u << cqb.get_field<CQB_LOG2SZ>()) - 1;
    u64 base = cqb.get_field<CQB_PPN>() << PAGE_BITS;

    cqcsr |= CQCSR_BUSY;

    while ((cqh != cqt) && (cqcsr & CQCSR_CQEN)) {
        command cmd{};
        u64 addr = base + cqh * sizeof(command);
        if (failed(dma_readw(addr, cmd, false, false))) {
            log_debug("command queue memory error");
            update_cqcsr(CQCSR_CQMF);
            break;
        }

        switch (cmd.opcode) {
        case IOMMU_OPCODE_IOTINVAL:
            handle_iotinval(cmd);
            break;
        case IOMMU_OPCODE_IOFENCE:
            handle_iofence(cmd);
            break;
        case IOMMU_OPCODE_IOTDIR:
            handle_iodir(cmd);
            break;
        case IOMMU_OPCODE_ATS:
            handle_ats(cmd);
            break;
        default:
            update_cqcsr(CQCSR_CMDILL);
            break;
        }

        if (cqcsr & CQCSR_CMDILL) {
            log_debug("command queue illegal opcode: 0x%02x:0x%01x",
                      (int)cmd.opcode, (int)cmd.func3);
            break;
        }

        cqh = (cqh + 1) & mask;
    }

    cqcsr &= ~CQCSR_BUSY;

    if (!(cqcsr & CQCSR_CQEN))
        cqcsr &= ~CQCSR_CQON;
}

void iommu::handle_fault() {
    if (fqcsr & FQCSR_FQEN)
        fqcsr |= FQCSR_FQON;

    u32 mask = (2u << fqb.get_field<FQB_LOG2SZ>()) - 1;
    u64 base = fqb.get_field<FQB_PPN>() << PAGE_BITS;

    fqcsr |= FQCSR_BUSY;

    size_t nwritten = 0;
    while (!m_faults.empty() && (fqcsr & FQCSR_FQEN)) {
        fault req = m_faults.front();
        m_faults.pop();

        if (fqt == ((fqh - 1) & mask)) {
            log_debug("fault queue overflow");
            fqcsr |= FQCSR_FQOF;
            if (fqcsr & FQCSR_FIE)
                update_ipsr(IPSR_FIP, 0);
            break;
        }

        u64 addr = base + fqt * sizeof(fault);
        if (failed(dma_writew(addr, req, false))) {
            log_debug("fault queue memory error");
            fqcsr |= FQCSR_FQMF;
            if (fqcsr & FQCSR_FIE)
                update_ipsr(IPSR_FIP, 0);
            break;
        }

        fqt = (fqt + 1) & mask;
        nwritten++;
    }

    if (nwritten > 0 && fqcsr & FQCSR_FIE)
        update_ipsr(IPSR_FIP, 0);

    // drop all remaining faults
    while (!m_faults.empty())
        m_faults.pop();

    fqcsr &= ~FQCSR_BUSY;

    if (!(fqcsr & FQCSR_FQEN))
        fqcsr &= ~FQCSR_FQON;
}

void iommu::handle_pgreq() {
    if (pqcsr & PQCSR_PQEN)
        pqcsr |= PQCSR_PQON;

    u32 mask = (2u << pqb.get_field<PQB_LOG2SZ>()) - 1;
    u64 base = pqb.get_field<PQB_PPN>() << PAGE_BITS;

    pqcsr |= PQCSR_BUSY;

    size_t nwritten = 0;
    while (!m_pgreqs.empty() && (pqcsr & PQCSR_PQEN)) {
        pgreq req = m_pgreqs.front();
        m_pgreqs.pop();

        if (pqt == ((pqh - 1) & mask)) {
            log_debug("page request queue overflow");
            pqcsr |= PQCSR_PQOF;
            if (pqcsr & PQCSR_PIE)
                update_ipsr(IPSR_PIP, 0);
            break;
        }

        u64 addr = base + pqt * sizeof(pgreq);
        if (failed(dma_writew(addr, req, false))) {
            log_debug("page queue memory error");
            pqcsr |= PQCSR_PQMF;
            if (pqcsr & PQCSR_PIE)
                update_ipsr(IPSR_PIP, 0);
            break;
        }

        pqt = (pqt + 1) & mask;
        nwritten++;
    }

    if (nwritten > 0 && pqcsr & PQCSR_PIE)
        update_ipsr(IPSR_PIP, 0);

    // drop all remaining requests
    while (!m_pgreqs.empty())
        m_pgreqs.pop();

    pqcsr &= ~PQCSR_BUSY;

    if (!(pqcsr & PQCSR_PQEN))
        pqcsr &= ~PQCSR_PQON;
}

void iommu::handle_trreq() {
    tlm_generic_payload tx;
    tx.set_address(tr_req_iova);
    tx.set_data_length(0);
    if (tr_req_ctl & TR_REQ_CTL_NW)
        tx.set_read();
    else
        tx.set_write();

    tlm_sbi info = SBI_NONE;
    info.privilege = !!(tr_req_ctl & TR_REQ_CTL_PRIV);
    info.is_insn = !!(tr_req_ctl & TR_REQ_CTL_EXE);
    info.cpuid = tr_req_ctl.get_field<TR_REQ_CTL_DID>();
    if (tr_req_ctl & TR_REQ_CTL_PV)
        info.asid = tr_req_ctl.get_field<TR_REQ_CTL_PID>();

    iotlb entry{};
    if (translate(tx, info, false, entry)) {
        tr_response = 0;
        tr_response.set_field<TR_RESPONSE_PPN>(entry.ppn);
        if (svpbmt)
            tr_response.set_field<TR_RESPONSE_PBMT>(entry.pbmt);
    } else {
        tr_response = TR_RESPONSE_FAULT;
    }

    tr_req_ctl &= ~TR_REQ_CTL_BUSY;
}

void iommu::worker() {
    while (true) {
        wait(m_workev);

        u32 work = m_work;
        u32 mask = 1;
        m_work = 0;

        while (work) {
            switch (work & mask) {
            case IOMMU_WORK_COMMAND:
                handle_command();
                break;
            case IOMMU_WORK_FAULT:
                handle_fault();
                break;
            case IOMMU_WORK_PGREQ:
                handle_pgreq();
                break;
            case IOMMU_WORK_TRREQ:
                handle_trreq();
                break;
            default:
                break;
            }

            work = work & ~mask;
            mask = mask << 1;
        }
    }
}

void iommu::overflow() {
    m_counter_val |= COUNTER_OV;
    iocntovf |= COUNTER_OVF;
    update_ipsr(IPSR_PIP, 0);
}

void iommu::update_cqcsr(u32 setmask) {
    setmask &= CQCSR_PENDING;
    if (setmask == 0)
        return;

    cqcsr |= setmask;
    if (cqcsr & CQCSR_CIE)
        update_ipsr(IPSR_CIP, 0);
}

void iommu::update_ipsr(u32 setmask, u32 clrmask) {
    u32 oldipsr = ipsr;

    ipsr &= ~clrmask;
    ipsr |= setmask;

    bool wsi = fctl & FCTL_WSI;

    cirq = wsi && (ipsr & IPSR_CIP);
    firq = wsi && (ipsr & IPSR_FIP);
    pmirq = wsi && (ipsr & IPSR_PMIP);
    pirq = wsi && (ipsr & IPSR_PIP);

    if (!wsi && !(oldipsr & IPSR_CIP) && (ipsr & IPSR_CIP))
        send_msi(MSI_CQ);
    if (!wsi && !(oldipsr & IPSR_FIP) && (ipsr & IPSR_FIP))
        send_msi(MSI_FQ);
    if (!wsi && !(oldipsr & IPSR_PMIP) && (ipsr & IPSR_PMIP))
        send_msi(MSI_PM);
    if (!wsi && !(oldipsr & IPSR_PIP) && (ipsr & IPSR_PIP))
        send_msi(MSI_PQ);
}

void iommu::report_fault(const fault& req) {
    m_faults.push(req);
    m_work |= IOMMU_WORK_FAULT;
    m_workev.notify(SC_ZERO_TIME);

    log_debug("--- iommu fault ---");
    log_debug("  cause:      %s", iommu_fault_str(req.cause));
    log_debug("  ttyp:       %s", iommu_ttyp_str(req.ttyp));
    log_debug("  device_id:  %u", (int)req.did);

    if (req.pv) {
        log_debug("  process_id: %u", (int)req.pid);
        log_debug("  privilege:  %s", req.priv ? "S" : "U");
    }

    log_debug("  iotval:     0x%llx", req.iotval);
    log_debug("  iotval2:    0x%llx", req.iotval2);
    log_debug("--- [fault end] ---");
}

void iommu::report_pgreq(const pgreq& req) {
    m_pgreqs.push(req);
    m_work |= IOMMU_WORK_PGREQ;
    m_workev.notify(SC_ZERO_TIME);

    log_debug("--- iommu page request ---");
    log_debug("  device_id:  %u", (int)req.did);

    if (req.pv) {
        log_debug("  process_id: %u", (int)req.pid);
        log_debug("  privilege:  %s", req.priv ? "S" : "U");
    }

    log_debug("  page:       0x%llx", req.pgaddr);
    log_debug("  prgi:       %u", (int)req.prgidx);
    log_debug("  prot:       %s%s%s", req.r ? "r" : "", req.w ? "w" : "",
              req.exec ? "x" : "");
    log_debug("--- [page request end] ---");
}

void iommu::send_msi(u32 irq) {
    u64 vector = extract(icvec.get(), irq * 4, 4) + 1;
    u64 addr = msi_cfg_tbl[2 * vector] & MSI_VECTOR_ADDR;
    u32 ctrl = get_field<MSI_VECTOR_CTRL>(msi_cfg_tbl[2 * vector + 1]);
    u32 data = get_field<MSI_VECTOR_DATA>(msi_cfg_tbl[2 * vector + 1]);

    if (ctrl & MSI_VECTOR_CTRL_M)
        return;

    if (failed(out.writew(addr, data))) {
        fault req{};
        req.cause = IOMMU_FAULT_MSI_WR_FAULT;
        req.ttyp = IOMMU_TTYP_NONE;
        req.did = 0;
        req.pid = 0;
        req.pv = 0;
        req.priv = 0;
        req.iotval = addr;
        req.iotval2 = 0;
        report_fault(req);
    }
}

iommu::iommu(const sc_module_name& nm):
    peripheral(nm),
    m_contexts(),
    m_iotlb_s(),
    m_iotlb_g(),
    m_workev("workev"),
    m_faults(),
    m_pgreqs(),
    m_iotval2(0),
    m_dmi_lo(~0ull),
    m_dmi_hi(0ull),
    m_dma_addr(~0ull),
    m_dma_xval(),
    m_dma_xptr(),
    m_counter_val(0ull),
    m_counter_start(),
    m_counter_ovev("counter_ovev"),
    sv32("sv32", true),
    sv39("sv39", true),
    sv48("sv48", true),
    sv57("sv57", true),
    svpbmt("svpbmt", true),
    sv32x4("sv32x4", true),
    sv39x4("sv39x4", true),
    sv48x4("sv48x4", true),
    sv57x4("sv57x4", true),
    msi_flat("msi_flat", true),
    msi_mrif("msi_mrif", msi_flat),
    amo_mrif("amo_mrif", msi_mrif),
    amo_hwad("amo_hwad", true),
    t2gpa("t2gpa", true),
    pd8("pd8", true),
    pd17("pd17", true),
    pd20("pd20", true),
    passthrough("passthrough", false),
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

    cqb.allow_read_write();
    cqb.sync_always();

    cqh.allow_read_only();
    cqh.sync_always();

    cqt.allow_read_write();
    cqt.sync_always();
    cqt.on_write(&iommu::write_cqt);

    fqb.allow_read_write();
    fqb.sync_always();

    fqh.allow_read_write();
    fqh.sync_always();
    fqh.on_write(&iommu::write_fqh);

    fqt.allow_read_only();
    fqt.sync_always();

    pqb.allow_read_write();
    pqb.sync_always();

    pqh.allow_read_write();
    pqh.sync_always();
    pqh.on_write(&iommu::write_pqh);

    pqt.allow_read_only();
    pqt.sync_always();

    cqcsr.allow_read_write();
    cqcsr.sync_always();
    cqcsr.on_write(&iommu::write_cqcsr);

    fqcsr.allow_read_write();
    fqcsr.sync_always();
    fqcsr.on_write(&iommu::write_fqcsr);

    pqcsr.allow_read_write();
    pqcsr.sync_always();
    pqcsr.on_write(&iommu::write_pqcsr);

    ipsr.allow_read_write();
    ipsr.sync_always();
    ipsr.on_write(&iommu::write_ipsr);

    iocntovf.allow_read_only();
    iocntovf.sync_always();

    iocntinh.allow_read_write();
    iocntinh.sync_always();
    iocntinh.on_write(&iommu::write_iocntinh);

    iohpmcycles.allow_read_write();
    iohpmcycles.sync_always();
    iohpmcycles.on_read(&iommu::read_iohpmcycles);
    iohpmcycles.on_write(&iommu::write_iohpmcycles);

    iohpmctr.allow_read_write();
    iohpmctr.sync_always();

    iohpmevt.allow_read_write();
    iohpmevt.sync_always();
    iohpmevt.on_write(&iommu::write_iohpmevt);

    tr_req_iova.allow_read_write();
    tr_req_iova.sync_never();
    tr_req_iova.on_write(&iommu::write_tr_req_iova);

    tr_req_ctl.allow_read_write();
    tr_req_ctl.sync_always();
    tr_req_ctl.on_write(&iommu::write_tr_req_ctl);

    tr_response.allow_read_only();
    tr_response.sync_never();

    icvec.allow_read_write();
    icvec.sync_always();

    msi_cfg_tbl.allow_read_write();
    msi_cfg_tbl.sync_always();

    load_capabilities();

    SC_HAS_PROCESS(iommu);
    SC_THREAD(worker);
    SC_METHOD(overflow);
    sensitive << m_counter_ovev;
    dont_initialize();
}

iommu::~iommu() {
    // nothing to do
}

void iommu::reset() {
    peripheral::reset();
    load_capabilities();
    m_contexts.clear();
    m_iotlb_s.clear();
    invalidate_direct_mem_ptr(0ull, ~0ull);
    restart_counter(0);
}

unsigned int iommu::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                            address_space as) {
    if (as == IOMMU_AS_DEFAULT)
        return peripheral::receive(tx, info, as);

    if (passthrough)
        return out.send(tx, info);

    iotlb entry{};
    u64 virt = tx.get_address();

    if (!translate(tx, info, false, entry)) {
        tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    if (tx.get_response_status() != TLM_INCOMPLETE_RESPONSE)
        return success(tx) ? tx.get_data_length() : 0;

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

    if (passthrough)
        return out->get_direct_mem_ptr(tx, dmi);

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
    if (passthrough) {
        dma->invalidate_direct_mem_ptr(start, end);
        return;
    }

    if (start <= m_dma_addr && end >= m_dma_addr) {
        m_dma_addr = ~0ull;
        m_dma_xptr = nullptr;
        m_dma_xval = 0;
    }

    if (m_dmi_hi < m_dmi_lo)
        return;

    if (start > m_dmi_hi || end < m_dmi_lo)
        return;

    m_dmi_lo = ~0ull;
    m_dmi_hi = 0;

    dma.dmi_cache().invalidate(0ull, ~0ull);
    dma->invalidate_direct_mem_ptr(0ull, ~0ull);
}

VCML_EXPORT_MODEL(vcml::riscv::iommu, name, args) {
    return new iommu(name);
}

} // namespace riscv
} // namespace vcml
