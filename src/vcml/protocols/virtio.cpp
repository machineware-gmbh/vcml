/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
        VCML_ERROR_ON(!dest, "no DMI pointer for 0x%016llx", buf.addr);

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
    u8* dest = (u8*)ptr;
    size_t copied = 0;

    for (auto buf : in) {
        if (offset >= buf.size) {
            offset -= buf.size;
            continue;
        }

        size_t n = min(size, buf.size - offset);
        const u8* src = dmi(buf.addr + offset, n, VCML_ACCESS_READ);
        VCML_ERROR_ON(!src, "no DMI pointer for 0x%016llx", buf.addr);

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
        u64 end = in.addr + in.size - 1;
        os << "\n  IN [" << mkstr("0x%0*llx", in.addr > ~0u ? 16 : 8, in.addr)
           << ".." << mkstr("0x%0*llx", end > ~0u ? 16 : 8, end) << "] ("
           << in.size << " bytes)";
    }

    for (auto out : msg.out) {
        u64 end = out.addr + out.size - 1;
        os << "\n  OUT ["
           << mkstr("0x%0*llx", out.addr > ~0u ? 16 : 8, out.addr) << ".."
           << mkstr("0x%0*llx", end > ~0u ? 16 : 8, end) << "] (" << out.size
           << " bytes)";
    }

    return os;
}

virtqueue::virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi):
    sc_object(mkstr("vq%u", desc.id).c_str()),
    id(desc.id),
    limit(desc.limit),
    size(desc.size),
    addr_desc(desc.desc),
    addr_driver(desc.driver),
    addr_device(desc.device),
    has_event_idx(desc.has_event_idx),
    notify(false),
    vector(desc.vector),
    dmi(std::move(dmi)),
    parent(hierarchy_search<module>()),
    log(this) {
    VCML_ERROR_ON(!parent, "virtqueue created outside module");
}

virtqueue::~virtqueue() {
    // nothing to do
}

bool virtqueue::get(vq_message& msg) {
    msg.dmi = dmi;
    msg.status = VIRTIO_INCOMPLETE;
    msg.index = -1;
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
        log_warn("  descriptors at 0x%llx -> %p", addr_desc, m_desc);
        log_warn("  driver ring at 0x%llx -> %p", addr_driver, m_avail);
        log_warn("  device ring at 0x%llx -> %p", addr_device, m_used);
        return false;
    }

    if (has_event_idx) {
        m_used_ev = (u16*)(m_avail->ring + size);
        m_avail_ev = (u16*)(m_used->ring + size);
    }

    log_debug("created split virtqueue %u with size %u", id, limit);
    log_debug("  descriptors at 0x%llx -> %p", addr_desc, m_desc);
    log_debug("  driver ring at 0x%llx -> %p", addr_driver, m_avail);
    log_debug("  device ring at 0x%llx -> %p", addr_device, m_used);

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
        if (!desc->is_write() && msg.length_out() > 0)
            log_warn("invalid descriptor order");

        if (lookup_desc_ptr(desc)) {
            msg.append(desc->addr, desc->len, desc->is_write());
        } else {
            // we could not get a DMI pointer for the entire chunk, so lets
            // split it into smaller regions in case an IOMMU is in between us
            // and the memory. best guess is to use 4k chunks as this seems to
            // be a popular minimal page size
            u64 addr = desc->addr;
            u64 nbytes = 0;
            vcml_access access = desc->is_write() ? VCML_ACCESS_WRITE
                                                  : VCML_ACCESS_READ;
            while (nbytes < desc->len) {
                u64 psz = 4096;
                u64 len = min(psz - (addr & (psz - 1)), desc->len - nbytes);
                u8* ptr = dmi(addr, len, access);
                if (!ptr) {
                    log_error(
                        "cannot get DMI pointer for descriptor at address "
                        "0x%016llx",
                        addr);
                    return VIRTIO_ERR_NODMI;
                };
                msg.append(addr, len, desc->is_write());
                addr += len;
                nbytes += len;
            }
        }

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

    m_used->ring[m_used->idx % size].id = msg.index;
    m_used->ring[m_used->idx % size].len = msg.length_out();
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
        log_warn("  descriptors at 0x%llx -> %p", addr_desc, m_desc);

        if (!has_event_idx)
            return false;

        log_warn("  driver events at 0x%llx -> %p", addr_driver, m_driver);
        log_warn("  device events at 0x%llx -> %p", addr_device, m_device);
        return false;
    }

    log_debug("created packed virtqueue %u with size %u", id, limit);
    log_debug("  descriptors at 0x%llx -> %p", addr_desc, m_desc);

    if (m_driver)
        log_debug("  driver events at 0x%llx -> %p", addr_driver, m_driver);
    if (m_device)
        log_debug("  device events at 0x%llx -> %p", addr_device, m_device);

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

        if (!desc->is_write() && msg.length_out() > 0)
            log_warn("invalid descriptor order");

        if (lookup_desc_ptr(desc)) {
            msg.append(desc->addr, desc->len, desc->is_write());
        } else {
            // we could not get a DMI pointer for the entire chunk, so lets
            // split it into smaller regions in case an IOMMU is in between us
            // and the memory. best guess is to use 4k chunks as this seems to
            // be a popular minimal page size
            u64 addr = desc->addr;
            u64 nbytes = 0;
            vcml_access access = desc->is_write() ? VCML_ACCESS_WRITE
                                                  : VCML_ACCESS_READ;
            while (nbytes < desc->len) {
                u64 psz = 4096;
                u64 len = min(psz - (addr & (psz - 1)), desc->len - nbytes);
                u8* ptr = dmi(addr, len, access);
                if (!ptr) {
                    log_error(
                        "cannot get DMI pointer for descriptor at address "
                        "0x%016llx",
                        addr);
                    return VIRTIO_ERR_NODMI;
                };
                msg.append(addr, len, desc->is_write());
                addr += len;
                nbytes += len;
            }
        }

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

virtio_shared_region::virtio_shared_region(u32 shmid, u64 base, u64 size):
    m_shmid(shmid), m_addr(base, base + size - 1), m_objects() {
}

bool virtio_shared_region::map(u64 id, u64 offset, void* data, u64 size) {
    if (m_objects.count(id) > 0)
        return false; // id already used

    u64 addr = base() + offset;
    virtio_shared_object newobj;
    newobj.id = id;
    newobj.data = (u8*)data;
    newobj.addr = range(addr, addr + size - 1);

    for (auto& [id, obj] : m_objects) {
        if (obj.addr.overlaps(newobj.addr))
            return false;
    }

    m_objects[id] = newobj;
    return true;
}

bool virtio_shared_region::unmap(u64 id) {
    auto it = m_objects.find(id);
    if (it == m_objects.end())
        return false;
    m_objects.erase(it);
    return true;
}

const virtio_shared_object* virtio_shared_region::find(u64 id) const {
    auto it = m_objects.find(id);
    return it != m_objects.end() ? &it->second : nullptr;
}

unsigned int virtio_shared_region::transport(virtio_initiator_socket& socket,
                                             tlm_generic_payload& tx) {
    range addr(tx);
    for (auto& [id, obj] : m_objects) {
        if (obj.addr.includes(addr)) {
            u64 off = addr.start - obj.addr.start;
            u64 len = addr.length();
            if (tx.is_read())
                memcpy(tx.get_data_ptr(), obj.data + off, len);
            if (tx.is_write())
                memcpy(obj.data + off, tx.get_data_ptr(), len);
            tx.set_dmi_allowed(true);
            tx.set_response_status(TLM_OK_RESPONSE);
            return len;
        }
    }

    bool ok = false;
    if (tx.is_read())
        ok = socket->read_shm(m_shmid, tx, tx.get_data_ptr());
    if (tx.is_write())
        ok = socket->write_shm(m_shmid, tx, tx.get_data_ptr());
    if (ok)
        tx.set_response_status(TLM_OK_RESPONSE);
    else
        tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
    return ok ? tx.get_data_length() : 0;
}

bool virtio_shared_region::get_dmi_ptr(u64 addr, tlm_dmi& dmi) {
    for (auto& [id, obj] : m_objects) {
        if (obj.addr.includes(addr)) {
            dmi.allow_read_write();
            dmi.set_start_address(obj.addr.start);
            dmi.set_end_address(obj.addr.end);
            dmi.set_dmi_ptr(obj.data);
            return true;
        }
    }

    return false;
}

virtio_shared_memory::virtio_shared_memory(u64 capacity):
    m_capacity(capacity), m_regions() {
}

u64 virtio_shared_memory::next_base() const {
    u64 hi = 0;
    for (auto& [shmid, shm] : m_regions)
        hi = max(hi, shm.addr().end + 1);

    // round up to a multiple of page size
    u64 page_size = mwr::get_page_size();
    return (hi + page_size - 1) & ~(page_size - 1);
}

u64 virtio_shared_memory::region_base(u32 shmid) const {
    auto it = m_regions.find(shmid);
    return it != m_regions.end() ? it->second.base() : -1;
}

u64 virtio_shared_memory::region_size(u32 shmid) const {
    auto it = m_regions.find(shmid);
    return it != m_regions.end() ? it->second.size() : -1;
}

const virtio_shared_object* virtio_shared_memory::find(u32 shmid,
                                                       u64 id) const {
    auto it = m_regions.find(shmid);
    return it != m_regions.end() ? it->second.find(id) : nullptr;
}

bool virtio_shared_memory::request(u32 shmid, u64 size) {
    if (m_regions.count(shmid) > 0)
        return false; // already requested

    u64 base = next_base();
    u64 remaining = m_capacity - base;

    if (size > remaining)
        return false;

    m_regions.emplace(shmid, virtio_shared_region(shmid, base, size));
    return true;
}

bool virtio_shared_memory::map(u32 shmid, u64 id, u64 offset, void* data,
                               u64 size) {
    auto it = m_regions.find(shmid);
    if (it == m_regions.end())
        return false;
    return it->second.map(id, offset, data, size);
}

bool virtio_shared_memory::unmap(u32 shmid, u64 id) {
    auto it = m_regions.find(shmid);
    if (it == m_regions.end())
        return false;
    return it->second.unmap(id);
}

void virtio_shared_memory::reset() {
    m_regions.clear();
}

unsigned int virtio_shared_memory::transport(virtio_initiator_socket& socket,
                                             tlm_generic_payload& tx) {
    range addr(tx);
    for (auto& [shmid, shm] : m_regions) {
        if (shm.addr().includes(addr))
            return shm.transport(socket, tx);
    }

    tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
    return 0;
}

bool virtio_shared_memory::get_dmi_ptr(u64 addr, tlm_dmi& dmi) {
    for (auto& [shmid, shm] : m_regions) {
        if (shm.addr().includes(addr))
            return shm.get_dmi_ptr(addr, dmi);
    }

    return false;
}

virtio_base_initiator_socket::virtio_base_initiator_socket(const char* nm):
    virtio_base_initiator_socket_b(nm, VCML_AS_DEFAULT), m_stub(nullptr) {
}

virtio_base_initiator_socket::~virtio_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void virtio_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new virtio_target_stub(basename());
    bind(m_stub->virtio_in);
}

void virtio_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = virtio_base_initiator_socket;
    using T = virtio_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void virtio_base_initiator_socket::stub_socket(void* data) {
    stub();
}

virtio_base_target_socket::virtio_base_target_socket(const char* nm):
    virtio_base_target_socket_b(nm, VCML_AS_DEFAULT), m_stub(nullptr) {
}

virtio_base_target_socket::~virtio_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void virtio_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new virtio_initiator_stub(basename());
    m_stub->virtio_out.bind(*this);
}

void virtio_base_target_socket::bind_socket(sc_object& obj) {
    using I = virtio_base_initiator_socket;
    using T = virtio_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void virtio_base_target_socket::stub_socket(void* data) {
    stub();
}

virtio_initiator_socket::virtio_initiator_socket(const char* nm):
    virtio_base_initiator_socket(nm),
    m_controller(hierarchy_search<virtio_controller>()),
    m_transport(m_controller) {
    VCML_ERROR_ON(!m_controller, "%s has no virtio_controller", name());
    bind(m_transport);
}

virtio_target_socket::virtio_target_socket(const char* nm):
    virtio_base_target_socket(nm),
    m_device(hierarchy_search<virtio_device>()),
    m_transport(m_device) {
    VCML_ERROR_ON(!m_device, "%s has no virtio_device", name());
    bind(m_transport);
}

bool virtio_initiator_stub::put(u32 vqid, vq_message& msg) {
    return false;
}

bool virtio_initiator_stub::get(u32 vqid, vq_message& msg) {
    return false;
}

bool virtio_initiator_stub::notify() {
    return false;
}

bool virtio_initiator_stub::shm_map(u32 shmid, u64 id, u64 offset, void* ptr,
                                    u64 len) {
    return false;
}

bool virtio_initiator_stub::shm_unmap(u32 shmid, u64 id) {
    return false;
}

virtio_initiator_stub::virtio_initiator_stub(const char* nm):
    virtio_bw_transport_if(), virtio_out(mkstr("%s_stub", nm).c_str()) {
    virtio_out.bind(*this);
}

void virtio_target_stub::identify(virtio_device_desc& desc) {
    desc.device_id = VIRTIO_DEVICE_NONE;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
}

bool virtio_target_stub::notify(u32 vqid) {
    return false;
}

void virtio_target_stub::reset() {
    // nothing to do
}

void virtio_target_stub::read_features(u64& features) {
    features = 0;
}

bool virtio_target_stub::write_features(u64 features) {
    return false;
}

bool virtio_target_stub::read_config(const range& addr, void* ptr) {
    return false;
}

bool virtio_target_stub::write_config(const range& addr, const void* p) {
    return false;
}

bool virtio_target_stub::read_shm(u32 shmid, const range& addr, void* data) {
    return false;
}

bool virtio_target_stub::write_shm(u32 shmid, const range& addr,
                                   const void* data) {
    return false;
}

virtio_target_stub::virtio_target_stub(const char* nm):
    virtio_fw_transport_if(), virtio_in(mkstr("%s_stub", nm).c_str()) {
    virtio_in.bind(*this);
}

void virtio_stub(const sc_object& obj, const string& port) {
    stub(obj, port);
}

void virtio_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2) {
    bind(obj1, port1, obj2, port2);
}

} // namespace vcml
