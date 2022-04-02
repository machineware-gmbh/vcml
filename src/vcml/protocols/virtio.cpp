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

#include "vcml/protocols/virtio.h"

namespace vcml {

const char* virtio_status_str(virtio_status status) {
    switch (status) {
    case VIRTIO_INCOMPLETE:
        return "VIRTIO_INCOMPLETE";
    case VIRTIO_OK:
        return "VIRTIO_OK";
    case VIRTIO_ERR_INDIRECT:
        return "VIRTIO_ERR_INDIRECT";
    case VIRTIO_ERR_NODMI:
        return "VIRTIO_ERR_NODMI";
    case VIRTIO_ERR_CHAIN:
        return "VIRTIO_ERR_CHAIN";
    case VIRTIO_ERR_DESC:
        return "VIRTIO_ERR_DESC";
    default:
        return "unknown";
    }
}

size_t vq_message::copy_out(const void* ptr, size_t size, size_t offset) {
    const u8* src = (const u8*)ptr;
    size_t copied = 0;

    for (auto buf : out) {
        if (offset >= buf.size) {
            offset -= buf.size;
            continue;
        }

        size_t n = min(size, buf.size - offset);
        u8* dest = dmi(buf.addr + offset, n, VCML_ACCESS_WRITE);
        VCML_ERROR_ON(!dest, "no DMI pointer for 0x%016lx", buf.addr);

        offset = 0;

        memcpy(dest, src, n);

        copied += n;
        size -= n;
        src += n;

        if (size == 0u)
            break;
    }

    return copied;
}

size_t vq_message::copy_in(void* ptr, size_t size, size_t offset) {
    u8* dest      = (u8*)ptr;
    size_t copied = 0;

    for (auto buf : in) {
        if (offset >= buf.size) {
            offset -= buf.size;
            continue;
        }

        size_t n      = min(size, buf.size - offset);
        const u8* src = dmi(buf.addr + offset, n, VCML_ACCESS_READ);
        VCML_ERROR_ON(!src, "no DMI pointer for 0x%016lx", buf.addr);

        offset = 0;

        memcpy(dest, src, n);

        copied += n;
        size -= n;
        dest += n;

        if (size == 0u)
            break;
    }

    return copied;
}

#define HEX(x)                                                                \
    "0x" << std::setfill('0') << std::setw(x > ~0u ? 16 : 8) << std::hex << x \
         << std::dec << std::setfill(' ')

ostream& operator<<(ostream& os, const vq_message& msg) {
    stream_guard guard(os);
    os << "VIRTMSG@" << msg.index << " [";
    if (!msg.in.empty()) {
        os << "in: " << msg.in.size() << " descriptors | " << msg.length_in()
           << " bytes total";
    }

    if (!msg.in.empty() && !msg.out.empty())
        os << " ";

    if (!msg.out.empty()) {
        os << "out: " << msg.out.size() << " descriptors | "
           << msg.length_out() << " bytes total";
    }

    os << "] (" << virtio_status_str(msg.status) << ")";

    for (auto in : msg.in) {
        os << "\n  IN [" << HEX(in.addr) << ".." << HEX(in.addr + in.size - 1)
           << "] (" << in.size << " bytes)";
    }

    for (auto out : msg.out) {
        os << "\n  OUT [" << HEX(out.addr) << ".."
           << HEX(out.addr + out.size - 1) << "] (" << out.size << " bytes)";
    }

    return os;
}

virtqueue::virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi):
    sc_object(mkstr("VQ%u", desc.id).c_str()),
    id(desc.id),
    limit(desc.limit),
    size(desc.size),
    addr_desc(desc.desc),
    addr_driver(desc.driver),
    addr_device(desc.device),
    has_event_idx(desc.has_event_idx),
    notify(false),
    vector(VIRTIO_NO_VECTOR),
    dmi(std::move(dmi)),
    parent(hierarchy_search<module>()),
    log(this) {
    VCML_ERROR_ON(!parent, "virtqueue created outside module");
}

virtqueue::~virtqueue() {
    // nothing to do
}

bool virtqueue::get(vq_message& msg) {
    msg.dmi    = dmi;
    msg.status = VIRTIO_INCOMPLETE;
    msg.index  = -1;
    msg.in.clear();
    msg.out.clear();

    if (!validate())
        return false;

    msg.status = do_get(msg);

    if (msg.status == VIRTIO_INCOMPLETE)
        return false;

    parent->record(TRACE_FW_NOINDENT, *this, msg);
    return success(msg);
}

bool virtqueue::put(vq_message& msg) {
    if (!validate())
        return false;

    parent->record(TRACE_BW_NOINDENT, *this, msg);
    msg.status = do_put(msg);
    return success(msg);
}

split_virtqueue::split_virtqueue(const virtio_queue_desc& queue_desc,
                                 virtio_dmifn dmifn):
    virtqueue(queue_desc, std::move(dmifn)),
    m_last_avail_idx(0),
    m_desc(nullptr),
    m_avail(nullptr),
    m_used(nullptr),
    m_used_ev(nullptr),
    m_avail_ev(nullptr) {
    if (!addr_desc || !addr_driver || !addr_device)
        log_warn("invalid virtqueue ring addresses");
}

split_virtqueue::~split_virtqueue() {
    // nothing to do
}

bool split_virtqueue::validate() {
    if (m_desc && m_avail && m_used)
        return true;

    if (m_desc == nullptr)
        m_desc = (vq_desc*)dmi(addr_desc, descsz(), VCML_ACCESS_READ);

    if (m_avail == nullptr)
        m_avail = (vq_avail*)dmi(addr_driver, drvsz(), VCML_ACCESS_READ);

    if (m_used == nullptr)
        m_used = (vq_used*)dmi(addr_device, devsz(), VCML_ACCESS_WRITE);

    if (!m_desc || !m_avail || !m_used) {
        log_warn("failed to get virtqueue DMI pointers");
        log_warn("  descriptors at 0x%lx -> %p", addr_desc, m_desc);
        log_warn("  driver ring at 0x%lx -> %p", addr_driver, m_avail);
        log_warn("  device ring at 0x%lx -> %p", addr_device, m_used);
        return false;
    }

    if (has_event_idx) {
        m_used_ev  = (u16*)(m_avail->ring + size);
        m_avail_ev = (u16*)(m_used->ring + size);
    }

    log_debug("created split virtqueue %u with size %u", id, limit);
    log_debug("  descriptors at 0x%lx -> %p", addr_desc, m_desc);
    log_debug("  driver ring at 0x%lx -> %p", addr_driver, m_avail);
    log_debug("  device ring at 0x%lx -> %p", addr_device, m_used);

    return m_desc && m_avail && m_used;
}

void split_virtqueue::invalidate(const range& mem) {
    const range desc(addr_desc, addr_desc + descsz() - 1);
    const range driver(addr_driver, addr_driver + drvsz() - 1);
    const range device(addr_device, addr_device + devsz() - 1);

    if (mem.overlaps(desc))
        m_desc = nullptr;
    if (mem.overlaps(driver))
        m_avail = nullptr;
    if (mem.overlaps(device))
        m_used = nullptr;
}

virtio_status split_virtqueue::do_get(vq_message& msg) {
    if (m_last_avail_idx == m_avail->idx)
        return VIRTIO_INCOMPLETE;

    msg.index = m_avail->ring[m_last_avail_idx++ % size];
    if (msg.index >= size) {
        log_warn("illegal descriptor index %u", msg.index);
        return VIRTIO_ERR_DESC;
    }

    if (m_avail_ev)
        *m_avail_ev = m_last_avail_idx;

    u32 count = 0;
    u32 limit = size;

    vq_desc* base = m_desc;
    vq_desc* desc = base + msg.index;

    if (desc->is_indirect()) {
        if (!desc->len || desc->len % sizeof(vq_desc)) {
            log_warn("broken indirect descriptor");
            return VIRTIO_ERR_INDIRECT;
        }

        limit = desc->len / sizeof(vq_desc);
        desc = base = (vq_desc*)lookup_desc_ptr(desc);

        if (!desc) {
            log_warn("cannot access indirect descriptor");
            return VIRTIO_ERR_INDIRECT;
        }
    }

    while (true) {
        if (lookup_desc_ptr(desc) == nullptr) {
            log_warn(
                "cannot get DMI pointer for descriptor at address "
                "0x%016lx",
                desc->addr);
            return VIRTIO_ERR_NODMI;
        }

        if (!desc->is_write() && msg.length_out() > 0)
            log_warn("invalid descriptor order");

        msg.append(desc->addr, desc->len, desc->is_write());

        if (!desc->is_chained())
            return VIRTIO_OK;

        if (desc->next >= size) {
            log_warn("broken descriptor chain");
            return VIRTIO_ERR_CHAIN;
        }

        if (count++ >= limit) {
            log_warn("descriptor chain too long");
            return VIRTIO_ERR_CHAIN;
        }

        desc = base + desc->next;
    }
}

virtio_status split_virtqueue::do_put(vq_message& msg) {
    notify = false;

    if (msg.index >= size) {
        log_warn("index out of bounds: %u", msg.index);
        return VIRTIO_ERR_DESC;
    }

    if ((m_used_ev && *m_used_ev == m_used->idx) || !m_avail->no_irq())
        notify = true;

    m_used->ring[m_used->idx % size].id  = msg.index;
    m_used->ring[m_used->idx % size].len = msg.length();
    m_used->idx++;

    return VIRTIO_OK;
}

packed_virtqueue::packed_virtqueue(const virtio_queue_desc& queue_desc,
                                   virtio_dmifn dmifn):
    virtqueue(queue_desc, std::move(dmifn)),
    m_last_avail_idx(0),
    m_desc(nullptr),
    m_driver(nullptr),
    m_device(nullptr),
    m_wrap_get(true),
    m_wrap_put(true) {
    if (!addr_desc || !addr_driver || !addr_device)
        log_warn("invalid virtqueue ring addresses");
}

packed_virtqueue::~packed_virtqueue() {
    // nothing to do
}

bool packed_virtqueue::validate() {
    if (m_desc && ((m_driver && m_device) || !has_event_idx))
        return true;

    if (!m_desc)
        m_desc = (vq_desc*)dmi(addr_desc, dscsz(), VCML_ACCESS_READ_WRITE);

    if (!m_driver && has_event_idx)
        m_driver = (vq_event*)dmi(addr_driver, drvsz(), VCML_ACCESS_READ);

    if (!m_device && has_event_idx)
        m_device = (vq_event*)dmi(addr_device, devsz(), VCML_ACCESS_WRITE);

    if (!m_desc || (has_event_idx && (!m_driver || !m_device))) {
        log_warn("failed to get DMI pointers for packed virtqueue");
        log_warn("  descriptors at 0x%lx -> %p", addr_desc, m_desc);

        if (!has_event_idx)
            return false;

        log_warn("  driver events at 0x%lx -> %p", addr_driver, m_driver);
        log_warn("  device events at 0x%lx -> %p", addr_device, m_device);
        return false;
    }

    log_debug("created packed virtqueue %u with size %u", id, limit);
    log_debug("  descriptors at 0x%lx -> %p", addr_desc, m_desc);

    if (m_driver)
        log_debug("  driver events at 0x%lx -> %p", addr_driver, m_driver);
    if (m_device)
        log_debug("  device events at 0x%lx -> %p", addr_device, m_device);

    return true;
}

void packed_virtqueue::invalidate(const range& mem) {
    const range desc(addr_desc, addr_desc + dscsz() - 1);
    const range driver(addr_driver, addr_driver + drvsz() - 1);
    const range device(addr_device, addr_device + devsz() - 1);

    if (mem.overlaps(desc))
        m_desc = nullptr;
    if (mem.overlaps(driver))
        m_driver = nullptr;
    if (mem.overlaps(device))
        m_device = nullptr;
}

virtio_status packed_virtqueue::do_get(vq_message& msg) {
    vq_desc* base = m_desc;
    vq_desc* desc = base + m_last_avail_idx;

    if (!desc->is_avail(m_wrap_get) || desc->is_used(m_wrap_get))
        return VIRTIO_INCOMPLETE;

    msg.index = m_last_avail_idx;

    u32 count = 0;
    u32 limit = size;
    u32 index = msg.index;

    bool indirect = desc->is_indirect();
    if (indirect) {
        if (!desc->len || desc->len % sizeof(vq_desc)) {
            log_warn("broken indirect descriptor");
            return VIRTIO_ERR_INDIRECT;
        }

        index = 0;
        limit = desc->len / sizeof(vq_desc);
        desc = base = (vq_desc*)lookup_desc_ptr(desc);

        if (!desc) {
            log_warn("cannot access indirect descriptor");
            return VIRTIO_ERR_INDIRECT;
        }
    }

    while (true) {
        if (!desc->is_avail(m_wrap_get) || desc->is_used(m_wrap_get)) {
            log_warn("descriptor not available");
            return VIRTIO_ERR_DESC;
        }

        if (lookup_desc_ptr(desc) == nullptr) {
            log_warn(
                "cannot get DMI pointer for descriptor at address "
                "0x%016lx",
                desc->addr);
            return VIRTIO_ERR_NODMI;
        }

        if (!desc->is_write() && msg.length_out() > 0)
            log_warn("invalid descriptor order");

        msg.append(desc->addr, desc->len, desc->is_write());

        if (count++ >= limit) {
            log_warn("descriptor chain too long");
            return VIRTIO_ERR_CHAIN;
        }

        index++;
        if (index >= limit) {
            index -= limit;
            m_wrap_get = !m_wrap_get;
            VCML_ERROR_ON(indirect, "indirect descriptors must not wrap");
        }

        if (!desc->is_chained())
            break;

        desc = base + index;
    }

    m_last_avail_idx += indirect ? 1 : msg.ndescs();
    if (m_last_avail_idx >= size)
        m_last_avail_idx -= size;

    return VIRTIO_OK;
}

virtio_status packed_virtqueue::do_put(vq_message& msg) {
    u32 count = 0;
    u32 index = msg.index;
    u32 limit = size;

    vq_desc* base = m_desc;
    vq_desc* desc = base + index;

    notify = m_driver->should_notify(index);

    if (desc->is_indirect()) {
        if (!desc->len || desc->len % sizeof(vq_desc)) {
            log_warn("broken indirect descriptor");
            return VIRTIO_ERR_DESC;
        }

        index = 0;
        limit = desc->len / sizeof(vq_desc);
        desc = base = (vq_desc*)lookup_desc_ptr(desc);

        if (!desc) {
            log_warn("cannot access indirect descriptor");
            return VIRTIO_ERR_INDIRECT;
        }
    }

    while (true) {
        desc->mark_used(m_wrap_put);

        if (count++ >= limit) {
            log_warn("descriptor chain too long");
            return VIRTIO_ERR_CHAIN;
        }

        index++;
        if (index >= limit) {
            index -= limit;
            m_wrap_put = !m_wrap_put;
        }

        if (!desc->is_chained())
            return VIRTIO_OK;

        desc = base + index;
    }
}

bool virtio_initiator_socket::put(u32 vqid, vq_message& msg) {
    return m_controller->put(vqid, msg); // virtqueue handles tracing
}

bool virtio_initiator_socket::get(u32 vqid, vq_message& msg) {
    return m_controller->get(vqid, msg); // virtqueue handles tracing
}

bool virtio_initiator_socket::notify() {
    return m_controller->notify();
}

virtio_initiator_socket::virtio_initiator_socket(const char* nm):
    virtio_base_initiator_socket(nm, VCML_AS_DEFAULT),
    virtio_bw_transport_if(),
    m_parent(hierarchy_search<module>()),
    m_controller(hierarchy_search<virtio_controller>()),
    m_stub(nullptr) {
    VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
    VCML_ERROR_ON(!m_controller, "%s has no virtio_controller", name());
    bind(*(virtio_bw_transport_if*)this);
}

virtio_initiator_socket::~virtio_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

sc_type_index virtio_initiator_socket::get_protocol_types() const {
    return typeid(virtio_initiator_socket);
}

void virtio_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
    hierarchy_guard guard(m_parent);
    m_stub = new virtio_target_stub(mkstr("%s_stub", basename()).c_str());
    bind(m_stub->virtio_in);
}

void virtio_target_socket::identify(virtio_device_desc& desc) {
    m_device->identify(desc);
}

bool virtio_target_socket::notify(u32 vqid) {
    return m_device->notify(vqid);
}

void virtio_target_socket::read_features(u64& features) {
    m_device->read_features(features);
}

bool virtio_target_socket::write_features(u64 features) {
    return m_device->write_features(features);
}

bool virtio_target_socket::read_config(const range& addr, void* data) {
    return m_device->read_config(addr, data);
}

bool virtio_target_socket::write_config(const range& addr, const void* p) {
    return m_device->write_config(addr, p);
}

virtio_target_socket::virtio_target_socket(const char* nm):
    virtio_base_target_socket(nm, VCML_AS_DEFAULT),
    m_parent(hierarchy_search<module>()),
    m_device(hierarchy_search<virtio_device>()),
    m_stub(nullptr) {
    VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
    VCML_ERROR_ON(!m_device, "%s has no virtio_device", name());
    bind(*(virtio_fw_transport_if*)this);
}

virtio_target_socket::~virtio_target_socket() {
    if (m_stub)
        delete m_stub;
}

sc_type_index virtio_target_socket::get_protocol_types() const {
    return typeid(virtio_target_socket);
}

void virtio_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
    hierarchy_guard guard(m_parent);
    m_stub = new virtio_initiator_stub(mkstr("%s_stub", basename()).c_str());
    m_stub->virtio_out.bind(*this);
}

bool virtio_initiator_stub::put(u32 vqid, vq_message& msg) {
    (void)vqid;
    (void)msg;
    return false;
}

bool virtio_initiator_stub::get(u32 vqid, vq_message& msg) {
    (void)vqid;
    (void)msg;
    return false;
}

bool virtio_initiator_stub::notify() {
    return false;
}

virtio_initiator_stub::virtio_initiator_stub(const sc_module_name& nm):
    module(nm), virtio_controller(), virtio_out("virtio_out") {
}

virtio_initiator_stub::~virtio_initiator_stub() {
    // nothing to do
}

void virtio_target_stub::identify(virtio_device_desc& desc) {
    desc.device_id = VIRTIO_DEVICE_NONE;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
}

bool virtio_target_stub::notify(u32 vqid) {
    (void)vqid;
    return false;
}

void virtio_target_stub::read_features(u64& features) {
    features = 0;
}

bool virtio_target_stub::write_features(u64 features) {
    (void)features;
    return false;
}

bool virtio_target_stub::read_config(const range& addr, void* ptr) {
    (void)addr;
    (void)ptr;
    return false;
}

bool virtio_target_stub::write_config(const range& addr, const void* ptr) {
    (void)addr;
    (void)ptr;
    return false;
}

virtio_target_stub::virtio_target_stub(const sc_module_name& nm):
    module(nm), virtio_device(), virtio_in("virtio_in") {
}

virtio_target_stub::~virtio_target_stub() {
    // nothing to do
}

} // namespace vcml
