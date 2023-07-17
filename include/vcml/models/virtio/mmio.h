/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_MMIO_H
#define VCML_VIRTIO_MMIO_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/virtio.h"

namespace vcml {
namespace virtio {

class mmio : public peripheral, public virtio_controller
{
private:
    u64 m_drv_features;
    u64 m_dev_features;

    virtio_device_desc m_device;

    std::unordered_map<u32, virtqueue*> m_queues;

    void enable_virtqueue(u32 vqid);
    void disable_virtqueue(u32 vqid);
    void cleanup_virtqueues();

    virtual void invalidate_dmi(u64 start, u64 end) override;

    virtual bool get(u32 vqid, vq_message& msg) override;
    virtual bool put(u32 vqid, vq_message& msg) override;

    virtual bool notify() override;

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info) override;
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info) override;

    u8* lookup_dmi_ptr(u64 addr, vcml_access acs);

    u32 read_device_id();
    u32 read_vendor_id();

    void write_device_features_sel(u32 val);
    void write_driver_features(u32 val);
    void write_queue_sel(u32 val);
    void write_queue_ready(u32 val);
    void write_queue_notify(u32 val);
    void write_interrrupt_ack(u32 val);
    void write_status(u32 val);

    // disabled
    mmio() = delete;
    mmio(const mmio&) = delete;

public:
    property<bool> use_packed_queues;
    property<bool> use_strong_barriers;

    reg<u32> magic;
    reg<u32> version;
    reg<u32> device_id;
    reg<u32> vendor_id;
    reg<u32> device_features;
    reg<u32> device_features_sel;
    reg<u32> driver_features;
    reg<u32> driver_features_sel;
    reg<u32> queue_sel;
    reg<u32> queue_num_max;
    reg<u32> queue_num;
    reg<u32> queue_ready;
    reg<u32> queue_notify;
    reg<u32> interrupt_status;
    reg<u32> interrupt_ack;
    reg<u32> status;
    reg<u32> queue_desc_lo;
    reg<u32> queue_desc_hi;
    reg<u32> queue_driver_lo;
    reg<u32> queue_driver_hi;
    reg<u32> queue_device_lo;
    reg<u32> queue_device_hi;
    reg<u32> config_gen;

    tlm_target_socket in;
    tlm_initiator_socket out;
    gpio_initiator_socket irq;
    virtio_initiator_socket virtio_out;

    mmio(const sc_module_name& nm);
    virtual ~mmio();
    VCML_KIND(virtio::mmio);

    virtual void reset() override;

    bool has_feature(u64 feature) const;
    bool device_ready() const;
};

inline bool mmio::has_feature(u64 feature) const {
    return (m_drv_features & m_dev_features & feature) == feature;
}

inline bool mmio::device_ready() const {
    return virtio_device_ready(status);
}

} // namespace virtio
} // namespace vcml

#endif
