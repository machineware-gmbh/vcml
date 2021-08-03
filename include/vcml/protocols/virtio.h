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

#ifndef VCML_PROTOCOLS_VIRTIO_H
#define VCML_PROTOCOLS_VIRTIO_H

#include "vcml/common/types.h"
#include "vcml/common/bitops.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/module.h"

namespace vcml { namespace virtio {

    enum virtio_devices : u32 {
        VIRTIO_DEVICE_NONE    = 0,
        VIRTIO_DEVICE_NET     = 1,
        VIRTIO_DEVICE_BLOCK   = 2,
        VIRTIO_DEVICE_CONSOLE = 3,
        VIRTIO_DEVICE_RNG     = 4,
        VIRTIO_DEVICE_GPU     = 16,
        VIRTIO_DEVICE_INPUT   = 18,
    };

    enum virtio_vendors : u32 {
        VIRTIO_VENDOR_NONE = 0,
        VIRTIO_VENDOR_VCML = fourcc("vcml"),
    };

    enum virtio_features : u64 {
        VIRTIO_F_RING_INDIRECT_DESC = 1ull << 28,
        VIRTIO_F_RING_EVENT_IDX     = 1ull << 29,
        VIRTIO_F_VERSION_1          = 1ull << 32,
        VIRTIO_F_ACCESS_PLATFORM    = 1ull << 33,
        VIRTIO_F_RING_PACKED        = 1ull << 34,
        VIRTIO_F_IN_ORDER           = 1ull << 35,
        VIRTIO_F_ORDER_PLATFORM     = 1ull << 36,
        VIRTIO_F_SR_IOV             = 1ull << 37,
        VIRTIO_F_NOTIFICATION_DATA  = 1ull << 38,
    };

    struct virtio_queue_desc {
        u32  id;
        u32  limit;
        u32  size;
        u64  desc;
        u64  driver;
        u64  device;
        bool has_event_idx;
    };

    struct virtio_device_desc {
        u32 device_id;
        u32 vendor_id;
        std::map<u32, virtio_queue_desc> virtqueues;

        void request_virtqueue(u32 id, u32 max_size) {
            virtqueues.insert({id, {id, max_size, 0, 0, 0, 0, false} });
        }

        void reset();
    };

    inline void virtio_device_desc::reset() {
        device_id = 0;
        vendor_id = 0;
        virtqueues.clear();
    }

    struct vq_message {
        u32 index;

        u32 length_in;
        u32 length_out;

        struct vq_buffer {
            u64 phys_addr;
            u8* host_addr;
            u32 size;
        };

        vector<vq_buffer> in;
        vector<vq_buffer> out;

        void append(u64 addr, u8* ptr, u32 sz, bool iswr);
        void clear();

        u32 ndescs() const { return in.size() + out.size(); }
        u32 length() const { return length_in + length_out; }

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

    inline void vq_message::append(u64 addr, u8* ptr, u32 sz, bool iswr) {
        if (iswr) {
            out.push_back({addr, ptr, sz});
            length_out += sz;
        } else {
            in.push_back({addr, ptr, sz});
            length_in += sz;
        }
    }

    inline void vq_message::clear() {
        index = -1;
        length_in = 0;
        length_out = 0;
        in.clear();
        out.clear();
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

    typedef function<u8*(u64, u64, vcml_access)> virtio_dmifn;

    class virtqueue
    {
    public:
        const u32 id;
        const u32 limit;
        const u32 size;

        const u64 addr_desc;
        const u64 addr_driver;
        const u64 addr_device;

        const bool has_event_idx;

        bool notify;

        virtio_dmifn lookup_dmi_ptr;

        module* parent;

        virtqueue() = delete;
        virtqueue(const virtqueue&) = delete;
        virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi);
        virtual ~virtqueue();

        virtual bool good() = 0;
        virtual bool get(vq_message& msg) = 0;
        virtual bool put(vq_message& msg) = 0;
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
                F_NEXT     = 1u << 0,
                F_WRITE    = 1u << 1,
                F_INDIRECT = 1u << 2,
            };

            bool is_chained()  const { return flags & F_NEXT; }
            bool is_write()    const { return flags & F_WRITE; }
            bool is_indirect() const { return flags & F_INDIRECT; }
        };

        struct vq_avail {
            u16 flags;
            u16 idx;
            u16 ring[];

            enum flags : u16 {
                F_NO_INTERRUPT = 1u << 0,
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
                F_NO_NOTIFY = 1u << 0,
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
            return lookup_dmi_ptr(desc->addr, desc->len, desc->is_write() ?
                                  VCML_ACCESS_WRITE : VCML_ACCESS_READ);
        }

    public:
        split_virtqueue() = delete;
        split_virtqueue(const split_virtqueue&) = delete;
        split_virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi);
        virtual ~split_virtqueue();

        virtual bool good() override;
        virtual bool get(vq_message& msg) override;
        virtual bool put(vq_message& msg) override;
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
                F_NEXT         = 1u << 0,
                F_WRITE        = 1u << 1,
                F_INDIRECT     = 1u << 2,
                F_PACKED_AVAIL = 1u << 7,
                F_PACKED_USED  = 1u << 15,
            };

            bool is_chained()  const { return flags & F_NEXT; }
            bool is_write()    const { return flags & F_WRITE; }
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
                case F_EVENT_ENABLE: return true;
                case F_EVENT_DISABLE: return false;
                case F_EVENT_DESC: return index == off_wrap;
                default:
                    VCML_ERROR("illegal virtio event flags: 0x%04hx", flags);
                }
            }
        };

        static_assert(sizeof(vq_desc) == 16, "descriptor size mismatch");
        static_assert(sizeof(vq_event) == 4, "event area size mismatch");

        u16 m_last_avail_idx;

        vq_desc*  m_desc;
        vq_event* m_driver;
        vq_event* m_device;

        bool m_wrap_get;
        bool m_wrap_put;

        u8* lookup_desc_ptr(vq_desc* desc) {
            return lookup_dmi_ptr(desc->addr, desc->len, desc->is_write() ?
                                  VCML_ACCESS_WRITE : VCML_ACCESS_READ);
        }

    public:
        packed_virtqueue() = delete;
        packed_virtqueue(const packed_virtqueue&) = delete;
        packed_virtqueue(const virtio_queue_desc& desc, virtio_dmifn dmi);
        virtual ~packed_virtqueue();

        virtual bool good() override;
        virtual bool get(vq_message& msg) override;
        virtual bool put(vq_message& msg) override;
    };

    class virtio_fw_transport_if: public sc_core::sc_interface
    {
    public:
        virtio_fw_transport_if() = default;
        virtual ~virtio_fw_transport_if() {}

        virtual void identify(virtio_device_desc& desc) = 0;
        virtual bool notify(u32 vqid) = 0;

        virtual void read_features(u64& features) = 0;
        virtual bool write_features(u64 features) = 0;

        virtual bool read_config(const range& addr, void* data) = 0;
        virtual bool write_config(const range& addr, const void* data) = 0;
    };

    class virtio_bw_transport_if: public sc_core::sc_interface
    {
    public:
        virtio_bw_transport_if() = default;
        virtual ~virtio_bw_transport_if() {}

        virtual bool put(u32 vqid, vq_message& msg) = 0;
        virtual bool get(u32 vqid, vq_message& msg) = 0;

        virtual bool notify() = 0;
    };

    typedef tlm::tlm_base_initiator_socket<1, virtio_fw_transport_if,
                                           virtio_bw_transport_if, 1,
                                           sc_core::SC_ONE_OR_MORE_BOUND>
        virtio_base_initiator_socket;

    typedef tlm::tlm_base_target_socket<1, virtio_fw_transport_if,
                                        virtio_bw_transport_if, 1,
                                        sc_core::SC_ONE_OR_MORE_BOUND>
        virtio_base_target_socket;

    class virtio_initiator_socket: public virtio_base_initiator_socket
    {
    private:
        sc_module* m_parent;
        sc_module* m_stub;

    public:
        bool is_stubbed() const { return m_stub != nullptr; }

        virtio_initiator_socket();
        explicit virtio_initiator_socket(const char* name);
        virtual ~virtio_initiator_socket();
        VCML_KIND(virtio_initiator_socket);
        virtual sc_type_index get_protocol_types() const;
        virtual void stub();
    };

    class virtio_target_socket: public virtio_base_target_socket
    {
    private:
        sc_module* m_parent;
        sc_module* m_stub;

    public:
        bool is_stubbed() const { return m_stub != nullptr; }

        virtio_target_socket();
        explicit virtio_target_socket(const char* name);
        virtual ~virtio_target_socket();
        VCML_KIND(virtio_target_socket);
        virtual sc_type_index get_protocol_types() const;
        virtual void stub();
    };

    class virtio_initiator_stub: public module, private virtio_bw_transport_if
    {
    private:
        virtual bool put(u32 vqid, vq_message& msg) override;
        virtual bool get(u32 vqid, vq_message& msg) override;
        virtual bool notify() override;

    public:
        virtio_initiator_socket VIRTIO_OUT;

        virtio_initiator_stub() = delete;
        virtio_initiator_stub(const virtio_initiator_stub&) = delete;
        virtio_initiator_stub(const sc_module_name& nm);
        virtual ~virtio_initiator_stub();
        VCML_KIND(virtio_initiator_stub);
    };

    class virtio_target_stub: public module, private virtio_fw_transport_if
    {
    private:
        virtual void identify(virtio_device_desc& desc) override;
        virtual bool notify(u32 vqid) override;

        virtual void read_features(u64& features) override;
        virtual bool write_features(u64 features) override;

        virtual bool read_config(const range& addr, void* ptr) override;
        virtual bool write_config(const range& addr, const void* ptr) override;

    public:
        virtio_target_socket VIRTIO_IN;

        virtio_target_stub() = delete;
        virtio_target_stub(const virtio_target_stub&) = delete;
        virtio_target_stub(const sc_module_name& nm);
        virtual ~virtio_target_stub();
        VCML_KIND(virtio_target_stub);
    };

}}

#endif
