/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TEST_MODELS_VIRTIO_TRANSPORT_H
#define VCML_TEST_MODELS_VIRTIO_TRANSPORT_H

#include "testing.h"

class virtio_echo : public module, public virtio_device
{
public:
    enum : u32 {
        DEVICE_ID = 42,
        VQ0_SIZE = 8,
        VQ1_SIZE = 4,
        SHM_ID = 1,
    };

    enum : u64 {
        FEATURES = bit(0) | bit(5),
    };

    virtio_target_socket virtio_in;

    u64 shm_size;
    u64 driver_features;
    bool accept_features;
    bool accept_notify;
    size_t resets;
    vector<u32> kicks;
    vector<u8> config;
    vector<u8> shm;

    virtio_echo(const sc_module_name& nm, u64 shmsz = 0):
        module(nm),
        virtio_device(),
        virtio_in("virtio_in"),
        shm_size(shmsz),
        driver_features(0),
        accept_features(true),
        accept_notify(true),
        resets(0),
        kicks(),
        config(16),
        shm(shmsz) {
        for (size_t i = 0; i < config.size(); i++)
            config[i] = (u8)(0xc0 + i);
    }

    virtual ~virtio_echo() = default;

    virtual void identify(virtio_device_desc& desc) override {
        desc.device_id = DEVICE_ID;
        desc.vendor_id = VIRTIO_VENDOR_VCML;
        desc.pci_class = PCI_CLASS_OTHERS;
        desc.request_virtqueue(0, VQ0_SIZE);
        desc.request_virtqueue(1, VQ1_SIZE);
        if (shm_size > 0) {
            EXPECT_TRUE(desc.request_shm(SHM_ID, shm_size));
        }
    }

    virtual bool notify(u32 vqid) override {
        kicks.push_back(vqid);
        if (!accept_notify)
            return false;

        vq_message msg;
        while (virtio_in->get(vqid, msg)) {
            vector<u8> data(msg.length_in());
            msg.copy_in(data);
            for (u8& val : data)
                val = ~val;

            size_t len = min<size_t>(data.size(), msg.length_out());
            msg.copy_out((const void*)data.data(), len);
            msg.trim(len);
            if (!virtio_in->put(vqid, msg))
                return false;
        }

        return true;
    }

    virtual void reset() override { resets++; }

    virtual void read_features(u64& features) override { features = FEATURES; }

    virtual bool write_features(u64 features) override {
        driver_features = features;
        return accept_features;
    }

    virtual bool read_config(const range& addr, void* data) override {
        if (addr.end >= config.size())
            return false;
        memcpy(data, config.data() + addr.start, addr.length());
        return true;
    }

    virtual bool write_config(const range& addr, const void* data) override {
        if (addr.end >= config.size())
            return false;
        memcpy(config.data() + addr.start, data, addr.length());
        return true;
    }

    virtual bool read_shm(u32 shmid, const range& addr, void* data) override {
        if (shmid != SHM_ID || addr.end >= shm.size())
            return false;
        memcpy(data, shm.data() + addr.start, addr.length());
        return true;
    }

    virtual bool write_shm(u32 shmid, const range& addr,
                           const void* data) override {
        if (shmid != SHM_ID || addr.end >= shm.size())
            return false;
        memcpy(shm.data() + addr.start, data, addr.length());
        return true;
    }
};

struct virtio_buffer {
    u64 addr;
    u32 len;
    bool write;
};

class virtio_ring_memory
{
protected:
    u8* m_mem;

    template <typename T>
    T get(u64 addr) const {
        T val;
        memcpy(&val, m_mem + addr, sizeof(T));
        return val;
    }

    template <typename T>
    void put(u64 addr, T val) {
        memcpy(m_mem + addr, &val, sizeof(T));
    }

public:
    virtio_ring_memory(u8* mem): m_mem(mem) {}
    virtual ~virtio_ring_memory() = default;
};

class virtio_split_driver : public virtio_ring_memory
{
public:
    enum : u16 {
        F_NEXT = bit(0),
        F_WRITE = bit(1),
        F_INDIRECT = bit(2),
    };

    const u32 size;
    const u64 desc;
    const u64 avail;
    const u64 used;

    u16 next;
    u16 last_used;

    virtio_split_driver(u8* mem, u64 base, u32 sz):
        virtio_ring_memory(mem),
        size(sz),
        desc(base),
        avail(base + 0x400),
        used(base + 0x800),
        next(0),
        last_used(0) {
        memset(m_mem + base, 0, 0xc00);
    }

    void write_desc(u64 table, u16 idx, u64 addr, u32 len, u16 flags,
                    u16 link) {
        put<u64>(table + idx * 16ull + 0, addr);
        put<u32>(table + idx * 16ull + 8, len);
        put<u16>(table + idx * 16ull + 12, flags);
        put<u16>(table + idx * 16ull + 14, link);
    }

    u16 avail_idx() const { return get<u16>(avail + 2); }
    u16 used_idx() const { return get<u16>(used + 2); }

    void set_no_interrupt(bool set) { put<u16>(avail, set ? 1 : 0); }
    void set_used_event(u16 idx) { put<u16>(avail + 4 + 2ull * size, idx); }
    u16 avail_event() const { return get<u16>(used + 4 + 8ull * size); }

    void publish(u16 head) {
        u16 idx = avail_idx();
        put<u16>(avail + 4 + 2ull * (idx % size), head);
        put<u16>(avail + 2, idx + 1);
    }

    u16 chain(const vector<virtio_buffer>& bufs) {
        u16 head = next;
        for (size_t i = 0; i < bufs.size(); i++) {
            u16 idx = next;
            next = (next + 1) % size;
            u16 flags = bufs[i].write ? +F_WRITE : 0;
            if (i + 1 < bufs.size())
                flags |= F_NEXT;
            write_desc(desc, idx, bufs[i].addr, bufs[i].len, flags, next);
        }

        return head;
    }

    u16 add(const vector<virtio_buffer>& bufs) {
        u16 head = chain(bufs);
        publish(head);
        return head;
    }

    u16 add_indirect(u64 table, const vector<virtio_buffer>& bufs) {
        for (size_t i = 0; i < bufs.size(); i++) {
            u16 flags = bufs[i].write ? +F_WRITE : 0;
            if (i + 1 < bufs.size())
                flags |= F_NEXT;
            write_desc(table, i, bufs[i].addr, bufs[i].len, flags, i + 1);
        }

        u16 head = next;
        next = (next + 1) % size;
        write_desc(desc, head, table, bufs.size() * 16, F_INDIRECT, 0);
        publish(head);
        return head;
    }

    bool pop_used(u32& id, u32& len) {
        if (last_used == used_idx())
            return false;
        u64 elem = used + 4 + 8ull * (last_used % size);
        id = get<u32>(elem);
        len = get<u32>(elem + 4);
        last_used++;
        return true;
    }
};

class virtio_packed_driver : public virtio_ring_memory
{
public:
    enum : u16 {
        F_NEXT = bit(0),
        F_WRITE = bit(1),
        F_INDIRECT = bit(2),
        F_AVAIL = bit(7),
        F_USED = bit(15),
        EVENT_ENABLE = 0,
        EVENT_DISABLE = 1,
        EVENT_DESC = 2,
    };

    const u32 size;
    const u64 desc;
    const u64 driver;
    const u64 device;

    u16 next;
    bool wrap;
    u16 next_id;
    u16 last_used;
    bool used_wrap;

    virtio_packed_driver(u8* mem, u64 base, u32 sz):
        virtio_ring_memory(mem),
        size(sz),
        desc(base),
        driver(base + 0x400),
        device(base + 0x800),
        next(0),
        wrap(true),
        next_id(0),
        last_used(0),
        used_wrap(true) {
        memset(m_mem + base, 0, 0xc00);
    }

    u16 avail_flags() const { return wrap ? +F_AVAIL : +F_USED; }

    void write_desc(u64 table, u16 idx, u64 addr, u32 len, u16 id, u16 flags) {
        put<u64>(table + idx * 16ull + 0, addr);
        put<u32>(table + idx * 16ull + 8, len);
        put<u16>(table + idx * 16ull + 12, id);
        put<u16>(table + idx * 16ull + 14, flags);
    }

    void set_driver_event(u16 flags, u16 off_wrap = 0) {
        put<u16>(driver, off_wrap);
        put<u16>(driver + 2, flags);
    }

    void advance() {
        if (++next >= size) {
            next = 0;
            wrap = !wrap;
        }
    }

    u16 add(const vector<virtio_buffer>& bufs) {
        u16 id = next_id++;
        for (size_t i = 0; i < bufs.size(); i++) {
            u16 flags = avail_flags();
            if (bufs[i].write)
                flags |= F_WRITE;
            if (i + 1 < bufs.size())
                flags |= F_NEXT;
            write_desc(desc, next, bufs[i].addr, bufs[i].len, id, flags);
            advance();
        }

        return id;
    }

    u16 add_indirect(u64 table, const vector<virtio_buffer>& bufs) {
        u16 id = next_id++;
        for (size_t i = 0; i < bufs.size(); i++) {
            u16 flags = bufs[i].write ? +F_WRITE : 0;
            write_desc(table, i, bufs[i].addr, bufs[i].len, 0, flags);
        }

        write_desc(desc, next, table, bufs.size() * 16, id,
                   avail_flags() | F_INDIRECT);
        advance();
        return id;
    }

    bool pop_used(u32& id, u32& len, u16 ndescs = 1) {
        u16 flags = get<u16>(desc + last_used * 16ull + 14);
        bool avail = flags & F_AVAIL;
        bool used = flags & F_USED;
        if (avail != used || used != used_wrap)
            return false;

        id = get<u16>(desc + last_used * 16ull + 12);
        len = get<u32>(desc + last_used * 16ull + 8);
        last_used += ndescs;
        if (last_used >= size) {
            last_used -= size;
            used_wrap = !used_wrap;
        }

        return true;
    }
};

#endif
