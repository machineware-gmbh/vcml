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

#define vq_log_debug(...)                              \
        parent->log_debug("vq%u.%s: %s", id, __func__, \
                         mkstr(__VA_ARGS__).c_str())
#define vq_log_warn(...)                               \
        parent->log_warn("vq%u.%s: %s", id, __func__,  \
                         mkstr(__VA_ARGS__).c_str())

namespace vcml { namespace virtio {

    size_t vq_message::copy_out(const void* ptr, size_t size, size_t offset) {
        const u8* src = (const u8*)ptr;
        size_t copied = 0;

        for (auto buf : out) {
            if (offset >= buf.size) {
                offset -= buf.size;
                continue;
            }

            u8* dest = buf.host_addr + offset;
            size_t n = min(size, buf.size - offset);
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

            const u8* src = buf.host_addr + offset;
            size_t n = min(size, buf.size - offset);
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

    virtqueue::virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi):
        id(desc.id),
        limit(desc.limit),
        size(desc.size),
        addr_desc(desc.desc),
        addr_driver(desc.driver),
        addr_device(desc.device),
        has_event_idx(desc.has_event_idx),
        notify(false),
        lookup_dmi_ptr(dmi),
        parent(dynamic_cast<module*>(hierarchy_top())) {
        VCML_ERROR_ON(!parent, "virtqueue created outside module");
    }

    virtqueue::~virtqueue() {
        // nothing to do
    }

    split_virtqueue::split_virtqueue(const virtio_queue_desc& queue_desc,
        virtio_dmifn dmifn):
        virtqueue(queue_desc, dmifn),
        m_last_avail_idx(0),
        m_desc(nullptr),
        m_avail(nullptr),
        m_used(nullptr),
        m_used_ev(nullptr),
        m_avail_ev(nullptr) {
        if (!addr_desc || !addr_driver || !addr_device) {
            vq_log_warn("invalid virtqueue ring addresses");
            return;
        }

        u64 descsz = sizeof(vq_desc) * size;
        u64 availsz = sizeof(vq_avail) + sizeof(m_avail->ring[0]) * size;
        u64 usedsz = sizeof(vq_used) + sizeof(m_used->ring[0]) * size;

        if (has_event_idx) {
            availsz += sizeof(*m_used_ev);
            usedsz += sizeof(*m_avail_ev);
        }

        u8* desc_ptr = lookup_dmi_ptr(addr_desc, descsz, VCML_ACCESS_READ);
        u8* avail_ptr = lookup_dmi_ptr(addr_driver, availsz, VCML_ACCESS_READ);
        u8* used_ptr = lookup_dmi_ptr(addr_device, usedsz, VCML_ACCESS_WRITE);

        if (!desc_ptr || !avail_ptr || !used_ptr) {
            vq_log_warn("failed to get DMI pointers");
            return;
        }

        m_desc = (vq_desc*)desc_ptr;
        m_avail = (vq_avail*)avail_ptr;
        m_used = (vq_used*)used_ptr;

        if (has_event_idx) {
            m_used_ev  = (u16*)(m_avail->ring + size);
            m_avail_ev = (u16*)(m_used->ring + size);
        }

        vq_log_debug("created split virtqueue %u with size %u", id, limit);
        vq_log_debug("  descriptors at 0x%lx -> %p", addr_desc, m_desc);
        vq_log_debug("  driver ring at 0x%lx -> %p", addr_driver, m_avail);
        vq_log_debug("  device ring at 0x%lx -> %p", addr_device, m_used);
    }

    split_virtqueue::~split_virtqueue() {
        // nothing to do
    }

    bool split_virtqueue::good() {
        return m_desc && m_avail && m_used;
    }

    bool split_virtqueue::get(vq_message& msg) {
        if (m_last_avail_idx == m_avail->idx)
            return false;

        msg.clear();
        msg.index = m_avail->ring[m_last_avail_idx++ % size];
        if (msg.index >= size) {
            vq_log_warn("illegal descriptor index %u", msg.index);
            return false;
        }

        if (m_avail_ev)
            *m_avail_ev = m_last_avail_idx;

        u32 count = 0;
        u32 limit = size;

        vq_desc* base = m_desc;
        vq_desc* desc = base + msg.index;

        if (desc->is_indirect()) {
            if (!desc->len || desc->len % sizeof(vq_desc)) {
                vq_log_warn("broken indirect descriptor");
                return false;
            }

            limit = desc->len / sizeof(vq_desc);
            desc = base = (vq_desc*)lookup_desc_ptr(desc);

            if (!desc) {
                vq_log_warn("cannot access indirect descriptor");
                return false;
            }
        }

        while (true) {
            u8* ptr = lookup_desc_ptr(desc);
            if (!ptr) {
                vq_log_warn("cannot get DMI pointer for descriptor at address "
                            "0x%016lx", desc->addr);
                return false;
            }

            if (!desc->is_write() && msg.length_out)
                vq_log_warn("invalid descriptor order");

            msg.append(desc->addr, ptr, desc->len, desc->is_write());

            if (!desc->is_chained())
                return true;

            if (desc->next >= size) {
                vq_log_warn("broken descriptor chain");
                return false;
            }

            if (count++ >= limit) {
                vq_log_warn("descriptor chain too long");
                return false;
            }

            desc = base + desc->next;
        }
    }

    bool split_virtqueue::put(vq_message& msg) {
        notify = false;

        if (msg.index >= size) {
            vq_log_warn("index out of bounds: %u", msg.index);
            return false;
        }

        if ((m_used_ev && *m_used_ev == m_used->idx) || !m_avail->no_irq())
            notify = true;

        m_used->ring[m_used->idx % size].id = msg.index;
        m_used->ring[m_used->idx % size].len = msg.length();
        m_used->idx++;

        return true;
    }

    packed_virtqueue::packed_virtqueue(const virtio_queue_desc& queue_desc,
        virtio_dmifn dmifn):
        virtqueue(queue_desc, dmifn),
        m_last_avail_idx(0),
        m_desc(nullptr),
        m_driver(nullptr),
        m_device(nullptr),
        m_wrap_get(true),
        m_wrap_put(true) {
        if (!addr_desc || !addr_driver || !addr_device) {
            vq_log_warn("invalid virtqueue ring addresses");
            return;
        }

        m_desc = (vq_desc*)lookup_dmi_ptr(addr_desc, sizeof(vq_desc) * size,
                                          VCML_ACCESS_READ_WRITE);

        if (has_event_idx) {
            m_driver = (vq_event*)lookup_dmi_ptr(addr_driver, sizeof(vq_event),
                                                 VCML_ACCESS_READ);
            m_device = (vq_event*)lookup_dmi_ptr(addr_device, sizeof(vq_event),
                                                 VCML_ACCESS_WRITE);
        }

        vq_log_debug("created packed virtqueue %u with size %u", id, limit);
        vq_log_debug("  descriptors at 0x%lx -> %p", addr_desc, m_desc);

        if (m_driver) {
            vq_log_debug("  driver events at 0x%lx -> %p",
                         addr_driver, m_driver);
        }

        if (m_device) {
            vq_log_debug("  device events at 0x%lx -> %p",
                         addr_device, m_device);
        }
    }

    packed_virtqueue::~packed_virtqueue() {
        // nothing to do
    }

    bool packed_virtqueue::good() {
        return m_desc && m_driver && m_device;
    }

    bool packed_virtqueue::get(vq_message& msg) {
        vq_desc* base = m_desc;
        vq_desc* desc = base + m_last_avail_idx;

        if (!desc->is_avail(m_wrap_get) || desc->is_used(m_wrap_get))
            return false;

        msg.clear();
        msg.index = m_last_avail_idx;

        u32 count = 0;
        u32 limit = size;
        u32 index = msg.index;

        bool indirect = desc->is_indirect();
        if (indirect) {
            if (!desc->len || desc->len % sizeof(vq_desc)) {
                vq_log_warn("broken indirect descriptor");
                return false;
            }

            index = 0;
            limit = desc->len / sizeof(vq_desc);
            desc = base = (vq_desc*)lookup_desc_ptr(desc);

            if (!desc) {
                vq_log_warn("cannot access indirect descriptor");
                return false;
            }
        }

        while (true) {
            if (!desc->is_avail(m_wrap_get) || desc->is_used(m_wrap_get)) {
                parent->log_warn("descriptor not available");
                return false;
            }

            u8* ptr = lookup_desc_ptr(desc);
            if (!ptr) {
                vq_log_warn("cannot get DMI pointer for descriptor at address "
                            "0x%016lx", desc->addr);
                return false;
            }

            if (!desc->is_write() && msg.length_out)
                vq_log_warn("invalid descriptor order");

            msg.append(desc->addr, ptr, desc->len, desc->is_write());

            if (count++ >= limit) {
                vq_log_warn("descriptor chain too long");
                return false;
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

        return true;
    }

    bool packed_virtqueue::put(vq_message& msg) {
        u32 count = 0;
        u32 index = msg.index;
        u32 limit = size;

        vq_desc* base = m_desc;
        vq_desc* desc = base + index;

        notify = m_driver->should_notify(index);

        if (desc->is_indirect()) {
            if (!desc->len || desc->len % sizeof(vq_desc)) {
                vq_log_warn("broken indirect descriptor");
                return false;
            }

            index = 0;
            limit = desc->len / sizeof(vq_desc);
            desc = base = (vq_desc*)lookup_desc_ptr(desc);

            if (!desc) {
                vq_log_warn("cannot access indirect descriptor");
                return false;
            }
        }

        while (true) {
            desc->mark_used(m_wrap_put);

            if (count++ >= limit) {
                vq_log_warn("descriptor chain too long");
                return false;
            }

            index++;
            if (index >= limit) {
                index -= limit;
                m_wrap_put = !m_wrap_put;
            }

            if (!desc->is_chained())
                return true;

            desc = base + index;
        }
    }

    virtio_initiator_socket::virtio_initiator_socket():
        virtio_base_initiator_socket(),
        m_parent(dynamic_cast<sc_module*>(get_parent_object())),
        m_stub(nullptr) {
    }

    virtio_initiator_socket::virtio_initiator_socket(const char* nm):
        virtio_base_initiator_socket(nm),
        m_parent(dynamic_cast<sc_module*>(get_parent_object())),
        m_stub(nullptr) {
    }

    virtio_initiator_socket::~virtio_initiator_socket() {
        if (m_stub)
            delete m_stub;
    }

    sc_type_index virtio_initiator_socket::get_protocol_types() const {
        return typeid(virtio_initiator_socket);
    }

    void virtio_initiator_socket::stub() {
        VCML_ERROR_ON(m_stub, "port %s already stubbed", name());

        const string stubname = mkstr("%s_istub", basename());
        sc_simcontext* simc = sc_object::simcontext();
        simc->hierarchy_push(m_parent);
        auto s = new virtio_target_stub(stubname.c_str());
        bind(s->VIRTIO_IN);
        simc->hierarchy_pop();

        m_stub = s;
    }

    virtio_target_socket::virtio_target_socket():
        virtio_base_target_socket(),
        m_parent(dynamic_cast<sc_module*>(get_parent_object())),
        m_stub(nullptr) {
    }

    virtio_target_socket::virtio_target_socket(const char* nm):
        virtio_base_target_socket(nm),
        m_parent(dynamic_cast<sc_module*>(get_parent_object())),
        m_stub(nullptr) {
    }

    virtio_target_socket::~virtio_target_socket() {
        if (m_stub)
            delete m_stub;
    }

    sc_type_index virtio_target_socket::get_protocol_types() const {
        return typeid(virtio_target_socket);
    }

    void virtio_target_socket::stub() {
        VCML_ERROR_ON(m_stub, "port %s already stubbed", name());

        const string stubname = mkstr("%s_tstub", basename());
        sc_simcontext* simc = sc_object::simcontext();
        simc->hierarchy_push(m_parent);
        auto s = new virtio_initiator_stub(stubname.c_str());
        s->VIRTIO_OUT.bind(*this);
        simc->hierarchy_pop();

        m_stub = s;
    }

    bool virtio_initiator_stub::put(u32 vqid, vq_message& msg) {
        (void) vqid;
        (void) msg;
        return false;
    }

    bool virtio_initiator_stub::get(u32 vqid, vq_message& msg) {
        (void) vqid;
        (void) msg;
        return false;
    }

    bool virtio_initiator_stub::notify() {
        return false;
    }

    virtio_initiator_stub::virtio_initiator_stub(const sc_module_name& nm):
        module(nm),
        virtio_bw_transport_if(),
        VIRTIO_OUT("VIRTIO_OUT") {
        VIRTIO_OUT.bind(*this);
    }

    virtio_initiator_stub::~virtio_initiator_stub() {
        // nothing to do
    }

    void virtio_target_stub::identify(virtio_device_desc& desc) {
        desc.device_id = VIRTIO_DEVICE_NONE;
        desc.vendor_id = VIRTIO_VENDOR_VCML;
    }

    bool virtio_target_stub::notify(u32 vqid) {
        (void) vqid;
        return false;
    }

    void virtio_target_stub::read_features(u64& features)  {
        features = 0;
    }

    bool virtio_target_stub::write_features(u64 features)  {
        (void) features;
        return false;
    }

    bool virtio_target_stub::read_config(const range& addr, void* ptr)  {
        (void) addr;
        (void) ptr;
        return false;
    }

    bool virtio_target_stub::write_config(const range& addr, const void* ptr) {
        (void) addr;
        (void) ptr;
        return false;
    }

    virtio_target_stub::virtio_target_stub(const sc_module_name& nm):
        module(nm),
        virtio_fw_transport_if(),
        VIRTIO_IN("VIRTIO_IN") {
        VIRTIO_IN.bind(*this);
    }

    virtio_target_stub::~virtio_target_stub() {
        // nothing to do
    }

}}
