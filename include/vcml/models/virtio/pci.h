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

#ifndef VCML_VIRTIO_PCI_H
#define VCML_VIRTIO_PCI_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/virtio.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/pci_ids.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

#include "vcml/models/generic/pci_device.h"

namespace vcml { namespace virtio {

    using generic::pci_device;
    using generic::pci_capability;

    enum virtio_cap_type : u8 {
        VIRTIO_PCI_CAP_COMMON  = 1,
        VIRTIO_PCI_CAP_NOTIFY  = 2,
        VIRTIO_PCI_CAP_ISR     = 3,
        VIRTIO_PCI_CAP_DEVICE  = 4,
        VIRTIO_PCI_CAP_PCI_CFG = 5,
    };

    class pci;

    struct pci_cap_virtio : pci_capability {
        reg< u8>* CAP_LEN;
        reg< u8>* CFG_TYPE;
        reg< u8>* BAR;
        reg<u32>* OFFSET;
        reg<u32>* LENGTH;
        reg<u32>* NOTIFY_MULT;

        pci_cap_virtio(const string& nm, u8 type, u8 bar, u32 offset,
                       u32 length, u32 mult);
        virtual ~pci_cap_virtio() = default;
    };

    class pci : public pci_device, public virtio_controller
    {
        friend class pci_cap_virtio;
    private:
        u64 m_drv_features;
        u64 m_dev_features;

        virtio_device_desc m_device;

        std::unordered_map<u32, virtqueue*> m_queues;

        pci_cap_virtio* m_cap_common;
        pci_cap_virtio* m_cap_notify;
        pci_cap_virtio* m_cap_isr;
        pci_cap_virtio* m_cap_device;

        void enable_virtqueue(u32 vqid);
        void disable_virtqueue(u32 vqid);
        void cleanup_virtqueues();

        virtual bool get(u32 vqid, vq_message& msg) override;
        virtual bool put(u32 vqid, vq_message& msg) override;

        virtual bool notify() override;

        virtual tlm_response_status read(const range& addr, void* data,
            const tlm_sbi& info, address_space as) override;
        virtual tlm_response_status write(const range& addr, const void* data,
            const tlm_sbi& info, address_space as) override;

        u32 write_DEVICE_FEATURE_SEL(u32 val);
        u32 write_DRIVER_FEATURE(u32 val);
        u8  write_DEVICE_STATUS(u8 val);

        u16 read_QUEUE_SIZE();
        u16 read_QUEUE_MSIX_VECTOR();
        u16 read_QUEUE_ENABLE();
        u16 read_QUEUE_NOTIFY_OFF();
        u64 read_QUEUE_DESC();
        u64 read_QUEUE_DRIVER();
        u64 read_QUEUE_DEVICE();

        u16 write_QUEUE_SIZE(u16 val);
        u16 write_QUEUE_MSIX_VECTOR(u16 val);
        u16 write_QUEUE_ENABLE(u16 val);
        u16 write_QUEUE_NOTIFY_OFF(u16 val);
        u64 write_QUEUE_DESC(u64 val);
        u64 write_QUEUE_DRIVER(u64 val);
        u64 write_QUEUE_DEVICE(u64 val);

        u32 write_QUEUE_NOTIFY(u32 val);
        u32 read_IRQ_STATUS();

    public:
        property<bool> use_packed_queues;
        property<bool> use_strong_barriers;

        property<unsigned int> msix_vectors;
        property<unsigned int> virtio_bar;
        property<unsigned int> msix_bar;

        pci_address_space virtio_as() const {
            return (pci_address_space)(PCI_AS_BAR0 + virtio_bar);
        }

        pci_address_space msix_as() const {
            return (pci_address_space)(PCI_AS_BAR0 + msix_bar);
        }

        reg<u32> DEVICE_FEATURE_SEL;
        reg<u32> DEVICE_FEATURE;
        reg<u32> DRIVER_FEATURE_SEL;
        reg<u32> DRIVER_FEATURE;
        reg<u16> MSIX_CONFIG;
        reg<u16> NUM_QUEUES;
        reg< u8> DEVICE_STATUS;
        reg< u8> CONFIG_GEN;
        reg<u16> QUEUE_SEL;
        reg<u16> QUEUE_SIZE;
        reg<u16> QUEUE_MSIX_VECTOR;
        reg<u16> QUEUE_ENABLE;
        reg<u16> QUEUE_NOTIFY_OFF;
        reg<u64> QUEUE_DESC;
        reg<u64> QUEUE_DRIVER;
        reg<u64> QUEUE_DEVICE;
        reg<u32> QUEUE_NOTIFY;
        reg<u32> IRQ_STATUS;

        pci_target_socket PCI_IN;
        virtio_initiator_socket VIRTIO_OUT;

        pci(const sc_module_name& nm);
        virtual ~pci();
        VCML_KIND(virtio::pci);
        virtual void reset() override;

        void virtio_declare_common_cap(u8 bar, u32 offset, u32 length);
        void virtio_declare_notify_cap(u8 bar, u32 off, u32 len, u32 mult);
        void virtio_declare_isr_cap(u8 bar, u32 offset, u32 length);
        void virtio_declare_device_cap(u8 bar, u32 offset, u32 length);

        bool has_feature(u64 feature) const;
        bool device_ready() const;
    };

    inline bool pci::has_feature(u64 feature) const {
        return (m_drv_features & m_dev_features & feature) == feature;
    }

    inline bool pci::device_ready() const {
        return DEVICE_STATUS == VIRTIO_STATUS_DEVICE_READY;
    }

}};

#endif
