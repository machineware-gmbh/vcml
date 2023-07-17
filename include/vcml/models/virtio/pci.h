/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_PCI_H
#define VCML_VIRTIO_PCI_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/virtio.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/pci_ids.h"

#include "vcml/models/pci/device.h"

namespace vcml {
namespace virtio {

using pci::device;
using pci::capability;

enum virtio_cap_type : u8 {
    VIRTIO_PCI_CAP_COMMON = 1,
    VIRTIO_PCI_CAP_NOTIFY = 2,
    VIRTIO_PCI_CAP_ISR = 3,
    VIRTIO_PCI_CAP_DEVICE = 4,
    VIRTIO_PCI_CAP_PCI_CFG = 5,
};

class pci;

struct cap_virtio : capability {
    reg<u8>* cap_len;
    reg<u8>* cfg_type;
    reg<u8>* cap_bar;
    reg<u32>* offset;
    reg<u32>* length;
    reg<u32>* notify_mult;

    cap_virtio(const string& nm, u8 type, u8 bar, u32 offset, u32 length,
               u32 mult);
    virtual ~cap_virtio() = default;
};

class pci : public device, public virtio_controller
{
    friend struct cap_virtio;

private:
    u64 m_drv_features;
    u64 m_dev_features;

    virtio_device_desc m_device;

    unordered_map<u32, virtqueue*> m_queues;

    cap_virtio* m_cap_common;
    cap_virtio* m_cap_notify;
    cap_virtio* m_cap_isr;
    cap_virtio* m_cap_device;

    void enable_virtqueue(u32 vqid);
    void disable_virtqueue(u32 vqid);
    void cleanup_virtqueues();

    virtual bool get(u32 vqid, vq_message& msg) override;
    virtual bool put(u32 vqid, vq_message& msg) override;

    virtual bool notify() override;

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info,
                                     address_space as) override;
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info,
                                      address_space as) override;

    void write_device_feature_sel(u32 val);
    void write_driver_feature(u32 val);
    void write_device_status(u8 val);

    u16 read_queue_size();
    u16 read_queue_msix_vector();
    u16 read_queue_enable();
    u16 read_queue_notify_off();
    u64 read_queue_desc();
    u64 read_queue_driver();
    u64 read_queue_device();

    void write_queue_size(u16 val);
    void write_queue_msix_vector(u16 val);
    void write_queue_enable(u16 val);
    void write_queue_notify_off(u16 val);
    void write_queue_desc(u64 val);
    void write_queue_driver(u64 val);
    void write_queue_device(u64 val);

    void write_queue_notify(u32 val);
    u32 read_irq_status();

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

    reg<u32> device_feature_sel;
    reg<u32> device_feature;
    reg<u32> driver_feature_sel;
    reg<u32> driver_feature;
    reg<u16> msix_config;
    reg<u16> num_queues;
    reg<u8> device_status;
    reg<u8> config_gen;
    reg<u16> queue_sel;
    reg<u16> queue_size;
    reg<u16> queue_msix_vector;
    reg<u16> queue_enable;
    reg<u16> queue_notify_off;
    reg<u64> queue_desc;
    reg<u64> queue_driver;
    reg<u64> queue_device;
    reg<u32> queue_notify;
    reg<u32> irq_status;

    pci_target_socket pci_in;
    virtio_initiator_socket virtio_out;

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
    return virtio_device_ready(device_status);
}

} // namespace virtio
}; // namespace vcml

#endif
