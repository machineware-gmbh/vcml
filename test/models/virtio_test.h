/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TEST_MODELS_VIRTIO_TEST_H
#define VCML_TEST_MODELS_VIRTIO_TEST_H

#include "testing.h"

class virtio_stim : public module, public virtio_controller
{
public:
    enum : size_t {
        MEM_SIZE = 1 * MiB,
    };

    virtio_initiator_socket virtio_out;

    vector<u8> mem;
    size_t mem_next;
    u32 msg_next;
    bool accept;
    size_t irqs;

    std::map<u32, std::deque<vq_message>> avail;
    std::map<u32, vector<vq_message>> used;

    virtio_stim(const sc_module_name& nm):
        module(nm),
        virtio_controller(),
        virtio_out("virtio_out"),
        mem(MEM_SIZE),
        mem_next(0),
        msg_next(0),
        accept(true),
        irqs(0),
        avail(),
        used() {}

    virtual ~virtio_stim() = default;

    virtual bool get(u32 vqid, vq_message& msg) override {
        if (avail[vqid].empty())
            return false;
        msg = avail[vqid].front();
        avail[vqid].pop_front();
        return true;
    }

    virtual bool put(u32 vqid, vq_message& msg) override {
        if (!accept)
            return false;
        used[vqid].push_back(msg);
        return true;
    }

    virtual bool notify() override {
        irqs++;
        return true;
    }

    virtual bool shm_map(u32 shmid, u64 id, u64 offset, void* ptr,
                         u64 len) override {
        return false;
    }

    virtual bool shm_unmap(u32 shmid, u64 id) override { return false; }

    void clear() {
        avail.clear();
        used.clear();
        accept = true;
        irqs = 0;
        mem_next = 0;
        std::fill(mem.begin(), mem.end(), 0xff);
    }

    u64 alloc(size_t size) {
        u64 addr = mem_next;
        mem_next += (size + 7) & ~7ull;
        VCML_ERROR_ON(mem_next > mem.size(), "out of test memory");
        return addr;
    }

    vq_message message(const vector<vector<u8>>& in,
                       const vector<size_t>& out) {
        vq_message msg;
        msg.status = VIRTIO_INCOMPLETE;
        msg.index = msg_next++;
        msg.dmi = [this](u64 addr, u64 size, vcml_access rw) -> u8* {
            if (addr >= mem.size() || size > mem.size() - addr)
                return nullptr;
            return mem.data() + addr;
        };

        for (const auto& data : in) {
            u64 addr = alloc(data.size());
            if (!data.empty())
                memcpy(mem.data() + addr, data.data(), data.size());
            msg.append(addr, (u32)data.size(), false);
        }

        for (size_t size : out)
            msg.append(alloc(size), (u32)size, true);
        return msg;
    }

    vq_message submit(u32 vqid, const vector<vector<u8>>& in,
                      const vector<size_t>& out) {
        vq_message msg = message(in, out);
        avail[vqid].push_back(msg);
        return msg;
    }

    bool kick(u32 vqid) { return virtio_out->notify(vqid); }

    vector<u8> response(const vq_message& msg) const {
        vector<u8> data;
        for (const auto& buf : msg.out) {
            const u8* ptr = mem.data() + buf.addr;
            data.insert(data.end(), ptr, ptr + buf.size);
        }

        return data;
    }

    virtio_device_desc identify() {
        virtio_device_desc desc;
        desc.reset();
        virtio_out->identify(desc);
        return desc;
    }

    u64 features() {
        u64 features = 0;
        virtio_out->read_features(features);
        return features;
    }

    template <typename T>
    bool read_config(u64 offset, T& data) {
        return virtio_out->read_config({ offset, offset + sizeof(T) - 1 },
                                       &data);
    }

    template <typename T>
    bool write_config(u64 offset, const T& data) {
        return virtio_out->write_config({ offset, offset + sizeof(T) - 1 },
                                        &data);
    }

    template <typename T>
    static vector<u8> bytes(const T& data) {
        const u8* ptr = reinterpret_cast<const u8*>(&data);
        return vector<u8>(ptr, ptr + sizeof(T));
    }

    static vector<u8> concat(std::initializer_list<vector<u8>> parts) {
        vector<u8> data;
        for (const auto& part : parts)
            data.insert(data.end(), part.begin(), part.end());
        return data;
    }
};

#endif
