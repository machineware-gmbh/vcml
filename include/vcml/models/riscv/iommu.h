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
        u64 pbmt : 2;
    };

    struct command {
        u64 opcode : 7;
        u64 func3 : 3;
        u64 operands0 : 54;
        u64 operands1;
    };

    struct fault {
        u64 cause : 12;
        u64 pid : 20;
        u64 pv : 1;
        u64 priv : 1;
        u64 ttyp : 6;
        u64 did : 24;
        u64 reserved;
        u64 iotval;
        u64 iotval2;
    };

    struct pgreq {
        u64 reserved0 : 12;
        u64 pid : 20;
        u64 pv : 1;
        u64 priv : 1;
        u64 exec : 1;
        u64 reserved1 : 5;
        u64 did : 24;
        u64 r : 1;
        u64 w : 1;
        u64 l : 1;
        u64 prgidx : 9;
        u64 pgaddr : 52;
    };

    struct vmcfg {
        u64 root;
        size_t levels;
        size_t vpnbits;
        size_t ptesize;
        bool sum;
        bool adue;
        bool pbmt;
    };

    static_assert(sizeof(iotlb) == 2 * sizeof(u64), "iotlb size");
    static_assert(sizeof(fault) == 4 * sizeof(u64), "fault size");
    static_assert(sizeof(pgreq) == 2 * sizeof(u64), "pgreq size");
    static_assert(sizeof(command) == 2 * sizeof(u64), "command size");

    unordered_map<u64, context> m_contexts;
    unordered_map<u64, iotlb> m_iotlb_s;
    unordered_map<u64, iotlb> m_iotlb_g;

    u32 m_work;
    sc_event m_workev;

    queue<fault> m_faults;
    queue<pgreq> m_pgreqs;

    u64 m_iotval2;

    u64 m_dmi_lo;
    u64 m_dmi_hi;

    u64 m_dma_addr;
    u64 m_dma_xval;
    u8* m_dma_xptr;

    u64 m_counter_val;
    sc_time m_counter_start;
    sc_event m_counter_ovev;

    template <typename T>
    tlm_response_status dma_readw(u64 addr, T& data, bool excl, bool dbg) {
        return dma_read(addr, &data, sizeof(T), excl, dbg);
    }

    template <typename T>
    tlm_response_status dma_writew(u64 addr, T& data, bool dbg) {
        return dma_write(addr, &data, sizeof(T), nullptr, dbg);
    }

    template <typename T>
    tlm_response_status dma_writex(u64 addr, T& data, bool& excl, bool dbg) {
        excl = true;
        return dma_write(addr, &data, sizeof(T), &excl, dbg);
    }

    tlm_response_status dma_read(u64 addr, void* data, size_t sz, bool excl,
                                 bool dbg);
    tlm_response_status dma_write(u64 addr, void* data, size_t sz, bool* excl,
                                  bool dbg);

    bool check_context(const context& ctx) const;
    bool check_msi(const context& ctx, u64 addr) const;

    int fetch_context(const tlm_sbi& info, bool dmi, context& ctx);
    int fetch_iotlb(context& ctx, tlm_generic_payload& tx, const tlm_sbi& info,
                    bool dmi, iotlb& entry);

    vmcfg get_vm_config(const context& ctx, bool g);

    int tablewalk(context& ctx, u64 va, bool g, bool super, bool wnr, bool ind,
                  bool dbg, iotlb& entry);
    int translate_g(context& ctx, u64 virt, bool wnr, bool ind, bool dbg,
                    u64& phys);
    int translate_msi(context& ctx, tlm_generic_payload& tx,
                      const tlm_sbi& info, u64 gpa, iotlb& entry);
    int transmit_mrif(context& ctx, tlm_generic_payload& tx,
                      const tlm_sbi& info, u64 msipte[2]);

    bool translate(tlm_generic_payload& tx, const tlm_sbi& sbi, bool dmi,
                   iotlb& entry);

    void restart_counter(u64 val);
    void increment_counter(context& ctx, u32 event);

    void load_capabilities();

    u64 read_iohpmcycles();

    void write_fctl(u32 val);
    void write_ddtp(u64 val);
    void write_cqt(u32 val);
    void write_fqh(u32 val);
    void write_pqh(u32 val);
    void write_cqcsr(u32 val);
    void write_fqcsr(u32 val);
    void write_pqcsr(u32 val);
    void write_ipsr(u32 val);
    void write_iocntinh(u32 val);
    void write_iohpmcycles(u64 val);
    void write_iohpmevt(u64 val, size_t idx);
    void write_tr_req_iova(u64 val);
    void write_tr_req_ctl(u64 val);

    void handle_iotinval(const command& cmd);
    void handle_iofence(const command& cmd);
    void handle_iodir(const command& cmd);
    void handle_ats(const command& cmd);

    void handle_command();
    void handle_fault();
    void handle_pgreq();
    void handle_trreq();

    void worker();
    void overflow();

    void update_cqcsr(u32 setmask);
    void update_ipsr(u32 setmask, u32 clrmask);

    void report_fault(const fault& req);
    void report_pgreq(const pgreq& req);
    void send_msi(u32 ipsr);

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
    property<bool> msi_flat;
    property<bool> msi_mrif;
    property<bool> amo_mrif;
    property<bool> amo_hwad;
    property<bool> t2gpa;
    property<bool> pd8;
    property<bool> pd17;
    property<bool> pd20;
    property<bool> passthrough;

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

    iommu(const sc_module_name& nm, bool passthrough = true);
    virtual ~iommu();
    VCML_KIND(riscv::iommu);

    virtual void reset() override;

    void flush_contexts() { m_contexts.clear(); }
    void flush_tlb_s() { m_iotlb_s.clear(); }
    void flush_tlb_g() { m_iotlb_g.clear(); }

protected:
    virtual unsigned int receive(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as) override;
    virtual bool get_direct_mem_ptr(tlm_target_socket& origin,
                                    tlm_generic_payload& tx,
                                    tlm_dmi& dmi) override;
    virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                           u64 start, u64 end) override;
    virtual void invalidate_direct_mem_ptr(u64 start, u64 end);
};

} // namespace riscv
} // namespace vcml

#endif
