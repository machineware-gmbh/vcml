/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_VIRTIO_H
#define VCML_PROTOCOLS_VIRTIO_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/protocols/base.h"
#include "vcml/protocols/pci_ids.h"

namespace vcml {

enum virtio_status : int {
    VIRTIO_INCOMPLETE = 0,
    VIRTIO_OK = 1,
    VIRTIO_ERR_INDIRECT = -1,
    VIRTIO_ERR_NODMI = -2,
    VIRTIO_ERR_CHAIN = -3,
    VIRTIO_ERR_DESC = -4,
};

const char* virtio_status_str(virtio_status status);

inline bool success(virtio_status sts) {
    return sts > 0;
}
inline bool failed(virtio_status sts) {
    return sts < 0;
}

enum virtio_devices : u32 {
    VIRTIO_DEVICE_NONE = 0,
    VIRTIO_DEVICE_NET = 1,
    VIRTIO_DEVICE_BLOCK = 2,
    VIRTIO_DEVICE_CONSOLE = 3,
    VIRTIO_DEVICE_RNG = 4,
    VIRTIO_DEVICE_P9FS = 9,
    VIRTIO_DEVICE_GPU = 16,
    VIRTIO_DEVICE_INPUT = 18,
    VIRTIO_DEVICE_SOUND = 25,
};

enum virtio_vendors : u32 {
    VIRTIO_VENDOR_NONE = 0,
    VIRTIO_VENDOR_VCML = fourcc("vcml"),
};

enum virtio_features : u64 {
    VIRTIO_F_RING_INDIRECT_DESC = bit(28),
    VIRTIO_F_RING_EVENT_IDX = bit(29),
    VIRTIO_F_VERSION_1 = bit(32),
    VIRTIO_F_ACCESS_PLATFORM = bit(33),
    VIRTIO_F_RING_PACKED = bit(34),
    VIRTIO_F_IN_ORDER = bit(35),
    VIRTIO_F_ORDER_PLATFORM = bit(36),
    VIRTIO_F_SR_IOV = bit(37),
    VIRTIO_F_NOTIFICATION_DATA = bit(38),
    VIRTIO_F_NOTIF_CONFIG_DATA = bit(39),
    VIRTIO_F_RING_RESET = bit(40),
    VIRTIO_F_ADMIN_VQ = bit(41),
};

enum virtio_vectors : u16 {
    VIRTIO_NO_VECTOR = 0xffff,
};

enum virtio_device_status : u32 {
    VIRTIO_STATUS_ACKNOWLEDGE = bit(0),
    VIRTIO_STATUS_DRIVER = bit(1),
    VIRTIO_STATUS_DRIVER_OK = bit(2),
    VIRTIO_STATUS_FEATURES_OK = bit(3),
    VIRTIO_STATUS_DEVICE_NEEDS_RESET = bit(6),
    VIRTIO_STATUS_FAILED = bit(7),

    VIRTIO_STATUS_FEATURE_CHECK = VIRTIO_STATUS_DRIVER |
                                  VIRTIO_STATUS_FEATURES_OK,
    VIRTIO_STATUS_DEVICE_READY = VIRTIO_STATUS_DRIVER |
                                 VIRTIO_STATUS_FEATURES_OK |
                                 VIRTIO_STATUS_DRIVER_OK,

    VIRTIO_STATUS_MASK = bitmask(8),
};

constexpr bool virtio_feature_check(u32 sts) {
    return (sts & VIRTIO_STATUS_FEATURE_CHECK) == VIRTIO_STATUS_FEATURE_CHECK;
}

constexpr bool virtio_device_ready(u32 sts) {
    return (sts & VIRTIO_STATUS_DEVICE_READY) == VIRTIO_STATUS_DEVICE_READY;
}

enum virtio_irq_status : u32 {
    VIRTIO_IRQSTATUS_VQUEUE = bit(0),
    VIRTIO_IRQSTATUS_CONFIG = bit(1),

    VIRTIO_IRQSTATUS_MASK = bitmask(2, 0),
};

enum virtio_virtqueue_ids : u32 {
    VIRTQUEUE_MAX = 1024,
};

struct virtio_queue_desc {
    u32 id;
    u32 limit;
    u32 size;
    u64 desc;
    u64 driver;
    u64 device;
    u16 vector;
    bool has_event_idx;

    virtio_queue_desc(u32 qid, u32 sz):
        id(qid),
        limit(sz),
        size(sz),
        desc(0),
        driver(0),
        device(0),
        vector(VIRTIO_NO_VECTOR),
        has_event_idx(false) {}
};

struct virtio_shm_desc {
    u32 shmid;
    u64 capacity;

    virtio_shm_desc(u32 id, u64 size): shmid(id), capacity(size) {}
};

struct virtio_device_desc {
    u32 device_id;
    u32 vendor_id;
    u32 pci_class;
    u64 shm_capacity;

    std::map<u32, virtio_queue_desc> virtqueues;
    std::map<u32, virtio_shm_desc> shmems;

    void request_virtqueue(u32 id, u32 max_size) {
        virtqueues.insert({ id, virtio_queue_desc(id, max_size) });
    }

    u64 remaining_shm_capacity() const {
        u64 remaining = shm_capacity;
        for (auto& [shmid, desc] : shmems)
            remaining -= desc.capacity;
        return remaining;
    }

    bool request_shm(u32 shmid, u64 capacity) {
        if (capacity == 0 || capacity > remaining_shm_capacity())
            return false;

        shmems.insert({ shmid, virtio_shm_desc(shmid, capacity) });
        return true;
    }

    void reset();
};

inline void virtio_device_desc::reset() {
    device_id = 0;
    vendor_id = 0;
    pci_class = PCI_CLASS_OTHERS;
    shm_capacity = 0;
    virtqueues.clear();
    shmems.clear();
}

typedef function<u8*(u64, u64, vcml_access)> virtio_dmifn;

struct vq_message {
    virtio_dmifn dmi;
    virtio_status status;

    u32 index;

    struct vq_buffer {
        u64 addr;
        u32 size;
    };

    vector<vq_buffer> in;
    vector<vq_buffer> out;

    void append(u64 addr, u32 sz, bool iswr);
    void trim(u32 max_len);

    u32 length_in() const;
    u32 length_out() const;

    u32 length() const { return length_in() + length_out(); }
    u32 ndescs() const { return in.size() + out.size(); }

    size_t copy_out(const void* ptr, size_t sz, size_t offset = 0);
    size_t copy_in(void* ptr, size_t sz, size_t offset = 0);

    template <typename T>
    size_t copy_out(const vector<T>& data, size_t offset = 0);

    template <typename T>
    size_t copy_in(vector<T>& data, size_t offset = 0);

    template <typename T>
    size_t copy_out(const T& data, size_t offset = 0);

    template <typename T>
    size_t copy_in(T& data, size_t offset = 0);
};

inline void vq_message::append(u64 addr, u32 sz, bool iswr) {
    if (iswr)
        out.push_back({ addr, sz });
    else
        in.push_back({ addr, sz });
}

inline void vq_message::trim(u32 max_len) {
    for (auto& buf : out) {
        if (buf.size > max_len) {
            buf.size = max_len;
            max_len = 0;
        } else {
            max_len -= buf.size;
        }
    }
}

inline u32 vq_message::length_in() const {
    u32 length = 0;
    for (const auto& buf : in)
        length += buf.size;
    return length;
}

inline u32 vq_message::length_out() const {
    u32 length = 0;
    for (const auto& buf : out)
        length += buf.size;
    return length;
}

template <typename T>
size_t vq_message::copy_out(const T& data, size_t offset) {
    return copy_out(&data, sizeof(data), offset);
}

template <typename T>
size_t vq_message::copy_in(T& data, size_t offset) {
    return copy_in(&data, sizeof(data), offset);
}

template <typename T>
size_t vq_message::copy_out(const vector<T>& data, size_t offset) {
    return copy_out(data.data(), data.size(), offset);
}

template <typename T>
size_t vq_message::copy_in(vector<T>& data, size_t offset) {
    return copy_in(data.data(), data.size(), offset);
}

template <>
inline bool success(const vq_message& msg) {
    return success(msg.status);
}

template <>
inline bool failed(const vq_message& msg) {
    return failed(msg.status);
}

ostream& operator<<(ostream& os, const vq_message& msg);

class virtqueue : public sc_object
{
protected:
    virtual virtio_status do_get(vq_message& msg) = 0;
    virtual virtio_status do_put(vq_message& msg) = 0;

public:
    const u32 id;
    const u32 limit;
    const u32 size;

    const u64 addr_desc;
    const u64 addr_driver;
    const u64 addr_device;

    const bool has_event_idx;

    bool notify;

    u16 vector;

    virtio_dmifn dmi;

    module* parent;

    logger log;

    virtqueue() = delete;
    virtqueue(const virtqueue&) = delete;
    virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi);
    virtual ~virtqueue();

    virtual bool validate() = 0;
    virtual void invalidate(const range& mem) = 0;

    bool get(vq_message& msg);
    bool put(vq_message& msg);
};

class split_virtqueue : public virtqueue
{
private:
    struct vq_desc {
        u64 addr;
        u32 len;
        u16 flags;
        u16 next;

        enum flags : u16 {
            F_NEXT = bit(0),
            F_WRITE = bit(1),
            F_INDIRECT = bit(2),
        };

        bool is_chained() const { return flags & F_NEXT; }
        bool is_write() const { return flags & F_WRITE; }
        bool is_indirect() const { return flags & F_INDIRECT; }
    };

    struct vq_avail {
        u16 flags;
        u16 idx;
        u16 ring[];

        enum flags : u16 {
            F_NO_INTERRUPT = bit(0),
        };

        bool no_irq() const { return flags & F_NO_INTERRUPT; }
    };

    struct vq_used {
        u16 flags;
        u16 idx;

        struct elem {
            u32 id;
            u32 len;
        } ring[];

        enum flags : u16 {
            F_NO_NOTIFY = bit(0),
        };

        bool no_notify() const { return flags & F_NO_NOTIFY; }
    };

    static_assert(sizeof(vq_desc) == 16, "descriptor size mismatch");
    static_assert(sizeof(vq_avail) == 4, "avail area size mismatch");
    static_assert(sizeof(vq_used) == 4, "used area size mismatch");

    u16 m_last_avail_idx;

    vq_desc* m_desc;
    vq_avail* m_avail;
    vq_used* m_used;

    u16* m_used_ev;
    u16* m_avail_ev;

    u8* lookup_desc_ptr(vq_desc* desc) {
        return dmi(desc->addr, desc->len,
                   desc->is_write() ? VCML_ACCESS_WRITE : VCML_ACCESS_READ);
    }

    u64 descsz() const { return sizeof(vq_desc) * size; }

    u64 drvsz() const {
        u64 availsz = sizeof(vq_avail) + sizeof(m_avail->ring[0]) * size;
        return has_event_idx ? availsz + sizeof(*m_used_ev) : availsz;
    }

    u64 devsz() const {
        u64 usedsz = sizeof(vq_used) + sizeof(m_used->ring[0]) * size;
        return has_event_idx ? usedsz + sizeof(*m_avail_ev) : usedsz;
    }

    virtual virtio_status do_get(vq_message& msg) override;
    virtual virtio_status do_put(vq_message& msg) override;

public:
    split_virtqueue() = delete;
    split_virtqueue(const split_virtqueue&) = delete;
    split_virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi);
    virtual ~split_virtqueue();

    virtual bool validate() override;
    virtual void invalidate(const range& mem) override;
};

class packed_virtqueue : public virtqueue
{
private:
    struct vq_desc {
        u64 addr;
        u32 len;
        u16 id;
        u16 flags;

        enum flags : u16 {
            F_NEXT = bit(0),
            F_WRITE = bit(1),
            F_INDIRECT = bit(2),
            F_PACKED_AVAIL = bit(7),
            F_PACKED_USED = bit(15),
        };

        bool is_chained() const { return flags & F_NEXT; }
        bool is_write() const { return flags & F_WRITE; }
        bool is_indirect() const { return flags & F_INDIRECT; }

        bool is_avail(bool wrap_counter) const {
            return flags & F_PACKED_AVAIL ? wrap_counter : !wrap_counter;
        }

        bool is_used(bool wrap_counter) const {
            return flags & F_PACKED_USED ? wrap_counter : !wrap_counter;
        }

        void mark_used(bool wrap_counter) {
            flags &= ~F_PACKED_USED;
            if (wrap_counter)
                flags |= F_PACKED_USED;
        }
    };

    struct vq_event {
        u16 off_wrap;
        u16 flags;

        enum event_flags : u16 {
            F_EVENT_ENABLE = 0,
            F_EVENT_DISABLE = 1,
            F_EVENT_DESC = 2,
        };

        bool should_notify(u32 index) const {
            switch (flags) {
            case F_EVENT_ENABLE:
                return true;
            case F_EVENT_DISABLE:
                return false;
            case F_EVENT_DESC:
                return index == off_wrap;
            default:
                VCML_ERROR("illegal virtio event flags: 0x%04hx", flags);
            }
        }
    };

    static_assert(sizeof(vq_desc) == 16, "descriptor size mismatch");
    static_assert(sizeof(vq_event) == 4, "event area size mismatch");

    u16 m_last_avail_idx;

    vq_desc* m_desc;
    vq_event* m_driver;
    vq_event* m_device;

    bool m_wrap_get;
    bool m_wrap_put;

    u8* lookup_desc_ptr(vq_desc* desc) {
        return dmi(desc->addr, desc->len,
                   desc->is_write() ? VCML_ACCESS_WRITE : VCML_ACCESS_READ);
    }

    u64 dscsz() const { return sizeof(vq_desc) * size; }
    u64 drvsz() const { return sizeof(vq_event); }
    u64 devsz() const { return sizeof(vq_event); }

    virtual virtio_status do_get(vq_message& msg) override;
    virtual virtio_status do_put(vq_message& msg) override;

public:
    packed_virtqueue() = delete;
    packed_virtqueue(const packed_virtqueue&) = delete;
    packed_virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi);
    virtual ~packed_virtqueue();

    virtual bool validate() override;
    virtual void invalidate(const range& mem) override;
};

class virtio_device
{
public:
    virtual ~virtio_device() = default;

    virtual void identify(virtio_device_desc& desc) = 0;
    virtual bool notify(u32 vqid) = 0;
    virtual void reset() = 0;

    virtual void read_features(u64& features) = 0;
    virtual bool write_features(u64 features) = 0;

    virtual bool read_config(const range& addr, void* data) = 0;
    virtual bool write_config(const range& addr, const void* data) = 0;

    virtual bool read_shm(u32 shmid, const range& addr, void* data) {
        return false;
    }

    virtual bool write_shm(u32 shmid, const range& addr, const void*) {
        return false;
    };
};

class virtio_controller
{
public:
    virtual ~virtio_controller() = default;

    virtual bool put(u32 vqid, vq_message& msg) = 0;
    virtual bool get(u32 vqid, vq_message& msg) = 0;

    virtual bool notify() = 0;

    virtual bool shm_map(u32 shmid, u64 id, u64 offset, void* ptr,
                         u64 len) = 0;
    virtual bool shm_unmap(u32 shmid, u64 id) = 0;
};

class virtio_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef vq_message protocol_types;

    virtio_fw_transport_if() = default;
    virtual ~virtio_fw_transport_if() {}

    virtual void identify(virtio_device_desc& desc) = 0;
    virtual bool notify(u32 vqid) = 0;
    virtual void reset() = 0;

    virtual void read_features(u64& features) = 0;
    virtual bool write_features(u64 features) = 0;

    virtual bool read_config(const range& addr, void* data) = 0;
    virtual bool write_config(const range& addr, const void* data) = 0;

    virtual bool read_shm(u32 shmid, const range& addr, void* data) = 0;
    virtual bool write_shm(u32 shmid, const range& addr, const void* data) = 0;
};

class virtio_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef vq_message protocol_types;

    virtio_bw_transport_if() = default;
    virtual ~virtio_bw_transport_if() {}

    virtual bool put(u32 vqid, vq_message& msg) = 0;
    virtual bool get(u32 vqid, vq_message& msg) = 0;

    virtual bool notify() = 0;

    virtual bool shm_map(u32 shmid, u64 id, u64 off, void* ptr, u64 len) = 0;
    virtual bool shm_unmap(u32 shmid, u64 id) = 0;
};

typedef base_initiator_socket<virtio_fw_transport_if, virtio_bw_transport_if>
    virtio_base_initiator_socket_b;

typedef base_target_socket<virtio_fw_transport_if, virtio_bw_transport_if>
    virtio_base_target_socket_b;

class virtio_initiator_stub;
class virtio_target_stub;

class virtio_base_initiator_socket : public virtio_base_initiator_socket_b
{
private:
    virtio_target_stub* m_stub;

public:
    virtio_base_initiator_socket(const char* nm);
    virtual ~virtio_base_initiator_socket();
    VCML_KIND(virtio_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    virtual void stub();

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* data) override;
};

class virtio_base_target_socket : public virtio_base_target_socket_b
{
private:
    virtio_initiator_stub* m_stub;

public:
    virtio_base_target_socket(const char* nm);
    virtual ~virtio_base_target_socket();
    VCML_KIND(virtio_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    virtual void stub();

    virtual void bind_socket(sc_object& obj) override;
    virtual void stub_socket(void* data) override;
};

class virtio_initiator_socket : public virtio_base_initiator_socket
{
private:
    virtio_controller* m_controller;

    struct virtio_bw_transport : virtio_bw_transport_if {
        virtio_controller* parent;
        virtio_bw_transport(virtio_controller* ctrl): parent(ctrl) {}
        virtual ~virtio_bw_transport() = default;

        virtual bool put(u32 vqid, vq_message& msg) override {
            return parent->put(vqid, msg);
        }

        virtual bool get(u32 vqid, vq_message& msg) override {
            return parent->get(vqid, msg);
        }

        virtual bool notify() override { return parent->notify(); }

        virtual bool shm_map(u32 shmid, u64 id, u64 offset, void* ptr,
                             u64 len) override {
            return parent->shm_map(shmid, id, offset, ptr, len);
        }

        virtual bool shm_unmap(u32 shmid, u64 id) override {
            return parent->shm_unmap(shmid, id);
        }
    } m_transport;

public:
    virtio_initiator_socket(const char* name);
    virtual ~virtio_initiator_socket() = default;
    VCML_KIND(virtio_initiator_socket);
};

class virtio_target_socket : public virtio_base_target_socket
{
private:
    virtio_device* m_device;

    struct virtio_fw_transport : virtio_fw_transport_if {
        virtio_device* device;
        virtio_fw_transport(virtio_device* dev): device(dev) {}
        virtual ~virtio_fw_transport() = default;

        virtual void identify(virtio_device_desc& desc) override {
            device->identify(desc);
        }

        virtual bool notify(u32 virtqueue_id) override {
            return device->notify(virtqueue_id);
        }

        virtual void reset() override { return device->reset(); }

        virtual void read_features(u64& features) override {
            device->read_features(features);
        }

        virtual bool write_features(u64 features) override {
            return device->write_features(features);
        }

        virtual bool read_config(const range& addr, void* data) override {
            return device->read_config(addr, data);
        }

        virtual bool write_config(const range& addr, const void* p) override {
            return device->write_config(addr, p);
        }

        virtual bool read_shm(u32 shmid, const range& addr,
                              void* data) override {
            return device->read_shm(shmid, addr, data);
        }

        virtual bool write_shm(u32 shmid, const range& addr,
                               const void* data) override {
            return device->write_shm(shmid, addr, data);
        }
    } m_transport;

public:
    virtio_target_socket(const char* name);
    virtual ~virtio_target_socket() = default;
    VCML_KIND(virtio_target_socket);
};

class virtio_initiator_stub : private virtio_bw_transport_if
{
private:
    virtual bool put(u32 vqid, vq_message& msg) override;
    virtual bool get(u32 vqid, vq_message& msg) override;
    virtual bool notify() override;
    virtual bool shm_map(u32 shmid, u64 id, u64 offset, void* ptr,
                         u64 len) override;
    virtual bool shm_unmap(u32 shmid, u64 id) override;

public:
    virtio_base_initiator_socket virtio_out;
    virtio_initiator_stub(const char* nm);
    virtual ~virtio_initiator_stub() = default;
};

class virtio_target_stub : private virtio_fw_transport_if
{
private:
    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;
    virtual void reset() override;
    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;
    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* p) override;
    virtual bool read_shm(u32 shmid, const range& addr, void* data) override;
    virtual bool write_shm(u32 shmid, const range& addr,
                           const void* data) override;

public:
    virtio_base_target_socket virtio_in;
    virtio_target_stub(const char* nm);
    virtual ~virtio_target_stub() = default;
};

void virtio_stub(const sc_object& obj, const string& port);
void virtio_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2);

struct virtio_shared_object {
    u64 id;
    range addr;
    u8* data;
};

class virtio_shared_region
{
private:
    u32 m_shmid;
    range m_addr;
    unordered_map<u64, virtio_shared_object> m_objects;

public:
    virtio_shared_region(u32 shmid, u64 base, u64 size);
    ~virtio_shared_region() = default;

    u32 shmid() const { return m_shmid; }
    range addr() const { return m_addr; }
    u64 base() const { return m_addr.start; }
    u64 size() const { return m_addr.length(); }

    bool map(u64 id, u64 offset, void* data, u64 size);
    bool unmap(u64 id);

    const virtio_shared_object* find(u64 id) const;

    unsigned int transport(virtio_initiator_socket& socket,
                           tlm_generic_payload& tx);
    bool get_dmi_ptr(u64 addr, tlm_dmi& dmi);
};

class virtio_shared_memory
{
private:
    u64 m_capacity;
    unordered_map<u32, virtio_shared_region> m_regions;

public:
    virtio_shared_memory(u64 capacity);
    virtio_shared_memory(virtio_shared_memory&&) = default;
    ~virtio_shared_memory() = default;

    u64 capacity() const { return m_capacity; }

    u64 next_base() const;

    u64 region_base(u32 shmid) const;
    u64 region_size(u32 shmid) const;

    const virtio_shared_object* find(u32 shmid, u64 id) const;

    bool request(u32 shmid, u64 capacity);

    bool map(u32 shmid, u64 id, u64 offset, void* data, u64 size);
    bool unmap(u32 shmid, u64 id);

    void reset();

    unsigned int transport(virtio_initiator_socket& socket,
                           tlm_generic_payload& tx);
    bool get_dmi_ptr(u64 addr, tlm_dmi& dmi);
};

} // namespace vcml

#endif
