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

#include "vcml/models/virtio/mmio.h"

namespace vcml { namespace virtio {

    void mmio::enable_virtqueue(u32 vqid) {
        log_debug("enabling virtqueue %u", vqid);

        auto it = m_device.virtqueues.find(vqid);
        if (it == m_device.virtqueues.end()) {
            log_warn("invalid virtqueue: %u", vqid);
            return;
        }

        if (m_queues.count(vqid) > 0) {
            log_warn("virtqueue %u already enabled", vqid);
            return;
        }

        virtio_queue_desc& qd = it->second;

        qd.has_event_idx = has_feature(VIRTIO_F_RING_EVENT_IDX);
        qd.desc   = (u64)QUEUE_DESC_HI   << 32 | (u64)QUEUE_DESC_LO;
        qd.driver = (u64)QUEUE_DRIVER_HI << 32 | (u64)QUEUE_DRIVER_LO;
        qd.device = (u64)QUEUE_DEVICE_HI << 32 | (u64)QUEUE_DEVICE_LO;

        qd.size = QUEUE_NUM;
        if (qd.size > qd.limit) {
            log_warn("virtqueue %u size %u exceeds limit %u, truncated", qd.id,
                     qd.size, qd.limit);
            qd.size = qd.limit;
        }

        virtio_dmifn dmifn = [=] (u64 addr, u64 len, vcml_access acs) -> u8* {
            return OUT.lookup_dmi_ptr(addr, len, acs);
        };

        hierarchy_guard guard(this);

        virtqueue* q;
        if (has_feature(VIRTIO_F_RING_PACKED))
            q = m_queues[vqid] = new packed_virtqueue(qd, dmifn);
        else
            q = m_queues[vqid] = new split_virtqueue(qd, dmifn);

        if (!q->validate()) {
            log_warn("failed to enable virtqueue %u", vqid);
            STATUS = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
        }
    }

    void mmio::disable_virtqueue(u32 vqid) {
        log_debug("disabling virtqueue %u", vqid);

        if (m_device.virtqueues.count(vqid) == 0u) {
            log_warn("invalid virtqueue: %u", vqid);
            return;
        }

        auto it = m_queues.find(vqid);
        if (it == m_queues.end()) {
            log_warn("virtqueue %u already disabled", vqid);
            return;
        }

        delete it->second;
        m_queues.erase(it);
    }

    void mmio::cleanup_virtqueues() {
        for (auto it : m_queues)
            delete it.second;
        m_queues.clear();
    }

    void mmio::invalidate_dmi(u64 start, u64 end) {
        for (auto vq : m_queues)
            vq.second->invalidate({start, end});
    }

    bool mmio::get(u32 vqid, vq_message& msg)  {
        if (!device_ready()) {
            log_warn("get: device not ready");
            return false;
        }

        auto it = m_queues.find(vqid);
        if (it == m_queues.end()) {
            log_warn("get: invalid virtqueue id %u", vqid);
            return false;
        }

        virtqueue* q = it->second;
        return q->get(msg);
    }

    bool mmio::put(u32 vqid, vq_message& msg)  {
        if (!device_ready()) {
            log_warn("put: device not ready");
            return false;
        }

        auto it = m_queues.find(vqid);
        if (it == m_queues.end()) {
            log_warn("put: illegal virtqueue %u", vqid);
            return false;
        }

        virtqueue* q = it->second;
        bool result = q->put(msg);
        if (result && q->notify) {
            INTERRUPT_STATUS |= VIRTIO_IRQSTATUS_VQUEUE;
            IRQ = INTERRUPT_STATUS != 0u;
        }

        return result;
    }

    bool mmio::notify() {
        if (!device_ready()) {
            log_warn("configuration change notification while inactive");
            return false;
        }

        INTERRUPT_STATUS |= VIRTIO_IRQSTATUS_CONFIG;
        IRQ = INTERRUPT_STATUS != 0u;
        return true;
    }

    tlm_response_status mmio::read(const range& addr, void* data,
                                   const tlm_sbi& info)  {
        const range& regbase = CONFIG_GEN.get_range();
        if (addr.start <= regbase.end && addr.end > regbase.end)
            return TLM_ADDRESS_ERROR_RESPONSE;
        if (!VIRTIO_OUT->read_config(addr - regbase.end - 1, data))
            return TLM_ADDRESS_ERROR_RESPONSE;

        return TLM_OK_RESPONSE;
    }

    tlm_response_status mmio::write(const range& addr, const void* data,
                                    const tlm_sbi& info)  {
        if (STATUS & VIRTIO_STATUS_DRIVER_OK) {
            log_warn("attempt to change configuration after initialization");
            return TLM_COMMAND_ERROR_RESPONSE;
        }

        const range& regbase = CONFIG_GEN.get_range();
        if (addr.start <= regbase.end && addr.end > regbase.end)
            return TLM_ADDRESS_ERROR_RESPONSE;

        if (!VIRTIO_OUT->write_config(addr - regbase.end - 1, data))
            return TLM_ADDRESS_ERROR_RESPONSE;

        CONFIG_GEN++;
        return TLM_OK_RESPONSE;
    }

    u32 mmio::read_DEVICE_ID() {
        return m_device.device_id;
    }

    u32 mmio::read_VENDOR_ID() {
        return m_device.vendor_id;
    }

    u32 mmio::write_DEVICE_FEATURES_SEL(u32 val) {
        unsigned int shift = val ? 32 : 0;
        DEVICE_FEATURES = (u32)(m_dev_features >> shift);
        return val ? 1 : 0;
    }

    u32 mmio::write_DRIVER_FEATURES(u32 val) {
        if (STATUS & VIRTIO_STATUS_FEATURES_OK) {
            log_warn("attempt to change features after negotiation");
            STATUS = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
            return 0;
        }

        if (DRIVER_FEATURES_SEL) {
            m_drv_features &= bitmask(32, 0);
            m_drv_features |= (u64)val << 32;
        } else {
            m_drv_features &= bitmask(32, 32);
            m_drv_features |= (u64)val;
        }

        return val;
    }

    u32 mmio::write_QUEUE_SEL(u32 val) {
        if (val >= VIRTQUEUE_MAX) {
            log_warn("illegal virtqueue id %u", val);
            return val;
        }

        auto it = m_device.virtqueues.find(val);
        if (it != m_device.virtqueues.end()) {
            const virtio_queue_desc& q = it->second;
            QUEUE_NUM_MAX   = q.limit;
            QUEUE_NUM       = q.size;
            QUEUE_READY     = m_queues.count(val);
            QUEUE_DESC_LO   = (u32)(q.desc);
            QUEUE_DESC_HI   = (u32)(q.desc >> 32);
            QUEUE_DRIVER_LO = (u32)(q.driver);
            QUEUE_DRIVER_HI = (u32)(q.driver >> 32);
            QUEUE_DEVICE_LO = (u32)(q.device);
            QUEUE_DEVICE_HI = (u32)(q.device >> 32);
        } else {
            QUEUE_NUM_MAX   = 0;
            QUEUE_NUM       = 0;
            QUEUE_READY     = 0;
            QUEUE_DESC_LO   = 0;
            QUEUE_DESC_HI   = 0;
            QUEUE_DRIVER_LO = 0;
            QUEUE_DRIVER_HI = 0;
            QUEUE_DEVICE_LO = 0;
            QUEUE_DEVICE_HI = 0;
        }

        return val;
    }

    u32 mmio::write_QUEUE_READY(u32 val) {
        if (val >= VIRTQUEUE_MAX) {
            log_warn("illegal virtqueue id %u", val);
            return val;
        }

        if (val)
            enable_virtqueue(QUEUE_SEL);
        else
            disable_virtqueue(QUEUE_SEL);
        return val;
    }

    u32 mmio::write_QUEUE_NOTIFY(u32 val) {
        if (!device_ready()) {
            log_warn("notify: device not ready");
            return val;
        }

        u32 vqid = val & 0xffff;
        if (vqid >= VIRTQUEUE_MAX) {
            log_warn("illegal virtqueue id %u", vqid);
            return val;
        }

        auto it = m_queues.find(vqid);
        if (it == m_queues.end()) {
            log_warn("notify: illegal queue id: %u", vqid);
            return val;
        }

        log_debug("notifying virtqueue %u", vqid);
        if (!VIRTIO_OUT->notify(vqid)) {
            log_warn("notify: device reported failure");
            STATUS = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
        }

        return val;
    }

    u32 mmio::write_INTERRRUPT_ACK(u32 val) {
        val &= VIRTIO_IRQSTATUS_MASK;

        INTERRUPT_ACK = val;
        INTERRUPT_STATUS &= ~INTERRUPT_ACK;
        IRQ = INTERRUPT_STATUS != 0u;

        return INTERRUPT_ACK;
    }

    u32 mmio::write_STATUS(u32 val) {
        val &= VIRTIO_STATUS_MASK;

        if (val == 0u) {
            log_debug("software triggered device reset");
            reset();
            return 0u;
        }

        if (popcnt(val ^ STATUS) > 1)
            log_warn("multiple status bits changed at once");
        if (popcnt(STATUS) > popcnt(val))
            log_warn("attempt to clear individual status bits");

        if (val == VIRTIO_STATUS_FEATURE_CHECK) {
            if (m_drv_features & ~m_dev_features)
                return val & ~VIRTIO_STATUS_FEATURES_OK;
            if (!VIRTIO_OUT->write_features(m_drv_features))
                return val & ~VIRTIO_STATUS_FEATURES_OK;
        }

        return val;
    }

    mmio::mmio(const sc_module_name& nm):
        peripheral(nm),
        virtio_controller(),
        m_drv_features(),
        m_dev_features(),
        m_queues(),
        use_packed_queues("use_packed_queues", false),
        use_strong_barriers("use_strong_barriers", false),
        MAGIC               ("MAGIC",               0x00, fourcc("virt")),
        VERSION             ("VERSION",             0x04, 2),
        DEVICE_ID           ("DEVICE_ID",           0x08, 0),
        VENDOR_ID           ("VENDOR_ID",           0x0c, 0),
        DEVICE_FEATURES     ("DEVICE_FEATURES",     0x10, 0),
        DEVICE_FEATURES_SEL ("DEVICE_FEATURES_SEL", 0x14, 0),
        DRIVER_FEATURES     ("DRIVER_FEATURES",     0x20, 0),
        DRIVER_FEATURES_SEL ("DRIVER_FEATURES_SEL", 0x24, 0),
        QUEUE_SEL           ("QUEUE_SEL",           0x30, 0),
        QUEUE_NUM_MAX       ("QUEUE_NUM_MAX",       0x34, 0),
        QUEUE_NUM           ("QUEUE_NUM",           0x38, 0),
        QUEUE_READY         ("QUEUE_READY",         0x44, 0),
        QUEUE_NOTIFY        ("QUEUE_NOTIFY",        0x50, 0),
        INTERRUPT_STATUS    ("INTERRUPT_STATUS",    0x60, 0),
        INTERRUPT_ACK       ("INTERRUPT_ACK",       0x64, 0),
        STATUS              ("STATUS",              0x70, 0),
        QUEUE_DESC_LO       ("QUEUE_DESC_LO",       0x80, 0),
        QUEUE_DESC_HI       ("QUEUE_DESC_HI",       0x84, 0),
        QUEUE_DRIVER_LO     ("QUEUE_DRIVER_LO",     0x90, 0),
        QUEUE_DRIVER_HI     ("QUEUE_DRIVER_HI",     0x94, 0),
        QUEUE_DEVICE_LO     ("QUEUE_DEVICE_LO",     0xa0, 0),
        QUEUE_DEVICE_HI     ("QUEUE_DEVICE_HI",     0xa4, 0),
        CONFIG_GEN          ("CONFIG_GEN",          0xfc, 0),
        IN("IN"),
        OUT("OUT"),
        IRQ("IRQ"),
        VIRTIO_OUT("VIRTIO_OUT") {

        MAGIC.sync_never();
        MAGIC.allow_read_only();

        VERSION.sync_never();
        VERSION.allow_read_only();

        DEVICE_ID.sync_never();
        DEVICE_ID.allow_read_only();
        DEVICE_ID.on_read(&mmio::read_DEVICE_ID);

        VENDOR_ID.sync_never();
        VENDOR_ID.allow_read_only();
        VENDOR_ID.on_read(&mmio::read_VENDOR_ID);

        DEVICE_FEATURES.sync_never();
        DEVICE_FEATURES.allow_read_only();

        DEVICE_FEATURES_SEL.sync_never();
        DEVICE_FEATURES_SEL.allow_write_only();
        DEVICE_FEATURES_SEL.on_write(&mmio::write_DEVICE_FEATURES_SEL);

        DRIVER_FEATURES.sync_never();
        DRIVER_FEATURES.allow_write_only();
        DRIVER_FEATURES.on_write(&mmio::write_DRIVER_FEATURES);

        DRIVER_FEATURES_SEL.sync_never();
        DRIVER_FEATURES_SEL.allow_write_only();

        QUEUE_SEL.sync_never();
        QUEUE_SEL.allow_write_only();
        QUEUE_SEL.on_write(&mmio::write_QUEUE_SEL);

        QUEUE_NUM_MAX.sync_never();
        QUEUE_NUM_MAX.allow_read_only();

        QUEUE_NUM.sync_never();
        QUEUE_NUM.allow_write_only();

        QUEUE_READY.sync_never();
        QUEUE_READY.allow_read_write();
        QUEUE_READY.on_write(&mmio::write_QUEUE_READY);

        QUEUE_NOTIFY.sync_always();
        QUEUE_NOTIFY.allow_write_only();
        QUEUE_NOTIFY.on_write(&mmio::write_QUEUE_NOTIFY);

        INTERRUPT_STATUS.sync_always();
        INTERRUPT_STATUS.allow_read_only();

        INTERRUPT_ACK.sync_always();
        INTERRUPT_ACK.allow_write_only();
        INTERRUPT_ACK.on_write(&mmio::write_INTERRRUPT_ACK);

        STATUS.sync_always();
        STATUS.allow_read_write();
        STATUS.on_write(&mmio::write_STATUS);

        QUEUE_DESC_LO.sync_never();
        QUEUE_DESC_LO.allow_write_only();

        QUEUE_DESC_HI.sync_never();
        QUEUE_DESC_HI.allow_write_only();

        QUEUE_DRIVER_LO.sync_never();
        QUEUE_DRIVER_LO.allow_write_only();

        QUEUE_DRIVER_HI.sync_never();
        QUEUE_DRIVER_HI.allow_write_only();

        QUEUE_DEVICE_LO.sync_never();
        QUEUE_DEVICE_LO.allow_write_only();

        QUEUE_DEVICE_HI.sync_never();
        QUEUE_DEVICE_HI.allow_write_only();

        CONFIG_GEN.sync_always();
        CONFIG_GEN.allow_read_only();
    }

    mmio:: ~mmio() {
        cleanup_virtqueues();
    }

    void mmio::reset()  {
        peripheral::reset();

        cleanup_virtqueues();

        m_drv_features = 0ull;
        m_dev_features = 0ull;

        VIRTIO_OUT->identify(m_device);
        VIRTIO_OUT->read_features(m_dev_features);
        m_dev_features |= VIRTIO_F_VERSION_1;
        m_dev_features |= VIRTIO_F_RING_EVENT_IDX;
        m_dev_features |= VIRTIO_F_RING_INDIRECT_DESC;

        if (use_packed_queues)
            m_dev_features |= VIRTIO_F_RING_PACKED;

        if (use_strong_barriers)
            m_dev_features |= VIRTIO_F_ORDER_PLATFORM;
    }

}}
