/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/mmio.h"

namespace vcml {
namespace virtio {

enum virtio_mmio_address_space : address_space {
    VIRTIO_MMIO_AS_BUS = VCML_AS_DEFAULT,
    VIRTIO_MMIO_AS_SHM,
};

void mmio::enable_virtqueue(u32 vqid) {
    log_debug("enabling virtqueue %u", vqid);

    auto it = m_device_desc.virtqueues.find(vqid);
    if (it == m_device_desc.virtqueues.end()) {
        log_warn("invalid virtqueue: %u", vqid);
        return;
    }

    if (m_queues.count(vqid) > 0) {
        log_warn("virtqueue %u already enabled", vqid);
        return;
    }

    virtio_queue_desc& qd = it->second;

    qd.has_event_idx = has_feature(VIRTIO_F_RING_EVENT_IDX);
    qd.desc = (u64)queue_desc_hi << 32 | (u64)queue_desc_lo;
    qd.driver = (u64)queue_driver_hi << 32 | (u64)queue_driver_lo;
    qd.device = (u64)queue_device_hi << 32 | (u64)queue_device_lo;

    qd.size = queue_num;
    if (qd.size > qd.limit) {
        log_warn("virtqueue %u size %u exceeds limit %u, truncated", qd.id,
                 qd.size, qd.limit);
        qd.size = qd.limit;
    }

    virtio_dmifn dmifn = [=](u64 addr, u64 len, vcml_access acs) -> u8* {
        return out.lookup_dmi_ptr(addr, len, acs);
    };

    auto guard = get_hierarchy_scope();

    virtqueue* q;
    if (has_feature(VIRTIO_F_RING_PACKED))
        q = m_queues[vqid] = new packed_virtqueue(qd, dmifn);
    else
        q = m_queues[vqid] = new split_virtqueue(qd, dmifn);

    if (!q->validate()) {
        log_warn("failed to enable virtqueue %u", vqid);
        status = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
    }
}

void mmio::disable_virtqueue(u32 vqid) {
    log_debug("disabling virtqueue %u", vqid);

    if (m_device_desc.virtqueues.count(vqid) == 0u) {
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

void mmio::reset_virtqueue(u32 vqid) {
    log_debug("resetting virtqueue %u", vqid);

    auto desc = m_device_desc.virtqueues.find(vqid);
    if (desc == m_device_desc.virtqueues.end()) {
        log_warn("invalid virtqueue: %u", vqid);
        return;
    }

    auto queue = m_queues.find(vqid);
    if (queue == m_queues.end()) {
        log_warn("virtqueue %u already disabled", vqid);
        return;
    }

    delete queue->second;
    m_queues.erase(queue);

    desc->second.size = 0;
    desc->second.desc = 0;
    desc->second.driver = 0;
    desc->second.device = 0;
    desc->second.vector = VIRTIO_NO_VECTOR;
    desc->second.has_event_idx = false;
}

void mmio::cleanup_virtqueues() {
    for (auto it : m_queues)
        delete it.second;
    m_queues.clear();
}

void mmio::reset_device() {
    cleanup_virtqueues();
    m_drv_features = 0;
    status = 0;
    virtio_out->reset();
}

void mmio::invalidate_dmi(u64 start, u64 end) {
    for (auto vq : m_queues)
        vq.second->invalidate({ start, end });
}

bool mmio::get(u32 vqid, vq_message& msg) {
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

bool mmio::put(u32 vqid, vq_message& msg) {
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
        interrupt_status |= VIRTIO_IRQSTATUS_VQUEUE;
        irq = interrupt_status != 0u;
    }

    return result;
}

bool mmio::notify() {
    if (!device_ready()) {
        log_warn("configuration change notification while inactive");
        return false;
    }

    interrupt_status |= VIRTIO_IRQSTATUS_CONFIG;
    irq = interrupt_status != 0u;
    return true;
}

bool mmio::shm_map(u32 shmid, u64 id, u64 offset, void* ptr, u64 len) {
    if (!m_shm)
        return false;
    if (!m_shm->map(shmid, id, offset, ptr, len))
        return false;

    const virtio_shared_object* obj = m_shm->find(shmid, id);
    if (obj && obj->data) {
        tlm_dmi dmi;
        dmi.allow_read_write();
        dmi.set_start_address(obj->addr.start);
        dmi.set_end_address(obj->addr.end);
        dmi.set_dmi_ptr(obj->data);
        shm.map_dmi(dmi);
    }

    return true;
}

bool mmio::shm_unmap(u32 shmid, u64 id) {
    if (!m_shm)
        return false;

    const virtio_shared_object* obj = m_shm->find(shmid, id);
    if (obj && obj->data)
        shm.unmap_dmi(obj->addr);

    return m_shm->unmap(shmid, id);
}

unsigned int mmio::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                           address_space as) {
    switch (as) {
    case VIRTIO_MMIO_AS_BUS:
        return peripheral::receive(tx, info, as);
    case VIRTIO_MMIO_AS_SHM:
        if (m_shm)
            return m_shm->transport(virtio_out, tx);
        // no break
    default:
        break;
    }

    tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
    return 0;
}

tlm_response_status mmio::read(const range& addr, void* data,
                               const tlm_sbi& info) {
    u64 limit = 0x100;
    if (addr.start < limit) {
        memset(data, 0xff, addr.length());
        return TLM_OK_RESPONSE;
    }

    if (!virtio_out->read_config(addr - limit, data))
        return TLM_ADDRESS_ERROR_RESPONSE;

    return TLM_OK_RESPONSE;
}

tlm_response_status mmio::write(const range& addr, const void* data,
                                const tlm_sbi& info) {
    if (status & VIRTIO_STATUS_DRIVER_OK) {
        log_warn("attempt to change configuration after initialization");
        return TLM_COMMAND_ERROR_RESPONSE;
    }

    u64 limit = 0x100;
    if (addr.start <= limit)
        return TLM_OK_RESPONSE;

    if (!virtio_out->write_config(addr - limit, data))
        return TLM_ADDRESS_ERROR_RESPONSE;

    config_gen++;
    return TLM_OK_RESPONSE;
}

void mmio::before_end_of_elaboration() {
    peripheral::before_end_of_elaboration();

    if (shm_size == 0 && !shm.is_bound())
        shm.stub();
}

void mmio::end_of_elaboration() {
    cleanup_virtqueues();

    m_drv_features = 0ull;
    m_dev_features = 0ull;

    m_device_desc.reset();

    if (m_shm) {
        m_shm->reset();
        m_device_desc.shm_capacity = m_shm->capacity();
    }

    virtio_out->identify(m_device_desc);

    if (m_shm) {
        for (auto& [shmid, desc] : m_device_desc.shmems) {
            if (!m_shm->request(shmid, desc.capacity))
                log_warn("failed to request shared memory %u", shmid);
        }
    }

    virtio_out->read_features(m_dev_features);

    m_dev_features |= VIRTIO_F_VERSION_1;
    m_dev_features |= VIRTIO_F_RING_EVENT_IDX;
    m_dev_features |= VIRTIO_F_RING_INDIRECT_DESC;
    m_dev_features |= VIRTIO_F_RING_RESET;

    if (use_packed_queues)
        m_dev_features |= VIRTIO_F_RING_PACKED;

    if (use_strong_barriers)
        m_dev_features |= VIRTIO_F_ORDER_PLATFORM;
}

u32 mmio::read_device_id() {
    return m_device_desc.device_id;
}

u32 mmio::read_vendor_id() {
    return m_device_desc.vendor_id;
}

void mmio::write_device_features_sel(u32 val) {
    unsigned int shift = val ? 32 : 0;
    device_features = (u32)(m_dev_features >> shift);
    device_features_sel = val ? 1 : 0;
}

void mmio::write_driver_features(u32 val) {
    if (status & VIRTIO_STATUS_FEATURES_OK) {
        log_warn("attempt to change features after negotiation");
        status = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
        return;
    }

    if (driver_features_sel) {
        m_drv_features &= bitmask(32, 0);
        m_drv_features |= (u64)val << 32;
    } else {
        m_drv_features &= bitmask(32, 32);
        m_drv_features |= (u64)val;
    }

    driver_features = val;
}

void mmio::write_queue_sel(u32 val) {
    if (val >= VIRTQUEUE_MAX) {
        log_warn("illegal virtqueue id %u", val);
        return;
    }

    queue_sel = val;
    auto it = m_device_desc.virtqueues.find(val);
    if (it != m_device_desc.virtqueues.end()) {
        const virtio_queue_desc& q = it->second;
        queue_num_max = q.limit;
        queue_num = q.size;
        queue_ready = m_queues.count(val);
        queue_desc_lo = (u32)(q.desc);
        queue_desc_hi = (u32)(q.desc >> 32);
        queue_driver_lo = (u32)(q.driver);
        queue_driver_hi = (u32)(q.driver >> 32);
        queue_device_lo = (u32)(q.device);
        queue_device_hi = (u32)(q.device >> 32);
    } else {
        queue_num_max = 0;
        queue_num = 0;
        queue_ready = 0;
        queue_desc_lo = 0;
        queue_desc_hi = 0;
        queue_driver_lo = 0;
        queue_driver_hi = 0;
        queue_device_lo = 0;
        queue_device_hi = 0;
    }
}

void mmio::write_queue_ready(u32 val) {
    if (val >= VIRTQUEUE_MAX) {
        log_warn("illegal virtqueue id %u", val);
        return;
    }

    if ((queue_ready = val))
        enable_virtqueue(queue_sel);
    else
        disable_virtqueue(queue_sel);
}

void mmio::write_queue_notify(u32 val) {
    if (!device_ready()) {
        log_warn("notify: device not ready");
        return;
    }

    u32 vqid = val & 0xffff;
    if (vqid >= VIRTQUEUE_MAX) {
        log_warn("illegal virtqueue id %u", vqid);
        return;
    }

    auto it = m_queues.find(vqid);
    if (it == m_queues.end()) {
        log_warn("notify: illegal queue id: %u", vqid);
        return;
    }

    queue_notify = vqid;

    log_debug("notifying virtqueue %u", vqid);
    if (!virtio_out->notify(vqid)) {
        log_warn("notify: device reported failure");
        status = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
    }
}

void mmio::write_interrrupt_ack(u32 val) {
    interrupt_ack = val & VIRTIO_IRQSTATUS_MASK;
    interrupt_status &= ~interrupt_ack;
    irq = interrupt_status != 0u;
}

void mmio::write_status(u32 val) {
    val &= VIRTIO_STATUS_MASK;

    if (val == 0u) {
        log_debug("software triggered device reset");
        reset_device();
        return;
    }

    if (popcnt(val ^ status) > 1)
        log_warn("multiple status bits changed at once");
    if (popcnt(status) > popcnt(val))
        log_warn("attempt to clear individual status bits");

    status = val;

    if (virtio_feature_check(val)) {
        if (m_drv_features & ~m_dev_features)
            status &= ~VIRTIO_STATUS_FEATURES_OK;
        else if (!virtio_out->write_features(m_drv_features))
            status &= ~VIRTIO_STATUS_FEATURES_OK;
    }
}

void mmio::write_shm_sel(u32 val) {
    if (m_shm) {
        shm_sel = val;
        shm_len_lo = m_shm->region_size(val);
        shm_len_hi = m_shm->region_size(val) >> 32;
        shm_base_lo = (shm_base + m_shm->region_base(val));
        shm_base_hi = (shm_base + m_shm->region_base(val)) >> 32;
    } else {
        shm_sel = -1;
        shm_len_lo = -1;
        shm_len_hi = -1;
        shm_base_lo = -1;
        shm_base_hi = -1;
    }
}

void mmio::write_queue_reset(u32 val) {
    if (!has_feature(VIRTIO_F_RING_RESET))
        log_warn("attempt to reset virtqueue without VIRTIO_F_RING_RESET");

    if (val == 1) {
        queue_reset = 1;
        reset_virtqueue(queue_sel);
        queue_reset = 0;
    }
}

mmio::mmio(const sc_module_name& nm):
    peripheral(nm),
    virtio_controller(),
    m_drv_features(),
    m_dev_features(),
    m_queues(),
    m_shm(),
    use_packed_queues("use_packed_queues", false),
    use_strong_barriers("use_strong_barriers", false),
    shm_base("shm_base", 0),
    shm_size("shm_size", 0),
    magic("magic", 0x00, fourcc("virt")),
    version("version", 0x04, 2),
    device_id("device_id", 0x08, 0),
    vendor_id("vendor_id", 0x0c, 0),
    device_features("device_features", 0x10, 0),
    device_features_sel("device_features_sel", 0x14, 0),
    driver_features("driver_features", 0x20, 0),
    driver_features_sel("driver_features_sel", 0x24, 0),
    queue_sel("queue_sel", 0x30, 0),
    queue_num_max("queue_num_max", 0x34, 0),
    queue_num("queue_num", 0x38, 0),
    queue_ready("queue_ready", 0x44, 0),
    queue_notify("queue_notify", 0x50, 0),
    interrupt_status("interrupt_status", 0x60, 0),
    interrupt_ack("interrupt_ack", 0x64, 0),
    status("status", 0x70, 0),
    queue_desc_lo("queue_desc_lo", 0x80, 0),
    queue_desc_hi("queue_desc_hi", 0x84, 0),
    queue_driver_lo("queue_driver_lo", 0x90, 0),
    queue_driver_hi("queue_driver_hi", 0x94, 0),
    queue_device_lo("queue_device_lo", 0xa0, 0),
    queue_device_hi("queue_device_hi", 0xa4, 0),
    shm_sel("shm_sel", 0xac, -1),
    shm_len_lo("shm_len_lo", 0xb0, -1),
    shm_len_hi("shm_len_hi", 0xb4, -1),
    shm_base_lo("shm_base_lo", 0xb8, -1),
    shm_base_hi("shm_base_hi", 0xbc, -1),
    queue_reset("queue_reset", 0xc0, 0),
    config_gen("config_gen", 0xfc, 0),
    in("in", VIRTIO_MMIO_AS_BUS),
    shm("shm", VIRTIO_MMIO_AS_SHM),
    out("out"),
    irq("irq"),
    virtio_out("virtio_out") {
    magic.sync_never();
    magic.allow_read_only();

    version.sync_never();
    version.allow_read_only();

    device_id.sync_never();
    device_id.allow_read_only();
    device_id.on_read(&mmio::read_device_id);

    vendor_id.sync_never();
    vendor_id.allow_read_only();
    vendor_id.on_read(&mmio::read_vendor_id);

    device_features.sync_never();
    device_features.allow_read_only();

    device_features_sel.sync_never();
    device_features_sel.allow_write_only();
    device_features_sel.on_write(&mmio::write_device_features_sel);

    driver_features.sync_never();
    driver_features.allow_write_only();
    driver_features.on_write(&mmio::write_driver_features);

    driver_features_sel.sync_never();
    driver_features_sel.allow_write_only();

    queue_sel.sync_never();
    queue_sel.allow_write_only();
    queue_sel.on_write(&mmio::write_queue_sel);

    queue_num_max.sync_never();
    queue_num_max.allow_read_only();

    queue_num.sync_never();
    queue_num.allow_write_only();

    queue_ready.sync_never();
    queue_ready.allow_read_write();
    queue_ready.on_write(&mmio::write_queue_ready);

    queue_notify.sync_always();
    queue_notify.allow_write_only();
    queue_notify.on_write(&mmio::write_queue_notify);

    interrupt_status.sync_always();
    interrupt_status.allow_read_only();

    interrupt_ack.sync_always();
    interrupt_ack.allow_write_only();
    interrupt_ack.on_write(&mmio::write_interrrupt_ack);

    status.sync_always();
    status.allow_read_write();
    status.on_write(&mmio::write_status);

    queue_desc_lo.sync_never();
    queue_desc_lo.allow_write_only();

    queue_desc_hi.sync_never();
    queue_desc_hi.allow_write_only();

    queue_driver_lo.sync_never();
    queue_driver_lo.allow_write_only();

    queue_driver_hi.sync_never();
    queue_driver_hi.allow_write_only();

    queue_device_lo.sync_never();
    queue_device_lo.allow_write_only();

    queue_device_hi.sync_never();
    queue_device_hi.allow_write_only();

    shm_sel.sync_always();
    shm_sel.allow_read_write();
    shm_sel.on_write(&mmio::write_shm_sel);

    shm_len_lo.sync_never();
    shm_len_lo.allow_read_only();

    shm_len_hi.sync_never();
    shm_len_hi.allow_read_only();

    shm_base_lo.sync_never();
    shm_base_lo.allow_read_only();

    shm_base_hi.sync_never();
    shm_base_hi.allow_read_only();

    queue_reset.sync_always();
    queue_reset.allow_read_write();
    queue_reset.on_write(&mmio::write_queue_reset);

    config_gen.sync_always();
    config_gen.allow_read_only();

    if (shm_size > 0)
        m_shm = new virtio_shared_memory(shm_size);
}

mmio::~mmio() {
    cleanup_virtqueues();

    if (m_shm)
        delete m_shm;
}

void mmio::reset() {
    peripheral::reset();
    reset_device();
}

VCML_EXPORT_MODEL(vcml::virtio::mmio, name, args) {
    return new mmio(name);
}

} // namespace virtio
} // namespace vcml
