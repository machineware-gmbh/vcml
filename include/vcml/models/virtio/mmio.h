/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#ifndef VCML_VIRTIO_MMIO_H
#define VCML_VIRTIO_MMIO_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/virtio.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml { namespace virtio {

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

        virtual tlm_response_status read  (const range& addr, void* data,
                                           const tlm_sbi& info) override;
        virtual tlm_response_status write (const range& addr, const void* data,
                                           const tlm_sbi& info) override;

        u8* lookup_dmi_ptr(u64 addr, vcml_access acs);

        u32 read_DEVICE_ID();
        u32 read_VENDOR_ID();

        u32 write_DEVICE_FEATURES_SEL(u32 val);
        u32 write_DRIVER_FEATURES(u32 val);
        u32 write_QUEUE_SEL(u32 val);
        u32 write_QUEUE_READY(u32 val);
        u32 write_QUEUE_NOTIFY(u32 val);
        u32 write_INTERRRUPT_ACK(u32 val);
        u32 write_STATUS(u32 val);

        // disabled
        mmio() = delete;
        mmio(const mmio&) = delete;

    public:
        property<bool> use_packed_queues;
        property<bool> use_strong_barriers;

        enum mmio_status_bits : u32 {
            VIRTIO_STATUS_ACKNOWLEDGE        = 1u << 0,
            VIRTIO_STATUS_DRIVER             = 1u << 1,
            VIRTIO_STATUS_DRIVER_OK          = 1u << 2,
            VIRTIO_STATUS_FEATURES_OK        = 1u << 3,
            VIRTIO_STATUS_DEVICE_NEEDS_RESET = 1u << 6,
            VIRTIO_STATUS_FAILED             = 1u << 7,

            VIRTIO_STATUS_FEATURE_CHECK      = VIRTIO_STATUS_ACKNOWLEDGE |
                                               VIRTIO_STATUS_DRIVER      |
                                               VIRTIO_STATUS_FEATURES_OK,
            VIRTIO_STATUS_DEVICE_READY       = VIRTIO_STATUS_ACKNOWLEDGE |
                                               VIRTIO_STATUS_DRIVER      |
                                               VIRTIO_STATUS_FEATURES_OK |
                                               VIRTIO_STATUS_DRIVER_OK,

            VIRTIO_STATUS_MASK               = bitmask(8),
        };

        enum mmio_irq_status_bits : u32 {
            VIRTIO_IRQSTATUS_VQUEUE = 1u << 0,
            VIRTIO_IRQSTATUS_CONFIG = 1u << 1,

            VIRTIO_IRQSTATUS_MASK   = bitmask(2, 0),
        };

        enum mmio_virtqueue_ids : u32 {
            VIRTQUEUE_MAX = 1024,
        };

        reg<mmio, u32> MAGIC;
        reg<mmio, u32> VERSION;
        reg<mmio, u32> DEVICE_ID;
        reg<mmio, u32> VENDOR_ID;
        reg<mmio, u32> DEVICE_FEATURES;
        reg<mmio, u32> DEVICE_FEATURES_SEL;
        reg<mmio, u32> DRIVER_FEATURES;
        reg<mmio, u32> DRIVER_FEATURES_SEL;
        reg<mmio, u32> QUEUE_SEL;
        reg<mmio, u32> QUEUE_NUM_MAX;
        reg<mmio, u32> QUEUE_NUM;
        reg<mmio, u32> QUEUE_READY;
        reg<mmio, u32> QUEUE_NOTIFY;
        reg<mmio, u32> INTERRUPT_STATUS;
        reg<mmio, u32> INTERRUPT_ACK;
        reg<mmio, u32> STATUS;
        reg<mmio, u32> QUEUE_DESC_LO;
        reg<mmio, u32> QUEUE_DESC_HI;
        reg<mmio, u32> QUEUE_DRIVER_LO;
        reg<mmio, u32> QUEUE_DRIVER_HI;
        reg<mmio, u32> QUEUE_DEVICE_LO;
        reg<mmio, u32> QUEUE_DEVICE_HI;
        reg<mmio, u32> CONFIG_GEN;

        out_port<bool> IRQ;

        tlm_slave_socket IN;
        tlm_initiator_socket OUT;
        virtio_initiator_socket VIRTIO_OUT;

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
        return STATUS == VIRTIO_STATUS_DEVICE_READY;
    }

}}

#endif
