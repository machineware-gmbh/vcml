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

#include "vcml/models/virtio/pci.h"

namespace vcml { namespace virtio {

    pci_cap_virtio::pci_cap_virtio(const string& nm, u8 type, u8 bar,
        u32 offset, u32 length, u32 mult):
        pci_capability(nm, PCI_CAPABILITY_VENDOR),
        CAP_LEN(),
        CFG_TYPE(),
        BAR(),
        OFFSET(),
        LENGTH(),
        NOTIFY_MULT() {
        u8 size = type == VIRTIO_PCI_CAP_NOTIFY ? 20 : 16;
        CAP_LEN = new_cap_reg_ro<u8>("CAP_LEN", size);
        CFG_TYPE = new_cap_reg_ro<u8>("CFG_TYPE", type);
        BAR = new_cap_reg_ro<u8>("BAR", bar);
        device->pci_cap_off += 3; // align
        OFFSET = new_cap_reg_ro<u32>("OFFSET", offset);
        LENGTH = new_cap_reg_ro<u32>("LENGTH", length);
        if (type == VIRTIO_PCI_CAP_NOTIFY)
            NOTIFY_MULT = new_cap_reg_ro<u32>("NOTIFY_MULT", mult);
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
        virtio_dmifn dmifn = [=] (u64 addr, u64 len, vcml_access rw) -> u8* {
            return (u8*)PCI_IN->pci_dma_ptr(rw, addr, len);
        };

        hierarchy_guard guard(this);

        virtqueue* q;
        if (has_feature(VIRTIO_F_RING_PACKED))
            q = m_queues[vqid] = new packed_virtqueue(qd, dmifn);
        else
            q = m_queues[vqid] = new split_virtqueue(qd, dmifn);

        if (!q->validate()) {
            log_warn("failed to enable virtqueue %u", vqid);
            DEVICE_STATUS = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
        }
    }

    void pci::disable_virtqueue(u32 vqid) {
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

    void pci::cleanup_virtqueues() {
        for (auto it : m_queues)
            delete it.second;
        m_queues.clear();
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
        if (result && q->notify) {
            IRQ_STATUS |= VIRTIO_IRQSTATUS_VQUEUE;
            pci_interrupt(true, q->vector);
        }

        return result;
    }

    bool pci::notify() {
        if (!device_ready()) {
            log_warn("configuration change notification while inactive");
            return false;
        }

        IRQ_STATUS |= VIRTIO_IRQSTATUS_CONFIG;
        pci_interrupt(true, MSIX_CONFIG);
        return true;
    }

    tlm_response_status pci::read(const range& addr, void* data,
        const tlm_sbi& info, address_space as) {
        if (as != virtio_as())
            return TLM_ADDRESS_ERROR_RESPONSE;

        static const range device(0x3000, 0x3fff);
        if (device.includes(addr)) {
            if (!VIRTIO_OUT->read_config(addr - device.start, data))
                return TLM_ADDRESS_ERROR_RESPONSE;
            return TLM_OK_RESPONSE;
        }

        return TLM_ADDRESS_ERROR_RESPONSE;
    }

    tlm_response_status pci::write(const range& addr, const void* data,
        const tlm_sbi& info, address_space as) {
        if (as != virtio_as())
            return TLM_ADDRESS_ERROR_RESPONSE;

        static const range device(0x3000, 0x3fff);
        if (device.includes(addr)) {
            if (!VIRTIO_OUT->write_config(addr - device.start, data))
                return TLM_ADDRESS_ERROR_RESPONSE;
            CONFIG_GEN++;
            return TLM_OK_RESPONSE;
        }

        return TLM_ADDRESS_ERROR_RESPONSE;
    }

    u32 pci::write_DEVICE_FEATURE_SEL(u32 val) {
        unsigned int shift = val ? 32 : 0;
        DEVICE_FEATURE = (u32)(m_dev_features >> shift);
        return val ? 1 : 0;
    }

    u32 pci::write_DRIVER_FEATURE(u32 val) {
        if (IRQ_STATUS & VIRTIO_STATUS_FEATURES_OK) {
            log_warn("attempt to change features after negotiation");
            IRQ_STATUS = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
            return 0;
        }

        if (DRIVER_FEATURE_SEL) {
            m_drv_features &= bitmask(32, 0);
            m_drv_features |= (u64)val << 32;
        } else {
            m_drv_features &= bitmask(32, 32);
            m_drv_features |= (u64)val;
        }

        return val;
    }

    u8 pci::write_DEVICE_STATUS(u8 val) {
        val &= VIRTIO_STATUS_MASK;

        if (val == 0u) {
            log_debug("software triggered virtio device reset");
            cleanup_virtqueues();
            m_drv_features = 0;
            return 0u;
        }

        if (popcnt(val ^ DEVICE_STATUS) > 1)
            log_warn("multiple status bits changed at once");
        if (popcnt(DEVICE_STATUS) > popcnt(val))
            log_warn("attempt to clear individual status bits");

        if (val == VIRTIO_STATUS_FEATURE_CHECK) {
            if (m_drv_features & ~m_dev_features)
                return val & ~VIRTIO_STATUS_FEATURES_OK;
            if (!VIRTIO_OUT->write_features(m_drv_features))
                return val & ~VIRTIO_STATUS_FEATURES_OK;
        }

        return val;
    }

    u16 pci::read_QUEUE_SIZE() {
        auto it = m_device.virtqueues.find(QUEUE_SEL);
        if (it == m_device.virtqueues.end())
            return 0;
        return it->second.size;
    }

    u16 pci::read_QUEUE_MSIX_VECTOR() {
        auto it = m_device.virtqueues.find(QUEUE_SEL);
        if (it == m_device.virtqueues.end())
            return 0;
        return it->second.vector;
    }

    u16 pci::read_QUEUE_ENABLE() {
        auto it = m_queues.find(QUEUE_SEL);
        return it == m_queues.end() ? 0 : 1;
    }

    u16 pci::read_QUEUE_NOTIFY_OFF() {
        return 0;
    }

    u64 pci::read_QUEUE_DESC() {
        auto it = m_device.virtqueues.find(QUEUE_SEL);
        if (it == m_device.virtqueues.end())
            return 0;
        return it->second.desc;
    }

    u64 pci::read_QUEUE_DRIVER() {
        auto it = m_device.virtqueues.find(QUEUE_SEL);
        if (it == m_device.virtqueues.end())
            return 0;
        return it->second.driver;
    }

    u64 pci::read_QUEUE_DEVICE() {
        auto it = m_device.virtqueues.find(QUEUE_SEL);
        if (it == m_device.virtqueues.end())
            return 0;
        return it->second.device;
    }

    u16 pci::write_QUEUE_SIZE(u16 val) {
        u32 vqid = QUEUE_SEL;
        auto it = m_device.virtqueues.find(vqid);
        if (it == m_device.virtqueues.end()) {
            log_warn("programming size of invalid virtqueue %u", vqid);
            return 0;
        }

        virtio_queue_desc& qdesc = it->second;
        if (val > qdesc.limit) {
            log_warn("virtqueue %u size %hu exceeds limit %u, truncating",
                     vqid, val, qdesc.limit);
            val = qdesc.limit;
        }

        return qdesc.size = val;
    }

    u16 pci::write_QUEUE_MSIX_VECTOR(u16 val) {
        u32 vqid = QUEUE_SEL;
        auto it = m_device.virtqueues.find(vqid);
        if (it == m_device.virtqueues.end()) {
            log_warn("programming MSIX vector of invalid virtqueue %u", vqid);
            return 0;
        }

        virtio_queue_desc& qdesc = it->second;
        return qdesc.vector = val;
    }

    u16 pci::write_QUEUE_ENABLE(u16 val) {
        if (val >= VIRTQUEUE_MAX) {
            log_warn("illegal virtqueue id %hu", val);
            return val;
        }

        if (val)
            enable_virtqueue(QUEUE_SEL);
        else
            disable_virtqueue(QUEUE_SEL);

        return val;
    }

    u16 pci::write_QUEUE_NOTIFY_OFF(u16 val) {
        return 0;
    }

    u64 pci::write_QUEUE_DESC(u64 val) {
        u32 vqid = QUEUE_SEL;
        auto it = m_device.virtqueues.find(vqid);
        if (it == m_device.virtqueues.end()) {
            log_warn("programming descriptors of invalid virtqueue %u", vqid);
            return 0;
        }

        virtio_queue_desc& qdesc = it->second;
        return qdesc.desc = val;
    }

    u64 pci::write_QUEUE_DRIVER(u64 val) {
        u32 vqid = QUEUE_SEL;
        auto it = m_device.virtqueues.find(vqid);
        if (it == m_device.virtqueues.end()) {
            log_warn("programming driver mem of invalid virtqueue %u", vqid);
            return 0;
        }

        virtio_queue_desc& qdesc = it->second;
        return qdesc.driver = val;
    }

    u64 pci::write_QUEUE_DEVICE(u64 val) {
        u32 vqid = QUEUE_SEL;
        auto it = m_device.virtqueues.find(vqid);
        if (it == m_device.virtqueues.end()) {
            log_warn("programming device mem of invalid virtqueue %u", vqid);
            return 0;
        }

        virtio_queue_desc& qdesc = it->second;
        return qdesc.device = val;
    }

    u32 pci::write_QUEUE_NOTIFY(u32 val) {
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
            DEVICE_STATUS = VIRTIO_STATUS_DEVICE_NEEDS_RESET;
        }

        return val;
    }

    u32 pci::read_IRQ_STATUS() {
        u32 val = IRQ_STATUS;
        IRQ_STATUS = 0;
        pci_interrupt(false, VIRTIO_NO_VECTOR);
        return val;
    }

    pci::pci(const sc_module_name& nm):
        pci_device(nm, VIRTIO_PCIE_CONFIG),
        virtio_controller(),
        m_drv_features(),
        m_dev_features(),
        m_device(),
        m_queues(),
        m_cap_common(),
        m_cap_notify(),
        m_cap_isr(),
        m_cap_device(),
        use_packed_queues("use_packed_queues", false),
        use_strong_barriers("use_strong_barriers", false),
        msix_vectors("msix_vectors", 16),
        virtio_bar("virtio_bar", 4),
        msix_bar("msix_bar", 2),
        DEVICE_FEATURE_SEL(virtio_as(), "DEVICE_FEATURE_SEL", 0x00),
        DEVICE_FEATURE    (virtio_as(), "DEVICE_FEATURE",     0x04),
        DRIVER_FEATURE_SEL(virtio_as(), "DRIVER_FEATURE_SEL", 0x08),
        DRIVER_FEATURE    (virtio_as(), "DRIVER_FEATURE",     0x0c),
        MSIX_CONFIG       (virtio_as(), "MSIX_CONFIG", 0x10, VIRTIO_NO_VECTOR),
        NUM_QUEUES        (virtio_as(), "NUM_QUEUES",         0x12),
        DEVICE_STATUS     (virtio_as(), "DEVICE_STATUS",      0x14),
        CONFIG_GEN        (virtio_as(), "CONFIG_GEN",         0x15),
        QUEUE_SEL         (virtio_as(), "QUEUE_SEL",   0x16, VIRTQUEUE_MAX),
        QUEUE_SIZE        (virtio_as(), "QUEUE_SIZE",         0x18),
        QUEUE_MSIX_VECTOR (virtio_as(), "QUEUE_MSIX_VECTOR",  0x1a),
        QUEUE_ENABLE      (virtio_as(), "QUEUE_ENABLE",       0x1c),
        QUEUE_NOTIFY_OFF  (virtio_as(), "QUEUE_NOTIFY_OFF",   0x1e),
        QUEUE_DESC        (virtio_as(), "QUEUE_DESC",         0x20),
        QUEUE_DRIVER      (virtio_as(), "QUEUE_DRIVER",       0x28),
        QUEUE_DEVICE      (virtio_as(), "QUEUE_DEVICE",       0x30),
        QUEUE_NOTIFY      (virtio_as(), "QUEUE_NOTIFY",       0x1000),
        IRQ_STATUS        (virtio_as(), "IRQ_STATUS",         0x2000),
        PCI_IN("PCI_IN"),
        VIRTIO_OUT("VIRTIO_OUT") {

        DEVICE_FEATURE_SEL.sync_never();
        DEVICE_FEATURE_SEL.allow_read_write();
        DEVICE_FEATURE_SEL.on_write(&pci::write_DEVICE_FEATURE_SEL);

        DEVICE_FEATURE.sync_never();
        DEVICE_FEATURE.allow_read_only();

        DRIVER_FEATURE_SEL.sync_never();
        DRIVER_FEATURE_SEL.allow_read_write();

        DRIVER_FEATURE.sync_never();
        DRIVER_FEATURE.allow_read_write();
        DRIVER_FEATURE.on_write(&pci::write_DRIVER_FEATURE);

        MSIX_CONFIG.sync_never();
        MSIX_CONFIG.allow_read_write();

        NUM_QUEUES.sync_never();
        NUM_QUEUES.allow_read_only();

        DEVICE_STATUS.sync_always();
        DEVICE_STATUS.allow_read_write();
        DEVICE_STATUS.on_write(&pci::write_DEVICE_STATUS);

        CONFIG_GEN.sync_always();
        CONFIG_GEN.allow_read_only();

        QUEUE_SEL.sync_never();
        QUEUE_SEL.allow_read_write();

        QUEUE_SIZE.sync_never();
        QUEUE_SIZE.allow_read_write();
        QUEUE_SIZE.on_read(&pci::read_QUEUE_SIZE);
        QUEUE_SIZE.on_write(&pci::write_QUEUE_SIZE);

        QUEUE_MSIX_VECTOR.sync_never();
        QUEUE_MSIX_VECTOR.allow_read_write();
        QUEUE_MSIX_VECTOR.on_read(&pci::read_QUEUE_MSIX_VECTOR);
        QUEUE_MSIX_VECTOR.on_write(&pci::write_QUEUE_MSIX_VECTOR);

        QUEUE_ENABLE.sync_never();
        QUEUE_ENABLE.allow_read_write();
        QUEUE_ENABLE.on_write(&pci::write_QUEUE_ENABLE);

        QUEUE_NOTIFY_OFF.sync_never();
        QUEUE_NOTIFY_OFF.allow_read_write();
        QUEUE_NOTIFY_OFF.on_read(&pci::read_QUEUE_NOTIFY_OFF);
        QUEUE_NOTIFY_OFF.on_write(&pci::write_QUEUE_NOTIFY_OFF);

        QUEUE_DESC.sync_never();
        QUEUE_DESC.allow_read_write();
        QUEUE_DESC.on_read(&pci::read_QUEUE_DESC);
        QUEUE_DESC.on_write(&pci::write_QUEUE_DESC);

        QUEUE_DRIVER.sync_never();
        QUEUE_DRIVER.allow_read_write();
        QUEUE_DRIVER.on_read(&pci::read_QUEUE_DRIVER);
        QUEUE_DRIVER.on_write(&pci::write_QUEUE_DRIVER);

        QUEUE_DEVICE.sync_never();
        QUEUE_DEVICE.allow_read_write();
        QUEUE_DEVICE.on_read(&pci::read_QUEUE_DEVICE);
        QUEUE_DEVICE.on_write(&pci::write_QUEUE_DEVICE);

        QUEUE_NOTIFY.sync_always();
        QUEUE_NOTIFY.allow_read_write();
        QUEUE_NOTIFY.on_write(&pci::write_QUEUE_NOTIFY);

        IRQ_STATUS.sync_always();
        IRQ_STATUS.allow_read_write();
        IRQ_STATUS.on_read(&pci::read_IRQ_STATUS);

        pci_declare_bar(virtio_bar, 0x4000, PCI_BAR_MMIO | PCI_BAR_64);

        virtio_declare_common_cap(virtio_bar, 0x0000, 0x1000);
        virtio_declare_notify_cap(virtio_bar, 0x1000, 0x1000, 0);
        virtio_declare_isr_cap   (virtio_bar, 0x2000, 0x1000);
        virtio_declare_device_cap(virtio_bar, 0x3000, 0x1000);

        if (msix_vectors) {
            pci_declare_bar(msix_bar, 0x1000, PCI_BAR_MMIO);
            pci_declare_msix_cap(msix_bar, msix_vectors, 0);
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
    }

    void pci::reset() {
        pci_device::reset();

        cleanup_virtqueues();

        m_drv_features = 0ull;
        m_dev_features = 0ull;

        m_device.reset();

        VIRTIO_OUT->identify(m_device);
        VIRTIO_OUT->read_features(m_dev_features);

        m_dev_features |= VIRTIO_F_VERSION_1;
        m_dev_features |= VIRTIO_F_RING_EVENT_IDX;
        m_dev_features |= VIRTIO_F_RING_INDIRECT_DESC;

        if (use_packed_queues)
            m_dev_features |= VIRTIO_F_RING_PACKED;

        if (use_strong_barriers)
            m_dev_features |= VIRTIO_F_ORDER_PLATFORM;

        PCI_CLASS = pci_class_code(m_device.pci_class, 1);
        PCI_DEVICE_ID = PCI_DEVICE_VIRTIO + m_device.device_id;
        PCI_SUBVENDOR_ID = m_device.vendor_id;
        PCI_SUBDEVICE_ID = m_device.device_id;
        NUM_QUEUES = m_device.virtqueues.size();
    }

    void pci::virtio_declare_common_cap(u8 bar, u32 offset, u32 length) {
        hierarchy_guard guard(this);
        VCML_ERROR_ON(m_cap_common, "common capability already declared");
        VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
        m_cap_common = new pci_cap_virtio("PCI_CAP_VIRTIO_COMMON",
            VIRTIO_PCI_CAP_COMMON, bar, offset, length, 0);
    }

    void pci::virtio_declare_notify_cap(u8 bar, u32 off, u32 len, u32 mult) {
        hierarchy_guard guard(this);
        VCML_ERROR_ON(m_cap_notify, "notify capability already declared");
        VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
        m_cap_notify = new pci_cap_virtio("PCI_CAP_VIRTIO_NOTIFY",
            VIRTIO_PCI_CAP_NOTIFY, bar, off, len, mult);
    }

    void pci::virtio_declare_isr_cap(u8 bar, u32 offset, u32 length) {
        hierarchy_guard guard(this);
        VCML_ERROR_ON(m_cap_isr, "isr capability already declared");
        VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
        m_cap_isr = new pci_cap_virtio("PCI_CAP_VIRTIO_ISR",
            VIRTIO_PCI_CAP_ISR, bar, offset, length, 0);
    }

    void pci::virtio_declare_device_cap(u8 bar, u32 offset, u32 length) {
        hierarchy_guard guard(this);
        VCML_ERROR_ON(m_cap_device, "device capability already declared");
        VCML_ERROR_ON(bar >= PCI_NUM_BARS, "invalid BAR specified: %hhu", bar);
        m_cap_device = new pci_cap_virtio("PCI_CAP_VIRTIO_DEVICE",
            VIRTIO_PCI_CAP_DEVICE, bar, offset, length, 0);
    }

}}
