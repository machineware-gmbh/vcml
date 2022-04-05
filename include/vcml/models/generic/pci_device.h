/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_PCI_DEVICE_H
#define VCML_GENERIC_PCI_DEVICE_H

#include "vcml/common/types.h"
#include "vcml/common/range.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"

#include "vcml/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/pci.h"

namespace vcml {
namespace generic {

class pci_device;

struct pci_capability {
    const string name;
    vector<reg_base*> registers;
    pci_device* device;

    reg<u8>* cap_id;
    reg<u8>* cap_next;

    pci_capability(const string& nm, pci_cap_id cap_id);
    virtual ~pci_capability();

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

struct pci_cap_pm : pci_capability {
    reg<u16>* pm_caps;
    reg<u32>* pm_ctrl;

    pci_cap_pm(const string& nm, u16 caps);
    virtual ~pci_cap_pm() = default;
};

struct pci_cap_msi : pci_capability {
    reg<u16>* msi_control;
    reg<u32>* msi_addr;
    reg<u32>* msi_addr_hi;
    reg<u16>* msi_data;
    reg<u32>* msi_mask;
    reg<u32>* msi_pending;

    size_t max_vectors() const { return 1u << ((*msi_control >> 1) & 7); }
    size_t num_vectors() const { return 1u << ((*msi_control >> 4) & 7); }

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

    pci_cap_msi(const string& nm, u16 msi_control);
    virtual ~pci_cap_msi() = default;
};

struct pci_cap_msix : pci_capability {
    const range tbl;
    const range bpa;
    const u32 bar;
    const address_space bar_as;
    const size_t num_vectors;

    struct msix_entry {
        u64 addr;
        u32 data;
        u32 ctrl;
    } * msix_table;

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

    pci_cap_msix(const string& nm, u32 bar, size_t nvec, u32 offset = 0);
    virtual ~pci_cap_msix();
    void reset();

    tlm_response_status read_table(const range& addr, void* data);
    tlm_response_status write_table(const range& addr, const void* data);
};

class pci_device : public peripheral, public pci_target
{
    friend struct pci_capability;
    friend struct pci_cap_pm;
    friend struct pci_cap_msi;
    friend struct pci_cap_msix;

public:
    enum pci_command_bits : u16 {
        PCI_COMMAND_IO         = 1 << 0,
        PCI_COMMAND_MMIO       = 1 << 1,
        PCI_COMMAND_BUS_MASTER = 1 << 2,
        PCI_COMMAND_SPECIAL    = 1 << 3,
        PCI_COMMAND_INVALIDATE = 1 << 4,
        PCI_COMMAND_PALETTE    = 1 << 5,
        PCI_COMMAND_PARITY     = 1 << 6,
        PCI_COMMAND_WAIT       = 1 << 7,
        PCI_COMMAND_SERR       = 1 << 8,
        PCI_COMMAND_FAST_B2B   = 1 << 9,
        PCI_COMMAND_NO_IRQ     = 1 << 10,
    };

    enum pci_status_bits : u16 {
        PCI_STATUS_IRQ                 = 1 << 3,
        PCI_STATUS_CAPABILITY_LIST     = 1 << 4,
        PCI_STATUS_66MHZ_CAPABLE       = 1 << 5,
        PCI_STATUS_FAST_B2B            = 1 << 7,
        PCI_STATUS_MASTER_PARITY_ERROR = 1 << 8,
        PCI_STATUS_TX_TARGET_ABORT     = 1 << 11,
        PCI_STATUS_RX_TARGET_ABORT     = 1 << 12,
        PCI_STATUS_RX_MASTER_ABORT     = 1 << 13,
        PCI_STATUS_TX_SYSTEM_ERROR     = 1 << 14,
        PCI_STATUS_PARITY_ERROR        = 1 << 15,
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

    pci_device(const sc_module_name& name, const pci_config& config);
    virtual ~pci_device();
    VCML_KIND(pci_device);
    virtual void reset() override;

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info,
                                     address_space as) override;

    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info,
                                      address_space as) override;

    void pci_declare_bar(int barno, u64 size, u32 type);

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
    virtual void pci_transport(const pci_target_socket& socket,
                               pci_payload& tx) override;

private:
    pci_bar m_bars[PCI_NUM_BARS];
    pci_irq m_irq;
    pci_cap_pm* m_pm;
    pci_cap_msi* m_msi;
    pci_cap_msix* m_msix;
    sc_event m_msi_notify;
    sc_event m_msix_notify;

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

    using pci_target::pci_interrupt;
};

template <typename T>
reg<T>* pci_capability::new_cap_reg(const string& regnm, T val,
                                    vcml_access rw) {
    hierarchy_guard guard(device);
    string nm = mkstr("%s_%s", name.c_str(), regnm.c_str());
    reg<T>* r = new reg<T>(PCI_AS_CFG, nm, device->curr_cap_off, val);
    if (is_write_allowed(rw))
        r->sync_always();
    else
        r->sync_never();
    r->set_access(rw);
    registers.push_back(r);
    device->curr_cap_off += r->size();
    if (device->curr_cap_off > 0x100)
        VCML_ERROR("out of PCI config space memory");
    return r;
}

} // namespace generic
} // namespace vcml

#endif
