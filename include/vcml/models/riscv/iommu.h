/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RISCV_IOMMU_H
#define VCML_RISCV_IOMMU_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace riscv {

class iommu : public peripheral
{
private:
    enum iommu_address_space : address_space {
        IOMMU_AS_DEFAULT = VCML_AS_DEFAULT,
        IOMMU_AS_DMA,
    };

    struct context {
        u32 device_id;
        u32 process_id;
        u64 tc;
        u64 ta;
        u64 satp;
        u64 gatp;
        u64 msi_addr_mask;
        u64 msi_addr_pattern;
        u64 msiptp;
    };

    struct iotlb {
        u64 vpn : 44;
        u64 pscid : 20;
        u64 ppn : 44;
        u64 gscid : 16;
        u64 r : 1;
        u64 w : 1;
        u64 dmi : 1;
        u64 unused : 1;
    };

    static_assert(sizeof(iotlb) == 2 * sizeof(u64), "iotlb_entry size");

    unordered_map<u64, context> m_contexts;
    unordered_map<u64, iotlb> m_iotlb;

    int fetch_context(u32 devid, u32 procid, bool dbg, bool dmi, context& ctx);
    int fetch_iotlb(context& ctx, u64 virt, bool dbg, bool dmi, iotlb& entry);

    bool translate(u32 devid, u32 procid, u64 virt, bool dbg, bool dmi,
                   iotlb& entry);

    void load_capabilities();

    void write_fctl(u32 val);
    void write_ddtp(u64 val);

    sc_event m_workev;
    void worker();

public:
    property<bool> sv32;
    property<bool> sv39;
    property<bool> sv48;
    property<bool> sv57;
    property<bool> svpbmt;
    property<bool> sv32x4;
    property<bool> sv39x4;
    property<bool> sv48x4;
    property<bool> sv57x4;
    property<bool> amo_mrif;
    property<bool> msi_flat;
    property<bool> msi_mrif;
    property<bool> amo_hwad;
    property<bool> t2gpa;
    property<bool> pd8;
    property<bool> pd17;
    property<bool> pd20;

    reg<u64> caps;
    reg<u32> fctl;
    reg<u64> ddtp;
    reg<u64> cqb;
    reg<u32> cqh;
    reg<u32> cqt;
    reg<u64> fqb;
    reg<u32> fqh;
    reg<u32> fqt;
    reg<u64> pqb;
    reg<u32> pqh;
    reg<u32> pqt;
    reg<u32> cqcsr;
    reg<u32> fqcsr;
    reg<u32> pqcsr;
    reg<u32> ipsr;
    reg<u32> iocntovf;
    reg<u32> iocntinh;
    reg<u64> iohpmcycles;
    reg<u64, 31> iohpmctr;
    reg<u64, 31> iohpmevt;
    reg<u64> tr_req_iova;
    reg<u64> tr_req_ctl;
    reg<u64> tr_response;
    reg<u64> icvec;
    reg<u64, 16> msi_cfg_tbl;

    tlm_initiator_socket out;

    tlm_target_socket in;
    tlm_target_socket dma;

    gpio_initiator_socket cirq;
    gpio_initiator_socket firq;
    gpio_initiator_socket pmirq;
    gpio_initiator_socket pirq;

    iommu(const sc_module_name& nm);
    virtual ~iommu();
    VCML_KIND(riscv::iommu);

    virtual void reset() override;

protected:
    virtual unsigned int receive(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as) override;
    virtual bool get_direct_mem_ptr(tlm_target_socket& origin,
                                    tlm_generic_payload& tx,
                                    tlm_dmi& dmi) override;
};

} // namespace riscv
} // namespace vcml

#endif
