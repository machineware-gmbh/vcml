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
        reg<pci_device, u8>* CAP_ID;
        reg<pci_device, u8>* CAP_NEXT;

        pci_capability(pci_device* dev, pci_cap_id cap_id);
        virtual ~pci_capability();
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

        pci_cap_msi(pci_device* dev, u16 msi_control);
        virtual ~pci_cap_msi();
    };

    class pci_device: public peripheral, public pci_target
    {
        friend class pci_capability;
        friend class pci_cap_msi;
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
        reg<pci_device, u8 > PCI_REV_ID;
        reg<pci_device, u8 > PCI_HEADER_TYPE;
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

        void declare_bar(int barno, u64 size, u32 type);

        void interrupt(bool state, unsigned int vector = 0);
        void raise_irq(unsigned int vector = 0) { interrupt(true, vector); }
        void lower_irq(unsigned int vector = 0) { interrupt(false, vector); }

        bool msi_enabled() { return m_msi && m_msi->is_enabled(); }
        void msi_interrupt(bool state, unsigned int vector);

        void legacy_interrupt(bool state);

    protected:
        u8 pci_cap_ptr;
        u8 pci_cap_off;

        template <typename T>
        reg<pci_device, T>* new_cap_reg(const string& nm, T v, vcml_access rw);

        template <typename T>
        reg<pci_device, T>* new_cap_reg_ro(const string& nm, T val) {
            return new_cap_reg<T>(nm, val, VCML_ACCESS_READ);
        }

        template <typename T>
        reg<pci_device, T>* new_cap_reg_rw(const string& nm, T val) {
            return new_cap_reg<T>(nm, val, VCML_ACCESS_READ_WRITE);
        }

        virtual void pci_transport(pci_target_socket& socket,
                                   pci_payload& tx) override;

    private:
        pci_bar m_bars[PCI_NUM_BARS];
        pci_irq m_irq;
        pci_cap_msi* m_msi;
        sc_event m_msi_notify;

        void msi_send(unsigned int vector);
        void msi_process();

        u8 read_CAP_PTR() { return pci_cap_ptr; }

        u32 write_BAR(u32 val, u32 barno);

        u16 write_COMMAND(u16 val);
        u16 write_STATUS(u16 val);

        u16 write_MSI_CONTROL(u16 val);
        u32 write_MSI_ADDR(u32 val);
        u32 write_MSI_MASK(u32 val);

        void update_bars();
        void update_irqs();
    };

    template <typename T>
    reg<pci_device, T>* pci_device::new_cap_reg(const string& nm, T defval,
        vcml_access rw) {
        hierarchy_guard guard(this);
        reg<pci_device, T>* r = new reg<pci_device, T>(PCI_AS_CFG, nm.c_str(),
                                                       pci_cap_off, defval);
        if (is_write_allowed(rw))
            r->sync_always();
        else
            r->sync_never();
        r->set_access(rw);
        pci_cap_off += r->size();
        VCML_ERROR_ON(pci_cap_off >= 0x100, "out of PCI config space memory");
        return r;
    }

}}

#endif
