/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PCI_DEVICE_H
#define VCML_PCI_DEVICE_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/pci.h"

namespace vcml {
namespace pci {

class device;

struct capability {
    const string name;
    vector<reg_base*> registers;
    device* dev;

    reg<u8>* cap_id;
    reg<u8>* nxt_ptr;

    capability(const string& nm, pci_cap_id cap_id);
    virtual ~capability();

    template <typename T>
    reg<T>* new_cap_reg(const string& nm, T v, vcml_access rw);

    template <typename T>
    reg<T>* new_cap_reg_ro(const string& nm, T val) {
        return new_cap_reg<T>(nm, val, VCML_ACCESS_READ);
    }

    template <typename T>
    reg<T>* new_cap_reg_rw(const string& nm, T val) {
        return new_cap_reg<T>(nm, val, VCML_ACCESS_READ_WRITE);
    }
};

struct cap_pm : capability {
    reg<u16>* pm_caps;
    reg<u32>* pm_ctrl;

    cap_pm(const string& nm, u16 caps);
    virtual ~cap_pm() = default;
};

struct cap_msi : capability {
    reg<u16>* msi_control;
    reg<u32>* msi_addr;
    reg<u32>* msi_addr_hi;
    reg<u16>* msi_data;
    reg<u32>* msi_mask;
    reg<u32>* msi_pending;

    size_t max_vectors() const { return 1ull << ((*msi_control >> 1) & 7); }
    size_t num_vectors() const { return 1ull << ((*msi_control >> 4) & 7); }

    bool is_enabled() const { return *msi_control & PCI_MSI_ENABLE; }
    bool is_64bit() const { return *msi_control & PCI_MSI_64BIT; }
    bool is_vector() const { return *msi_control & PCI_MSI_VECTOR; }

    bool is_masked(unsigned int vector) const {
        return msi_mask ? (*msi_mask >> vector) & 1u : false;
    }

    bool is_pending(unsigned int vector) const {
        return msi_pending ? (*msi_pending >> vector) & 1u : false;
    }

    void set_pending(unsigned int vector, bool set = true);

    cap_msi(const string& nm, u16 msi_control);
    virtual ~cap_msi() = default;
};

struct cap_msix : capability {
    range tbl;
    range pba;

    address_space tbl_as;
    address_space pba_as;

    size_t num_vectors;

    struct msix_entry {
        u64 addr;
        u32 data;
        u32 ctrl;
    };

    msix_entry* msix_table;

    u32* msix_pba;

    reg<u16>* msix_control;
    reg<u32>* msix_bir_off;
    reg<u32>* msix_pba_off;

    bool is_enabled() const { return *msix_control & PCI_MSIX_ENABLE; }

    bool is_masked(unsigned int vector) const {
        return (*msix_control & PCI_MSIX_ALL_MASKED) ||
               (msix_table[vector].ctrl & PCI_MSIX_MASKED);
    }

    bool is_pending(unsigned int vector) const {
        return (msix_pba[vector / 32] >> (vector % 32)) & 1;
    }

    void set_masked(unsigned int vector, bool set = true);
    void set_pending(unsigned int vector, bool set = true);

    cap_msix(const string& nm, u32 bar, size_t nvec, u32 offset = 0);
    virtual ~cap_msix();
    void reset();
    void update();

    tlm_response_status read_tbl(const range& addr, void* data);
    tlm_response_status write_tbl(const range& addr, const void* data);

    tlm_response_status read_pba(const range& addr, void* data);
    tlm_response_status write_pba(const range& addr, const void* data);
};

struct cap_pcie : capability {
    reg<u16>* flags;

    reg<u32>* dev_cap;
    reg<u16>* dev_ctl;
    reg<u16>* dev_sts;

    reg<u32>* link_cap;
    reg<u16>* link_ctl;
    reg<u16>* link_sts;

    reg<u32>* slot_cap;
    reg<u16>* slot_ctl;
    reg<u16>* slot_sts;

    reg<u16>* root_cap;
    reg<u16>* root_ctl;
    reg<u32>* root_sts;

    reg<u32>* dev_cap2;
    reg<u16>* dev_ctl2;
    reg<u16>* dev_sts2;

    reg<u32>* link_cap2;
    reg<u16>* link_ctl2;
    reg<u16>* link_sts2;

    reg<u32>* slot_cap2;
    reg<u16>* slot_ctl2;
    reg<u16>* slot_sts2;

    cap_pcie(const string& nm);
    virtual ~cap_pcie() = default;
};

class device : public peripheral, public pci_target
{
    friend struct capability;
    friend struct cap_pm;
    friend struct cap_msi;
    friend struct cap_msix;

public:
    enum pci_command_bits : u16 {
        PCI_COMMAND_IO = bit(0),
        PCI_COMMAND_MMIO = bit(1),
        PCI_COMMAND_BUS_MASTER = bit(2),
        PCI_COMMAND_SPECIAL = bit(3),
        PCI_COMMAND_INVALIDATE = bit(4),
        PCI_COMMAND_PALETTE = bit(5),
        PCI_COMMAND_PARITY = bit(6),
        PCI_COMMAND_WAIT = bit(7),
        PCI_COMMAND_SERR = bit(8),
        PCI_COMMAND_FAST_B2B = bit(9),
        PCI_COMMAND_NO_IRQ = bit(10),
    };

    enum pci_status_bits : u16 {
        PCI_STATUS_IRQ = bit(3),
        PCI_STATUS_CAPABILITY_LIST = bit(4),
        PCI_STATUS_66MHZ_CAPABLE = bit(5),
        PCI_STATUS_FAST_B2B = bit(7),
        PCI_STATUS_MASTER_PARITY_ERROR = bit(8),
        PCI_STATUS_TX_TARGET_ABORT = bit(11),
        PCI_STATUS_RX_TARGET_ABORT = bit(12),
        PCI_STATUS_RX_MASTER_ABORT = bit(13),
        PCI_STATUS_TX_SYSTEM_ERROR = bit(14),
        PCI_STATUS_PARITY_ERROR = bit(15),
    };

    constexpr u16 pci_status_init(bool pcie) {
        return pcie ? PCI_STATUS_CAPABILITY_LIST
                    : PCI_STATUS_CAPABILITY_LIST | PCI_STATUS_66MHZ_CAPABLE |
                          PCI_STATUS_FAST_B2B;
    }

    property<bool> pcie;

    reg<u16> pci_vendor_id;
    reg<u16> pci_device_id;
    reg<u16> pci_command;
    reg<u16> pci_status;
    reg<u32> pci_class;
    reg<u8> pci_cache_line;
    reg<u8> pci_latency_timer;
    reg<u8> pci_header_type;
    reg<u8> pci_bist;
    reg<u32, PCI_NUM_BARS> pci_bars;
    reg<u16> pci_subvendor_id;
    reg<u16> pci_subdevice_id;
    reg<u8> pci_cap_ptr;
    reg<u8> pci_int_line;
    reg<u8> pci_int_pin;
    reg<u8> pci_min_grant;
    reg<u8> pci_max_latency;
    reg<u32> pcie_xcap;

    size_t curr_cap_ptr;
    size_t curr_cap_off;

    device(const sc_module_name& name, const pci_config& config);
    virtual ~device();
    VCML_KIND(pci::device);
    virtual void reset() override;

    void pci_declare_bar(int barno, u64 size, u32 type, void* ptr = nullptr);

    void pci_declare_pm_cap(u16 pm_caps);
    void pci_declare_msi_cap(u16 msi_ctrl);
    void pci_declare_msix_cap(u32 bar, size_t num_vectors, u32 offset = 0);

    void pci_interrupt(bool state, unsigned int vector = 0);
    void pci_raise_irq(unsigned int vec = 0) { pci_interrupt(true, vec); }
    void pci_lower_irq(unsigned int vec = 0) { pci_interrupt(false, vec); }

    bool msix_enabled() { return m_msix && m_msix->is_enabled(); }
    void msix_interrupt(bool state, unsigned int vector);

    bool msi_enabled() { return m_msi && m_msi->is_enabled(); }
    void msi_interrupt(bool state, unsigned int vector);

    void pci_legacy_interrupt(bool state);

protected:
    pci_bar m_bars[PCI_NUM_BARS];
    pci_irq m_irq;
    cap_pm* m_pm;
    cap_msi* m_msi;
    cap_msix* m_msix;
    cap_pcie* m_pcie;
    sc_event m_msi_notify;
    sc_event m_msix_notify;
    vector<reg_base*> m_temp_rw_regs;

    bool is_bypassing_cfgro() const { return !m_temp_rw_regs.empty(); }
    void pci_bypass_cfgro(bool enable);

    using pci_target::pci_interrupt;

    virtual void pci_transport(const pci_target_socket& socket,
                               pci_payload& tx) override;

    virtual bool read_mem_bar(const range& addr, void* data,
                              const tlm_sbi& sbi, address_space as);
    virtual bool write_mem_bar(const range& addr, const void* data,
                               const tlm_sbi& sbi, address_space as);

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info,
                                     address_space as) override;

    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info,
                                      address_space as) override;

    void msi_send(unsigned int vector);
    void msi_process();

    void msix_send(unsigned int vector);
    void msix_process();

    void write_bars(u32 val, size_t barno);

    void write_command(u16 val);
    void write_status(u16 val);

    void write_pm_ctrl(u32 val);

    void write_msi_ctrl(u16 val);
    void write_msi_addr(u32 val);
    void write_msi_mask(u32 val);

    void write_msix_ctrl(u16 val);

    void update_bars();
    void update_irqs();
};

template <typename T>
reg<T>* capability::new_cap_reg(const string& regnm, T val, vcml_access rw) {
    auto guard = dev->get_hierarchy_scope();
    string nm = mkstr("%s_%s", name.c_str(), regnm.c_str());
    reg<T>* r = new reg<T>(PCI_AS_CFG, nm, dev->curr_cap_off, val);
    if (is_write_allowed(rw))
        r->sync_always();
    else
        r->sync_never();
    r->set_access(rw);
    registers.push_back(r);
    dev->curr_cap_off += r->size();
    if (dev->curr_cap_off > 0x100)
        VCML_ERROR("out of PCI config space memory");
    return r;
}

} // namespace pci
} // namespace vcml

#endif
