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
#include "vcml/common/systemc.h"

#include "vcml/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/pci.h"

namespace vcml { namespace generic {

    class pci_device;

    struct pci_capability {
        const string name;
        vector<reg_base*> registers;
        pci_device* device;

        reg<pci_device, u8>* CAP_ID;
        reg<pci_device, u8>* CAP_NEXT;

        pci_capability(const string& nm, pci_cap_id cap_id);
        virtual ~pci_capability();

        template <typename T, typename HOST = pci_device>
        reg<HOST, T>* new_cap_reg(const string& nm, T v, vcml_access rw);

        template <typename T, typename HOST = pci_device>
        reg<HOST, T>* new_cap_reg_ro(const string& nm, T val) {
            return new_cap_reg<T, HOST>(nm, val, VCML_ACCESS_READ);
        }

        template <typename T, typename HOST = pci_device>
        reg<HOST, T>* new_cap_reg_rw(const string& nm, T val) {
            return new_cap_reg<T, HOST>(nm, val, VCML_ACCESS_READ_WRITE);
        }
    };

    struct pci_cap_pm : pci_capability {
        reg<pci_device, u16>* PM_CAPS;
        reg<pci_device, u32>* PM_CTRL;

        pci_cap_pm(const string& nm, u16 caps);
        virtual ~pci_cap_pm() = default;
    };

    struct pci_cap_msi : pci_capability {
        reg<pci_device, u16>* MSI_CONTROL;
        reg<pci_device, u32>* MSI_ADDR;
        reg<pci_device, u32>* MSI_ADDR_HI;
        reg<pci_device, u16>* MSI_DATA;
        reg<pci_device, u32>* MSI_MASK;
        reg<pci_device, u32>* MSI_PENDING;

        size_t max_vectors() const { return 1u << ((*MSI_CONTROL >> 1) & 7); }
        size_t num_vectors() const { return 1u << ((*MSI_CONTROL >> 4) & 7); }

        bool is_enabled() const { return *MSI_CONTROL & PCI_MSI_ENABLE; }
        bool is_64bit()   const { return *MSI_CONTROL & PCI_MSI_64BIT;  }
        bool is_vector()  const { return *MSI_CONTROL & PCI_MSI_VECTOR; }

        bool is_masked(unsigned int vector) const {
            return MSI_MASK ? (*MSI_MASK >> vector) & 1u : false;
        }

        bool is_pending(unsigned int vector) const {
            return MSI_PENDING ? (*MSI_PENDING >> vector) & 1u : false;
        }

        void set_pending(unsigned int vector, bool set = true);

        pci_cap_msi(const string& nm, u16 msi_control);
        virtual ~pci_cap_msi() = default;
    };

    struct pci_cap_msix : pci_capability {
        const range mem;
        const u32 bar;
        const address_space bar_as;
        const size_t num_vectors;

        struct msix_entry {
            u32 data;
            u32 addr;
        }* msix_table;

        reg<pci_device, u16>* MSIX_CONTROL;
        reg<pci_device, u32>* MSIX_ADDR_HI;
        reg<pci_device, u32>* MSIX_BIR_OFF;

        bool is_enabled() const {
            return *MSIX_CONTROL & PCI_MSIX_ENABLE;
        }

        bool is_masked(unsigned int vector) const {
            return msix_table[vector].addr & PCI_MSIX_MASKED;
        }

        bool is_pending(unsigned int vector) const {
            return msix_table[vector].addr & PCI_MSIX_PENDING;
        }

        void set_masked(unsigned int vector, bool set = true);
        void set_pending(unsigned int vector, bool set = true);

        pci_cap_msix(const string& nm, u32 bar, size_t nvec,
                     u32 offset = 0);
        virtual ~pci_cap_msix();
        void reset();

        tlm_response_status read_table(const range& addr, void* data);
        tlm_response_status write_table(const range& addr, const void* data);
    };

    class pci_device: public peripheral, public pci_target
    {
        friend class pci_capability;
        friend class pci_cap_pm;
        friend class pci_cap_msi;
        friend class pci_cap_msix;
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

        constexpr u16 PCI_STATUS_INIT(bool pcie) {
            return pcie ? PCI_STATUS_CAPABILITY_LIST
                        : PCI_STATUS_CAPABILITY_LIST |
                          PCI_STATUS_66MHZ_CAPABLE |
                          PCI_STATUS_FAST_B2B;
        }

        property<bool> pcie;

        reg<pci_device, u16> PCI_VENDOR_ID;
        reg<pci_device, u16> PCI_DEVICE_ID;
        reg<pci_device, u16> PCI_COMMAND;
        reg<pci_device, u16> PCI_STATUS;
        reg<pci_device, u32> PCI_CLASS;
        reg<pci_device, u8 > PCI_CACHE_LINE;
        reg<pci_device, u8 > PCI_LATENCY_TIMER;
        reg<pci_device, u8 > PCI_HEADER_TYPE;
        reg<pci_device, u8 > PCI_BIST;
        reg<pci_device, u32, PCI_NUM_BARS> PCI_BAR;
        reg<pci_device, u16> PCI_SUBVENDOR_ID;
        reg<pci_device, u16> PCI_SUBDEVICE_ID;
        reg<pci_device, u8 > PCI_CAP_PTR;
        reg<pci_device, u8 > PCI_INT_LINE;
        reg<pci_device, u8 > PCI_INT_PIN;
        reg<pci_device, u8 > PCI_MIN_GRANT;
        reg<pci_device, u8 > PCI_MAX_LATENCY;
        reg<pci_device, u32> PCIE_XCAP;

        pci_device(const sc_module_name& name, const pci_config& config);
        virtual ~pci_device();
        VCML_KIND(pci_device);
        virtual void reset() override;

        virtual tlm_response_status read(const range& addr, void* data,
            const tlm_sbi& info, address_space as) override;

        virtual tlm_response_status write(const range& addr, const void* data,
            const tlm_sbi& info, address_space as) override;

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

        u8 pci_cap_ptr;
        u8 pci_cap_off;

    protected:
        virtual void pci_transport(pci_target_socket& socket,
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

        u8 read_CAP_PTR() { return pci_cap_ptr; }

        u32 write_BAR(u32 val, u32 barno);

        u16 write_COMMAND(u16 val);
        u16 write_STATUS(u16 val);

        u32 write_PM_CTRL(u32 val);

        u16 write_MSI_CTRL(u16 val);
        u32 write_MSI_ADDR(u32 val);
        u32 write_MSI_MASK(u32 val);

        u16 write_MSIX_CTRL(u16 val);

        void update_bars();
        void update_irqs();
    };

    template <typename T, typename HOST>
    reg<HOST, T>* pci_capability::new_cap_reg(const string& regnm, T val,
        vcml_access rw) {
        hierarchy_guard guard(device);
        string rname = mkstr("%s_%s", name.c_str(), regnm.c_str());
        reg<pci_device, T>* r = new reg<HOST, T>(PCI_AS_CFG, rname.c_str(),
                                                 device->pci_cap_off, val);
        if (is_write_allowed(rw))
            r->sync_always();
        else
            r->sync_never();
        r->set_access(rw);
        registers.push_back(r);
        device->pci_cap_off += r->size();
        if (device->pci_cap_off > 0x100)
            VCML_ERROR("out of PCI config space memory");
        return r;
    }

}}

#endif
