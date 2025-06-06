/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/pci.h"

namespace vcml {
namespace virtio {

constexpr u8 virtio_cap_size(u8 type) {
    switch (type) {
    case VIRTIO_PCI_CAP_NOTIFY:
        return 20;
    case VIRTIO_PCI_CAP_SHM_CFG:
        return 24;
    default:
        return 16;
    }
}

cap_virtio::cap_virtio(const string& nm, u8 type, u8 bar, u64 off, u64 len,
                       u32 arg0):
    capability(nm, PCI_CAPABILITY_VENDOR),
    cap_len(),
    cfg_type(),
    cap_bar(),
    offset(),
    length(),
    notify_mult() {
    u8 subid = type == VIRTIO_PCI_CAP_SHM_CFG ? arg0 : 0xff;
    cap_len = new_cap_reg_ro<u8>("cap_len", virtio_cap_size(type));
    cfg_type = new_cap_reg_ro<u8>("cfg_type", type);
    cap_bar = new_cap_reg_ro<u8>("cap_bar", bar);
    cap_subid = new_cap_reg_ro<u8>("cap_subid", subid);
    dev->curr_cap_off += 2; // align
    offset = new_cap_reg_ro<u32>("offset", off);
    length = new_cap_reg_ro<u32>("length", len);
    if (type == VIRTIO_PCI_CAP_NOTIFY)
        notify_mult = new_cap_reg_ro<u32>("notify_mult", arg0);
    if (type == VIRTIO_PCI_CAP_SHM_CFG) {
        offset_hi = new_cap_reg_ro<u32>("offset_hi", off >> 32);
        length_hi = new_cap_reg_ro<u32>("length_hi", len >> 32);
    }
}

static const pci_config VIRTIO_PCIE_CONFIG = {
    /* pcie            = */ false,
    /* vendor_id       = */ PCI_VENDOR_QUMRANET,
    /* device_id       = */ PCI_DEVICE_VIRTIO,
    /* subvendor_id    = */ 0xffff,
    /* subsystem_id    = */ 0xffff,
    /* class_code      = */ pci_class_code(0, 0, 0, 1),
    /* latency_timer   = */ 0,
    /* max_latency     = */ 0,
    /* min_grant       = */ 0,
    /* int_pin         = */ PCI_IRQ_A,
};

void pci::enable_virtqueue(u32 vqid) {
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
    virtio_dmifn dmifn = [=](u64 addr, u64 len, vcml_access rw) -> u8* {
        return (u8*)pci_in->pci_dma_ptr(rw, addr, len);
    };

    auto guard = get_hierarchy_scope();

    virtqueue* q;
    if (has_feature(VIRTIO_F_RING_PACKED))
        q = m_queues[vqid] = new packed_virtqueue(qd, dmifn);
    else
        q = m_queues[vqid] = new split_virtqueue(qd, dmifn);

    if (!q->validate()) {
        log_warn("failed to enable virtqueue %u", vqid);
        device_status = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
    }
}

void pci::disable_virtqueue(u32 vqid) {
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

void pci::reset_virtqueue(u32 vqid) {
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

void pci::cleanup_virtqueues() {
    for (auto it : m_queues)
        delete it.second;
    m_queues.clear();
}

void pci::reset_device() {
    cleanup_virtqueues();
    m_drv_features = 0;
    device_status = 0;
    virtio_out->reset();
}

bool pci::get(u32 vqid, vq_message& msg) {
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

bool pci::put(u32 vqid, vq_message& msg) {
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
    bool irqen = true;
    if (msix_enabled() || msi_enabled())
        irqen = q->vector != VIRTIO_NO_VECTOR;
    if (irqen && result && q->notify) {
        irq_status |= VIRTIO_IRQSTATUS_VQUEUE;
        pci_interrupt(true, q->vector);
    }

    return result;
}

bool pci::notify() {
    if (!device_ready()) {
        log_warn("configuration change notification while inactive");
        return false;
    }

    irq_status |= VIRTIO_IRQSTATUS_CONFIG;
    pci_interrupt(true, msix_config);
    return true;
}

bool pci::shm_map(u32 shmid, u64 id, u64 offset, void* ptr, u64 len) {
    if (!m_shm)
        return false;

    return m_shm->map(shmid, id, offset, ptr, len);
}

bool pci::shm_unmap(u32 shmid, u64 id) {
    if (!m_shm)
        return false;

    const virtio_shared_object* obj = m_shm->find(shmid, id);
    if (obj && obj->data)
        pci_in->pci_dmi_invalidate(shm_bar, obj->addr.start, obj->addr.end);

    return m_shm->unmap(shmid, id);
}

bool pci::pci_get_dmi_ptr(const pci_target_socket& socket,
                          const pci_payload& tx, tlm_dmi& dmi) {
    if (tx.space != shm_as() || !m_shm)
        return false;

    return m_shm->get_dmi_ptr(tx.addr, dmi);
}

unsigned int pci::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                          address_space as) {
    if (as == shm_as() && m_shm)
        return m_shm->transport(virtio_out, tx);
    else
        return device::receive(tx, info, as);
}

tlm_response_status pci::read(const range& addr, void* data,
                              const tlm_sbi& info, address_space as) {
    if (as != virtio_as())
        return device::read(addr, data, info, as);

    static const range device(0x3000, 0x3fff);
    if (device.includes(addr)) {
        if (!virtio_out->read_config(addr - device.start, data))
            return TLM_ADDRESS_ERROR_RESPONSE;
        return TLM_OK_RESPONSE;
    }

    return TLM_ADDRESS_ERROR_RESPONSE;
}

tlm_response_status pci::write(const range& addr, const void* data,
                               const tlm_sbi& info, address_space as) {
    if (as != virtio_as())
        return device::write(addr, data, info, as);

    static const range device(0x3000, 0x3fff);
    if (device.includes(addr)) {
        if (!virtio_out->write_config(addr - device.start, data))
            return TLM_ADDRESS_ERROR_RESPONSE;
        config_gen++;
        return TLM_OK_RESPONSE;
    }

    return TLM_ADDRESS_ERROR_RESPONSE;
}

void pci::end_of_elaboration() {
    cleanup_virtqueues();

    m_drv_features = 0ull;
    m_dev_features = 0ull;

    m_device_desc.reset();

    if (m_shm) {
        m_shm->reset();
        m_device_desc.shm_capacity = m_shm->capacity();
    }

    virtio_out->identify(m_device_desc);

    if (m_shm && !m_device_desc.shmems.empty()) {
        for (auto& [shmid, desc] : m_device_desc.shmems) {
            if (m_shm->request(shmid, desc.capacity)) {
                u64 base = m_shm->region_base(shmid);
                u64 size = m_shm->region_size(shmid);
                virtio_declare_shm_cap(shm_bar, base, size, shmid);
            }
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

    pci_class = pci_class_code(m_device_desc.pci_class, 1);
    pci_device_id = PCI_DEVICE_VIRTIO + m_device_desc.device_id;
    pci_subvendor_id = m_device_desc.vendor_id;
    pci_subdevice_id = m_device_desc.device_id;
    num_queues = m_device_desc.virtqueues.size();
}

void pci::write_device_feature_sel(u32 val) {
    unsigned int shift = val ? 32 : 0;
    device_feature = (u32)(m_dev_features >> shift);
    device_feature_sel = val ? 1 : 0;
}

void pci::write_driver_feature(u32 val) {
    if (device_status & VIRTIO_STATUS_FEATURES_OK) {
        log_warn("attempt to change features after negotiation");
        device_status = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
        return;
    }

    if (driver_feature_sel) {
        m_drv_features &= bitmask(32, 0);
        m_drv_features |= (u64)val << 32;
    } else {
        m_drv_features &= bitmask(32, 32);
        m_drv_features |= (u64)val;
    }

    driver_feature = val;
}

void pci::write_device_status(u8 val) {
    val &= VIRTIO_STATUS_MASK;

    if (val == 0u) {
        log_debug("software triggered virtio device reset");
        reset_device();
        return;
    }

    if (popcnt(val ^ device_status) > 1)
        log_warn("multiple status bits changed at once");
    if (popcnt(device_status) > popcnt(val))
        log_warn("attempt to clear individual status bits");

    device_status = val;

    if (virtio_feature_check(val)) {
        if (m_drv_features & ~m_dev_features)
            device_status &= ~VIRTIO_STATUS_FEATURES_OK;
        else if (!virtio_out->write_features(m_drv_features))
            device_status &= ~VIRTIO_STATUS_FEATURES_OK;
    }
}

u16 pci::read_queue_size() {
    auto it = m_device_desc.virtqueues.find(queue_sel);
    if (it == m_device_desc.virtqueues.end())
        return 0;
    return it->second.size;
}

u16 pci::read_queue_msix_vector() {
    auto it = m_device_desc.virtqueues.find(queue_sel);
    if (it == m_device_desc.virtqueues.end())
        return 0;
    return it->second.vector;
}

u16 pci::read_queue_enable() {
    auto it = m_queues.find(queue_sel);
    return it == m_queues.end() ? 0 : 1;
}

u16 pci::read_queue_notify_off() {
    return 0;
}

u64 pci::read_queue_desc() {
    auto it = m_device_desc.virtqueues.find(queue_sel);
    if (it == m_device_desc.virtqueues.end())
        return 0;
    return it->second.desc;
}

u64 pci::read_queue_driver() {
    auto it = m_device_desc.virtqueues.find(queue_sel);
    if (it == m_device_desc.virtqueues.end())
        return 0;
    return it->second.driver;
}

u64 pci::read_queue_device() {
    auto it = m_device_desc.virtqueues.find(queue_sel);
    if (it == m_device_desc.virtqueues.end())
        return 0;
    return it->second.device;
}

void pci::write_queue_size(u16 val) {
    u32 vqid = queue_sel;
    auto it = m_device_desc.virtqueues.find(vqid);
    if (it == m_device_desc.virtqueues.end()) {
        log_warn("programming size of invalid virtqueue %u", vqid);
        return;
    }

    virtio_queue_desc& qdesc = it->second;
    if (val > qdesc.limit) {
        log_warn("virtqueue %u size %hu exceeds limit %u, truncating", vqid,
                 val, qdesc.limit);
        val = qdesc.limit;
    }

    queue_size = qdesc.size = val;
}

void pci::write_queue_msix_vector(u16 val) {
    u32 vqid = queue_sel;
    auto it = m_device_desc.virtqueues.find(vqid);
    if (it == m_device_desc.virtqueues.end()) {
        log_warn("programming MSIX vector of invalid virtqueue %u", vqid);
        return;
    }

    virtio_queue_desc& qdesc = it->second;
    queue_msix_vector = qdesc.vector = val;
}

void pci::write_queue_enable(u16 val) {
    if ((queue_enable = val))
        enable_virtqueue(queue_sel);
    else
        disable_virtqueue(queue_sel);
}

void pci::write_queue_notify_off(u16 val) {
    if (val != 0)
        log_warn("nonzero notification offset not supported");
}

void pci::write_queue_desc(u64 val) {
    u32 vqid = queue_sel;
    auto it = m_device_desc.virtqueues.find(vqid);
    if (it == m_device_desc.virtqueues.end()) {
        log_warn("programming descriptors of invalid virtqueue %u", vqid);
        return;
    }

    virtio_queue_desc& qdesc = it->second;
    queue_desc = qdesc.desc = val;
}

void pci::write_queue_driver(u64 val) {
    u32 vqid = queue_sel;
    auto it = m_device_desc.virtqueues.find(vqid);
    if (it == m_device_desc.virtqueues.end()) {
        log_warn("programming driver mem of invalid virtqueue %u", vqid);
        return;
    }

    virtio_queue_desc& qdesc = it->second;
    queue_driver = qdesc.driver = val;
}

void pci::write_queue_device(u64 val) {
    u32 vqid = queue_sel;
    auto it = m_device_desc.virtqueues.find(vqid);
    if (it == m_device_desc.virtqueues.end()) {
        log_warn("programming device mem of invalid virtqueue %u", vqid);
        return;
    }

    virtio_queue_desc& qdesc = it->second;
    queue_device = qdesc.device = val;
}

void pci::write_queue_reset(u16 val) {
    if (!has_feature(VIRTIO_F_RING_RESET))
        log_warn("attempt to reset virtqueue without VIRTIO_F_RING_RESET");

    if (val == 1) {
        queue_reset = 1;
        reset_virtqueue(queue_sel);
        queue_reset = 0;
    }
}
void pci::write_queue_notify(u32 val) {
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

    queue_notify = val;
    log_debug("notifying virtqueue %u", vqid);
    if (!virtio_out->notify(vqid)) {
        log_warn("notify: device reported failure");
        device_status = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
    }
}

u32 pci::read_irq_status() {
    u32 val = irq_status;
    irq_status = 0;

    if (val & VIRTIO_IRQSTATUS_CONFIG)
        pci_interrupt(false, msix_config);
    if (val & VIRTIO_IRQSTATUS_VQUEUE) {
        for (auto it : m_queues) {
            if (!(msix_enabled() || msi_enabled()) ||
                it.second->vector != VIRTIO_NO_VECTOR) {
                pci_interrupt(false, it.second->vector);
            }
        }
    }

    return val;
}

pci::pci(const sc_module_name& nm):
    pci::device(nm, VIRTIO_PCIE_CONFIG),
    virtio_controller(),
    m_drv_features(),
    m_dev_features(),
    m_device_desc(),
    m_queues(),
    m_shm(),
    m_cap_common(),
    m_cap_notify(),
    m_cap_isr(),
    m_cap_device(),
    m_cap_shm(),
    use_packed_queues("use_packed_queues", false),
    use_strong_barriers("use_strong_barriers", false),
    msix_vectors("msix_vectors", 16),
    virtio_bar("virtio_bar", 0),
    msix_bar("msix_bar", 2),
    shm_bar("shm_bar", 4),
    shm_size("shm_size", 0),
    device_feature_sel(virtio_as(), "device_feature_sel", 0x00),
    device_feature(virtio_as(), "device_feature", 0x04),
    driver_feature_sel(virtio_as(), "driver_feature_sel", 0x08),
    driver_feature(virtio_as(), "driver_feature", 0x0c),
    msix_config(virtio_as(), "msix_config", 0x10, VIRTIO_NO_VECTOR),
    num_queues(virtio_as(), "num_queues", 0x12),
    device_status(virtio_as(), "device_status", 0x14),
    config_gen(virtio_as(), "config_gen", 0x15),
    queue_sel(virtio_as(), "queue_sel", 0x16, VIRTQUEUE_MAX),
    queue_size(virtio_as(), "queue_size", 0x18),
    queue_msix_vector(virtio_as(), "queue_msix_vector", 0x1a),
    queue_enable(virtio_as(), "queue_enable", 0x1c),
    queue_notify_off(virtio_as(), "queue_notify_off", 0x1e),
    queue_desc(virtio_as(), "queue_desc", 0x20),
    queue_driver(virtio_as(), "queue_driver", 0x28),
    queue_device(virtio_as(), "queue_device", 0x30),
    queue_notif_config_data(virtio_as(), "queue_notif_config_data", 0x38),
    queue_reset(virtio_as(), "queue_reset", 0x3a),
    queue_notify(virtio_as(), "queue_notify", 0x1000),
    irq_status(virtio_as(), "irq_status", 0x2000),
    pci_in("pci_in"),
    virtio_out("virtio_out") {
    device_feature_sel.sync_never();
    device_feature_sel.allow_read_write();
    device_feature_sel.on_write(&pci::write_device_feature_sel);

    device_feature.sync_never();
    device_feature.allow_read_only();

    driver_feature_sel.sync_never();
    driver_feature_sel.allow_read_write();

    driver_feature.sync_never();
    driver_feature.allow_read_write();
    driver_feature.on_write(&pci::write_driver_feature);

    msix_config.sync_never();
    msix_config.allow_read_write();

    num_queues.sync_never();
    num_queues.allow_read_only();

    device_status.sync_always();
    device_status.allow_read_write();
    device_status.on_write(&pci::write_device_status);

    config_gen.sync_always();
    config_gen.allow_read_only();

    queue_sel.sync_never();
    queue_sel.allow_read_write();

    queue_size.sync_never();
    queue_size.allow_read_write();
    queue_size.on_read(&pci::read_queue_size);
    queue_size.on_write(&pci::write_queue_size);

    queue_msix_vector.sync_never();
    queue_msix_vector.allow_read_write();
    queue_msix_vector.on_read(&pci::read_queue_msix_vector);
    queue_msix_vector.on_write(&pci::write_queue_msix_vector);

    queue_enable.sync_never();
    queue_enable.allow_read_write();
    queue_enable.on_read(&pci::read_queue_enable);
    queue_enable.on_write(&pci::write_queue_enable);

    queue_notify_off.sync_never();
    queue_notify_off.allow_read_write();
    queue_notify_off.on_read(&pci::read_queue_notify_off);
    queue_notify_off.on_write(&pci::write_queue_notify_off);

    queue_desc.sync_never();
    queue_desc.allow_read_write();
    queue_desc.on_read(&pci::read_queue_desc);
    queue_desc.on_write(&pci::write_queue_desc);

    queue_driver.sync_never();
    queue_driver.allow_read_write();
    queue_driver.on_read(&pci::read_queue_driver);
    queue_driver.on_write(&pci::write_queue_driver);

    queue_device.sync_never();
    queue_device.allow_read_write();
    queue_device.on_read(&pci::read_queue_device);
    queue_device.on_write(&pci::write_queue_device);

    queue_notif_config_data.sync_never();
    queue_notif_config_data.allow_read_write();

    queue_reset.sync_always();
    queue_reset.allow_read_write();
    queue_reset.on_write(&pci::write_queue_reset);

    queue_notify.sync_always();
    queue_notify.allow_read_write();
    queue_notify.on_write(&pci::write_queue_notify);

    irq_status.sync_always();
    irq_status.allow_read_write();
    irq_status.on_read(&pci::read_irq_status);

    pci_declare_bar(virtio_bar, 0x4000, PCI_BAR_MMIO | PCI_BAR_64);

    virtio_declare_common_cap(virtio_bar, 0x0000, 0x1000);
    virtio_declare_notify_cap(virtio_bar, 0x1000, 0x1000, 0);
    virtio_declare_isr_cap(virtio_bar, 0x2000, 0x1000);
    virtio_declare_device_cap(virtio_bar, 0x3000, 0x1000);

    if (msix_vectors) {
        pci_declare_bar(msix_bar, 0x1000, PCI_BAR_MMIO);
        pci_declare_msix_cap(msix_bar, msix_vectors, 0);
    }

    if (shm_size > 0) {
        m_shm = new virtio_shared_memory(shm_size);
        pci_declare_bar(shm_bar, shm_size, PCI_BAR_64 | PCI_BAR_PREFETCH);
    }
}

pci::~pci() {
    cleanup_virtqueues();

    if (m_cap_common)
        delete m_cap_common;
    if (m_cap_notify)
        delete m_cap_notify;
    if (m_cap_isr)
        delete m_cap_isr;
    if (m_cap_device)
        delete m_cap_device;
    for (auto* cap_shm : m_cap_shm)
        delete cap_shm;
    if (m_shm)
        delete m_shm;
}

void pci::reset() {
    pci::device::reset();
    reset_device();

    pci_class = pci_class_code(m_device_desc.pci_class, 1);
    pci_device_id = PCI_DEVICE_VIRTIO + m_device_desc.device_id;
    pci_subvendor_id = m_device_desc.vendor_id;
    pci_subdevice_id = m_device_desc.device_id;
    num_queues = m_device_desc.virtqueues.size();
}

void pci::virtio_declare_common_cap(u8 bar, u32 offset, u32 length) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_cap_common, "common capability already declared");
    VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
    m_cap_common = new cap_virtio("pci_cap_virtio_common",
                                  VIRTIO_PCI_CAP_COMMON, bar, offset, length);
}

void pci::virtio_declare_notify_cap(u8 bar, u32 off, u32 len, u32 mult) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_cap_notify, "notify capability already declared");
    VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
    m_cap_notify = new cap_virtio("pci_cap_virtio_notify",
                                  VIRTIO_PCI_CAP_NOTIFY, bar, off, len, mult);
}

void pci::virtio_declare_isr_cap(u8 bar, u32 offset, u32 length) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_cap_isr, "isr capability already declared");
    VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
    m_cap_isr = new cap_virtio("pci_cap_virtio_isr", VIRTIO_PCI_CAP_ISR, bar,
                               offset, length);
}

void pci::virtio_declare_device_cap(u8 bar, u32 offset, u32 length) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_cap_device, "device capability already declared");
    VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
    m_cap_device = new cap_virtio("pci_cap_virtio_device",
                                  VIRTIO_PCI_CAP_DEVICE, bar, offset, length);
}

void pci::virtio_declare_shm_cap(u8 bar, u32 offset, u32 length, u32 shmid) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
    auto* cap = new cap_virtio("pci_cap_virtio_shm", VIRTIO_PCI_CAP_SHM_CFG,
                               bar, offset, length, shmid);
    m_cap_shm.push_back(cap);
}

VCML_EXPORT_MODEL(vcml::virtio::pci, name, args) {
    return new pci(name);
}

} // namespace virtio
} // namespace vcml
