/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "virtio_transport.h"

enum : u64 {
    MEM_SIZE = 0x10000,
    MMIO_BASE = 0x10000,
    MMIO_SIZE = 0x1000,
    SHM_BASE = 0x80000000,
    SHM_SIZE = 0x10000,
    SHM_REGION = 0x4000,
    RING0 = 0x1000,
    RING1 = 0x2000,
    INDIRECT = 0x3000,
    BUFFERS = 0x4000,
};

enum : u64 {
    REG_MAGIC = 0x00,
    REG_VERSION = 0x04,
    REG_DEVICE_ID = 0x08,
    REG_VENDOR_ID = 0x0c,
    REG_DEVICE_FEATURES = 0x10,
    REG_DEVICE_FEATURES_SEL = 0x14,
    REG_DRIVER_FEATURES = 0x20,
    REG_DRIVER_FEATURES_SEL = 0x24,
    REG_QUEUE_SEL = 0x30,
    REG_QUEUE_NUM_MAX = 0x34,
    REG_QUEUE_NUM = 0x38,
    REG_QUEUE_READY = 0x44,
    REG_QUEUE_NOTIFY = 0x50,
    REG_INTERRUPT_STATUS = 0x60,
    REG_INTERRUPT_ACK = 0x64,
    REG_STATUS = 0x70,
    REG_QUEUE_DESC_LO = 0x80,
    REG_QUEUE_DESC_HI = 0x84,
    REG_QUEUE_DRIVER_LO = 0x90,
    REG_QUEUE_DRIVER_HI = 0x94,
    REG_QUEUE_DEVICE_LO = 0xa0,
    REG_QUEUE_DEVICE_HI = 0xa4,
    REG_SHM_SEL = 0xac,
    REG_SHM_LEN_LO = 0xb0,
    REG_SHM_LEN_HI = 0xb4,
    REG_SHM_BASE_LO = 0xb8,
    REG_SHM_BASE_HI = 0xbc,
    REG_QUEUE_RESET = 0xc0,
    REG_CONFIG_GEN = 0xfc,
    REG_CONFIG = 0x100,
};

enum : u32 {
    STATUS_ACK = VIRTIO_STATUS_ACKNOWLEDGE,
    STATUS_DRIVER = VIRTIO_STATUS_DRIVER,
    STATUS_DRIVER_OK = VIRTIO_STATUS_DRIVER_OK,
    STATUS_FEATURES_OK = VIRTIO_STATUS_FEATURES_OK,
    STATUS_NEEDS_RESET = VIRTIO_STATUS_DEVICE_NEEDS_RESET,
    IRQ_VQUEUE = VIRTIO_IRQSTATUS_VQUEUE,
    IRQ_CONFIG = VIRTIO_IRQSTATUS_CONFIG,
};

enum : u64 {
    F_INDIRECT_DESC = VIRTIO_F_RING_INDIRECT_DESC,
    F_EVENT_IDX = VIRTIO_F_RING_EVENT_IDX,
    F_VERSION_1 = VIRTIO_F_VERSION_1,
    F_PACKED = VIRTIO_F_RING_PACKED,
    F_RING_RESET = VIRTIO_F_RING_RESET,
    F_DEVICE = virtio_echo::FEATURES,
    F_COMMON = F_DEVICE | F_INDIRECT_DESC | F_EVENT_IDX | F_VERSION_1 |
               F_RING_RESET,
};

class virtio_mmio_test : public test_base
{
public:
    enum : size_t {
        SPLIT = 0,
        PACKED = 1,
        SHARED = 2,
        NDEVS = 3,
    };

    generic::bus bus;
    generic::memory mem;
    virtio::mmio mmio0;
    virtio::mmio mmio1;
    virtio::mmio mmio2;
    virtio_echo echo0;
    virtio_echo echo1;
    virtio_echo echo2;

    tlm_initiator_socket out;
    tlm_initiator_socket shm;
    gpio_target_array<NDEVS> irq;

    virtio::mmio* mmios[NDEVS];
    virtio_echo* echos[NDEVS];
    u64 next_buffer;

    virtio_mmio_test(const sc_module_name& nm):
        test_base(nm),
        bus("bus"),
        mem("mem", MEM_SIZE),
        mmio0("mmio0"),
        mmio1("mmio1"),
        mmio2("mmio2"),
        echo0("echo0"),
        echo1("echo1"),
        echo2("echo2", SHM_REGION),
        out("out"),
        shm("shm"),
        irq("irq"),
        mmios{ &mmio0, &mmio1, &mmio2 },
        echos{ &echo0, &echo1, &echo2 },
        next_buffer(BUFFERS) {
        bus.bind(out);
        bus.bind(mem.in, 0, MEM_SIZE - 1);
        rst.bind(bus.rst);
        clk.bind(bus.clk);
        rst.bind(mem.rst);
        clk.bind(mem.clk);

        for (size_t i = 0; i < NDEVS; i++) {
            u64 base = MMIO_BASE + i * MMIO_SIZE;
            bus.bind(mmios[i]->in, base, base + MMIO_SIZE - 1);
            bus.bind(mmios[i]->out);
            mmios[i]->virtio_out.bind(echos[i]->virtio_in);
            mmios[i]->irq.bind(irq[i]);
            rst.bind(mmios[i]->rst);
            clk.bind(mmios[i]->clk);
        }

        shm.bind(mmio2.shm);

        add_test("strings", &virtio_mmio_test::test_strings);
        add_test("identification", &virtio_mmio_test::test_identification);
        add_test("features", &virtio_mmio_test::test_features);
        add_test("status", &virtio_mmio_test::test_status);
        add_test("queue_setup", &virtio_mmio_test::test_queue_setup);
        add_test("queue_errors", &virtio_mmio_test::test_queue_errors);
        add_test("transfer", &virtio_mmio_test::test_transfer);
        add_test("chains", &virtio_mmio_test::test_chains);
        add_test("indirect", &virtio_mmio_test::test_indirect);
        add_test("interrupts", &virtio_mmio_test::test_interrupts);
        add_test("event_idx", &virtio_mmio_test::test_event_idx);
        add_test("notify", &virtio_mmio_test::test_notify);
        add_test("ring_errors", &virtio_mmio_test::test_ring_errors);
        add_test("config", &virtio_mmio_test::test_config);
        add_test("queue_reset", &virtio_mmio_test::test_queue_reset);
        add_test("shared_memory", &virtio_mmio_test::test_shared_memory);
        add_test("packed", &virtio_mmio_test::test_packed);
        add_test("packed_events", &virtio_mmio_test::test_packed_events);
        add_test("packed_indirect", &virtio_mmio_test::test_packed_indirect);
    }

    u32 read(size_t dev, u64 reg) {
        u32 data = 0;
        EXPECT_OK(out.readw(MMIO_BASE + dev * MMIO_SIZE + reg, data))
            << "reading register 0x" << std::hex << reg;
        return data;
    }

    void write(size_t dev, u64 reg, u32 data) {
        EXPECT_OK(out.writew(MMIO_BASE + dev * MMIO_SIZE + reg, data))
            << "writing register 0x" << std::hex << reg;
    }

    u64 device_features(size_t dev) {
        write(dev, REG_DEVICE_FEATURES_SEL, 1);
        u64 features = (u64)read(dev, REG_DEVICE_FEATURES) << 32;
        write(dev, REG_DEVICE_FEATURES_SEL, 0);
        return features | read(dev, REG_DEVICE_FEATURES);
    }

    void negotiate(size_t dev, u64 features) {
        write(dev, REG_STATUS, 0);
        write(dev, REG_STATUS, STATUS_ACK);
        write(dev, REG_STATUS, STATUS_ACK | STATUS_DRIVER);
        write(dev, REG_DRIVER_FEATURES_SEL, 0);
        write(dev, REG_DRIVER_FEATURES, (u32)features);
        write(dev, REG_DRIVER_FEATURES_SEL, 1);
        write(dev, REG_DRIVER_FEATURES, (u32)(features >> 32));
        write(dev, REG_STATUS,
              STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
    }

    void setup_queue(size_t dev, u32 vqid, u64 base, u32 size) {
        write(dev, REG_QUEUE_SEL, vqid);
        write(dev, REG_QUEUE_NUM, size);
        write(dev, REG_QUEUE_DESC_LO, base);
        write(dev, REG_QUEUE_DESC_HI, 0);
        write(dev, REG_QUEUE_DRIVER_LO, base + 0x400);
        write(dev, REG_QUEUE_DRIVER_HI, 0);
        write(dev, REG_QUEUE_DEVICE_LO, base + 0x800);
        write(dev, REG_QUEUE_DEVICE_HI, 0);
        write(dev, REG_QUEUE_READY, 1);
    }

    void start(size_t dev, u64 features) {
        next_buffer = BUFFERS;
        memset(mem.data(), 0, MEM_SIZE);
        echos[dev]->kicks.clear();
        negotiate(dev, features);
        EXPECT_EQ(read(dev, REG_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
        setup_queue(dev, 0, RING0, virtio_echo::VQ0_SIZE);
        setup_queue(dev, 1, RING1, virtio_echo::VQ1_SIZE);
        write(dev, REG_STATUS,
              STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK |
                  STATUS_DRIVER_OK);
        EXPECT_FALSE(irq[dev]);
    }

    void kick(size_t dev, u32 vqid) { write(dev, REG_QUEUE_NOTIFY, vqid); }

    void ack(size_t dev) {
        write(dev, REG_INTERRUPT_ACK, read(dev, REG_INTERRUPT_STATUS));
        EXPECT_FALSE(irq[dev]);
    }

    u64 alloc(size_t len) {
        u64 addr = next_buffer;
        next_buffer += (len + 15) & ~15ull;
        VCML_ERROR_ON(next_buffer > MEM_SIZE, "out of test memory");
        return addr;
    }

    virtio_buffer input(const vector<u8>& data) {
        u64 addr = alloc(data.size());
        memcpy(mem.data() + addr, data.data(), data.size());
        return { addr, (u32)data.size(), false };
    }

    virtio_buffer output(size_t len) {
        u64 addr = alloc(len);
        memset(mem.data() + addr, 0xee, len);
        return { addr, (u32)len, true };
    }

    vector<u8> content(const virtio_buffer& buf, size_t len) {
        return vector<u8>(mem.data() + buf.addr, mem.data() + buf.addr + len);
    }

    static vector<u8> pattern(size_t len, u8 seed) {
        vector<u8> data(len);
        for (size_t i = 0; i < len; i++)
            data[i] = (u8)(seed + i);
        return data;
    }

    static vector<u8> inverted(vector<u8> data) {
        for (u8& val : data)
            val = ~val;
        return data;
    }

    void test_strings() {
        EXPECT_STREQ(mmio0.kind(), "vcml::virtio::mmio");
        EXPECT_STREQ(mmio0.module::version(), VCML_VERSION_STRING);
        EXPECT_STREQ(bus.kind(), "vcml::generic::bus");
        EXPECT_STREQ(bus.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(mem.kind(), "vcml::generic::memory");
        EXPECT_STREQ(mem.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(mmio0.version.name(), "test.mmio0.version");
        EXPECT_FALSE(mmio0.use_packed_queues);
        EXPECT_TRUE(mmio1.use_packed_queues);
        EXPECT_EQ(mmio2.shm_size, SHM_SIZE);
    }

    void test_identification() {
        for (size_t dev = 0; dev < NDEVS; dev++) {
            EXPECT_EQ(read(dev, REG_MAGIC), fourcc("virt"));
            EXPECT_EQ(read(dev, REG_VERSION), 2);
            EXPECT_EQ(read(dev, REG_DEVICE_ID), virtio_echo::DEVICE_ID);
            EXPECT_EQ(read(dev, REG_VENDOR_ID), VIRTIO_VENDOR_VCML);
            EXPECT_EQ(read(dev, REG_STATUS), 0);
            EXPECT_EQ(read(dev, REG_INTERRUPT_STATUS), 0);
            EXPECT_EQ(read(dev, REG_CONFIG_GEN), 0);
        }

        u32 data = 0;
        for (u64 reg : { REG_MAGIC, REG_VERSION, REG_DEVICE_ID, REG_VENDOR_ID,
                         REG_DEVICE_FEATURES, REG_QUEUE_NUM_MAX,
                         REG_INTERRUPT_STATUS, REG_CONFIG_GEN }) {
            EXPECT_CE(out.writew(MMIO_BASE + reg, data)) << std::hex << reg;
        }

        for (u64 reg :
             { REG_DRIVER_FEATURES, REG_QUEUE_SEL, REG_QUEUE_NUM,
               REG_QUEUE_NOTIFY, REG_INTERRUPT_ACK, REG_QUEUE_DESC_LO }) {
            EXPECT_CE(out.readw(MMIO_BASE + reg, data)) << std::hex << reg;
        }

        EXPECT_EQ(read(SPLIT, 0x1c), 0xffffffff);
        write(SPLIT, 0x1c, 0);
    }

    void test_features() {
        EXPECT_EQ(device_features(SPLIT), F_COMMON);
        EXPECT_EQ(device_features(PACKED), F_COMMON | F_PACKED);
        EXPECT_EQ(device_features(SHARED), F_COMMON);

        negotiate(SPLIT, F_DEVICE | F_VERSION_1);
        EXPECT_EQ(read(SPLIT, REG_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
        EXPECT_EQ(echo0.driver_features, F_DEVICE | F_VERSION_1);
        EXPECT_TRUE(mmio0.has_feature(F_VERSION_1));
        EXPECT_FALSE(mmio0.has_feature(F_EVENT_IDX));

        write(SPLIT, REG_DRIVER_FEATURES_SEL, 0);
        write(SPLIT, REG_DRIVER_FEATURES, 0);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_NEEDS_RESET);

        negotiate(SPLIT, F_VERSION_1 | bit(1));
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_ACK | STATUS_DRIVER);
        negotiate(SPLIT, F_VERSION_1 | F_PACKED);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_ACK | STATUS_DRIVER);

        echo0.accept_features = false;
        negotiate(SPLIT, F_VERSION_1);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_ACK | STATUS_DRIVER);
        echo0.accept_features = true;
        negotiate(SPLIT, F_VERSION_1);
        EXPECT_EQ(read(SPLIT, REG_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
    }

    void test_status() {
        size_t resets = echo0.resets;
        start(SPLIT, F_VERSION_1);
        EXPECT_GT(echo0.resets, resets);
        EXPECT_TRUE(mmio0.device_ready());
        EXPECT_EQ(read(SPLIT, REG_STATUS) & 0xff00, 0);

        write(SPLIT, REG_QUEUE_SEL, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 1);

        resets = echo0.resets;
        write(SPLIT, REG_STATUS, 0);
        EXPECT_EQ(echo0.resets, resets + 1);
        EXPECT_EQ(read(SPLIT, REG_STATUS), 0);
        EXPECT_FALSE(mmio0.device_ready());
        write(SPLIT, REG_QUEUE_SEL, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);

        write(SPLIT, REG_STATUS, 0xffffff00 | STATUS_ACK);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_ACK);
        write(SPLIT, REG_STATUS,
              STATUS_ACK | STATUS_DRIVER | STATUS_DRIVER_OK);
        write(SPLIT, REG_STATUS, STATUS_ACK);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_ACK);

        vq_message msg;
        EXPECT_FALSE(echo0.virtio_in->get(0, msg));
        EXPECT_FALSE(echo0.virtio_in->put(0, msg));
        EXPECT_FALSE(echo0.virtio_in->notify());
    }

    void test_queue_setup() {
        negotiate(SPLIT, F_VERSION_1);
        write(SPLIT, REG_QUEUE_SEL, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_NUM_MAX), virtio_echo::VQ0_SIZE);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);
        write(SPLIT, REG_QUEUE_SEL, 1);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_NUM_MAX), virtio_echo::VQ1_SIZE);
        write(SPLIT, REG_QUEUE_SEL, 2);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_NUM_MAX), 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);
        write(SPLIT, REG_QUEUE_SEL, 1);
        write(SPLIT, REG_QUEUE_SEL, VIRTQUEUE_MAX);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_NUM_MAX), virtio_echo::VQ1_SIZE);

        setup_queue(SPLIT, 0, RING0, virtio_echo::VQ0_SIZE);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 1);
        write(SPLIT, REG_QUEUE_READY, 1);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 1);
        write(SPLIT, REG_QUEUE_SEL, 1);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);
        write(SPLIT, REG_QUEUE_SEL, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 1);

        write(SPLIT, REG_QUEUE_READY, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);
        write(SPLIT, REG_QUEUE_READY, 0);
        write(SPLIT, REG_QUEUE_READY, VIRTQUEUE_MAX);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);

        write(SPLIT, REG_QUEUE_SEL, 2);
        write(SPLIT, REG_QUEUE_READY, 1);
        write(SPLIT, REG_QUEUE_READY, 0);
        write(SPLIT, REG_QUEUE_RESET, 1);
        EXPECT_EQ(read(SPLIT, REG_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
    }

    void test_queue_errors() {
        negotiate(SPLIT, F_VERSION_1);
        setup_queue(SPLIT, 0, MEM_SIZE, virtio_echo::VQ0_SIZE);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_NEEDS_RESET);

        negotiate(SPLIT, F_VERSION_1);
        setup_queue(SPLIT, 0, 0, virtio_echo::VQ0_SIZE);
        write(SPLIT, REG_QUEUE_READY, 0);

        start(SPLIT, F_VERSION_1);
        write(SPLIT, REG_QUEUE_SEL, 1);
        write(SPLIT, REG_QUEUE_READY, 0);
        write(SPLIT, REG_QUEUE_NUM, 2 * virtio_echo::VQ1_SIZE);
        write(SPLIT, REG_QUEUE_READY, 1);

        virtio_split_driver ring(mem.data(), RING1, virtio_echo::VQ1_SIZE);
        vector<u8> data = pattern(4, 0x10);
        virtio_buffer in = input(data);
        virtio_buffer res = output(4);
        u16 head = ring.add({ in, res });
        kick(SPLIT, 1);

        u32 id = 0, len = 0;
        ASSERT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(id, head);
        EXPECT_EQ(content(res, 4), inverted(data));
        ack(SPLIT);
    }

    void test_transfer() {
        start(SPLIT, F_VERSION_1);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        u32 id = 0, len = 0;
        EXPECT_FALSE(ring.pop_used(id, len));

        for (u32 i = 0; i < 3 * virtio_echo::VQ0_SIZE; i++) {
            vector<u8> data = pattern(16 + i, i);
            virtio_buffer in = input(data);
            virtio_buffer res = output(64);
            u16 head = ring.add({ in, res });
            kick(SPLIT, 0);

            ASSERT_TRUE(ring.pop_used(id, len)) << "request " << i;
            EXPECT_EQ(id, head) << "request " << i;
            EXPECT_EQ(len, data.size()) << "request " << i;
            EXPECT_EQ(content(res, len), inverted(data)) << "request " << i;
            EXPECT_EQ(content(res, 64)[len], 0xee) << "request " << i;
            EXPECT_FALSE(ring.pop_used(id, len));
        }

        EXPECT_EQ(echo0.kicks, vector<u32>(3 * virtio_echo::VQ0_SIZE, 0));
        EXPECT_EQ(ring.used_idx(), 3 * virtio_echo::VQ0_SIZE);
        ack(SPLIT);
    }

    void test_chains() {
        start(SPLIT, F_VERSION_1);
        virtio_split_driver ring0(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        virtio_split_driver ring1(mem.data(), RING1, virtio_echo::VQ1_SIZE);

        vector<u8> data = pattern(48, 0x40);
        virtio_buffer in0 = input(vector<u8>(data.begin(), data.begin() + 8));
        virtio_buffer in1 = input(vector<u8>(data.begin() + 8, data.end()));
        virtio_buffer res0 = output(10);
        virtio_buffer res1 = output(20);
        virtio_buffer res2 = output(30);
        u16 head0 = ring0.add({ in0, in1, res0, res1, res2 });

        vector<u8> more = pattern(8, 0x80);
        virtio_buffer in2 = input(more);
        virtio_buffer res3 = output(4);
        u16 head1 = ring0.add({ in2, res3 });
        u16 head2 = ring0.add({ in2 });

        virtio_buffer res4 = output(8);
        u16 head3 = ring1.add({ in2, res4 });
        kick(SPLIT, 0);

        u32 id = 0, len = 0;
        ASSERT_TRUE(ring0.pop_used(id, len));
        EXPECT_EQ(id, head0);
        EXPECT_EQ(len, 48);
        vector<u8> result = content(res0, 10);
        vector<u8> part = content(res1, 20);
        result.insert(result.end(), part.begin(), part.end());
        part = content(res2, 18);
        result.insert(result.end(), part.begin(), part.end());
        EXPECT_EQ(result, inverted(data));
        EXPECT_EQ(content(res2, 30)[18], 0xee);

        ASSERT_TRUE(ring0.pop_used(id, len));
        EXPECT_EQ(id, head1);
        EXPECT_EQ(len, 4);
        EXPECT_EQ(content(res3, 4),
                  inverted(vector<u8>(more.begin(), more.begin() + 4)));

        ASSERT_TRUE(ring0.pop_used(id, len));
        EXPECT_EQ(id, head2);
        EXPECT_EQ(len, 0);
        EXPECT_FALSE(ring0.pop_used(id, len));
        EXPECT_FALSE(ring1.pop_used(id, len));

        kick(SPLIT, 1);
        ASSERT_TRUE(ring1.pop_used(id, len));
        EXPECT_EQ(id, head3);
        EXPECT_EQ(len, 8);
        EXPECT_EQ(content(res4, 8), inverted(more));
        EXPECT_EQ(echo0.kicks, vector<u32>({ 0, 1 }));
        ack(SPLIT);
    }

    void test_indirect() {
        start(SPLIT, F_VERSION_1 | F_INDIRECT_DESC);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);

        vector<u8> data = pattern(32, 0x21);
        virtio_buffer in0 = input(vector<u8>(data.begin(), data.begin() + 16));
        virtio_buffer in1 = input(vector<u8>(data.begin() + 16, data.end()));
        virtio_buffer res0 = output(16);
        virtio_buffer res1 = output(16);
        u16 head = ring.add_indirect(INDIRECT, { in0, in1, res0, res1 });
        kick(SPLIT, 0);

        u32 id = 0, len = 0;
        ASSERT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(id, head);
        EXPECT_EQ(len, 32);
        vector<u8> result = content(res0, 16);
        vector<u8> part = content(res1, 16);
        result.insert(result.end(), part.begin(), part.end());
        EXPECT_EQ(result, inverted(data));
        ack(SPLIT);
    }

    void test_interrupts() {
        start(SPLIT, F_VERSION_1);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        virtio_buffer in = input(pattern(4, 1));
        u32 id = 0, len = 0;

        ring.add({ in, output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(read(SPLIT, REG_INTERRUPT_STATUS), IRQ_VQUEUE);
        EXPECT_TRUE(irq[SPLIT]);
        EXPECT_FALSE(irq[PACKED]);

        write(SPLIT, REG_INTERRUPT_ACK, IRQ_CONFIG);
        EXPECT_EQ(read(SPLIT, REG_INTERRUPT_STATUS), IRQ_VQUEUE);
        EXPECT_TRUE(irq[SPLIT]);
        write(SPLIT, REG_INTERRUPT_ACK, IRQ_VQUEUE);
        EXPECT_EQ(read(SPLIT, REG_INTERRUPT_STATUS), 0);
        EXPECT_FALSE(irq[SPLIT]);

        ring.set_no_interrupt(true);
        ring.add({ in, output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(read(SPLIT, REG_INTERRUPT_STATUS), 0);
        EXPECT_FALSE(irq[SPLIT]);

        ring.set_no_interrupt(false);
        ring.add({ in, output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_TRUE(irq[SPLIT]);

        EXPECT_TRUE(echo0.virtio_in->notify());
        EXPECT_EQ(read(SPLIT, REG_INTERRUPT_STATUS), IRQ_VQUEUE | IRQ_CONFIG);
        write(SPLIT, REG_INTERRUPT_ACK, IRQ_VQUEUE);
        EXPECT_TRUE(irq[SPLIT]);
        write(SPLIT, REG_INTERRUPT_ACK, 0xfffffffc | IRQ_CONFIG);
        EXPECT_EQ(read(SPLIT, REG_INTERRUPT_STATUS), 0);
        EXPECT_FALSE(irq[SPLIT]);
    }

    void test_event_idx() {
        start(SPLIT, F_VERSION_1 | F_EVENT_IDX);
        EXPECT_TRUE(mmio0.has_feature(F_EVENT_IDX));
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        virtio_buffer in = input(pattern(4, 1));
        u32 id = 0, len = 0;

        ring.set_no_interrupt(true);
        ring.set_used_event(1);
        ring.add({ in, output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(ring.avail_event(), 1);
        EXPECT_FALSE(irq[SPLIT]);

        ring.add({ in, output(4) });
        ring.add({ in, output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(ring.avail_event(), 3);
        EXPECT_TRUE(irq[SPLIT]);
        ack(SPLIT);

        ring.add({ in, output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(ring.avail_event(), 4);
        EXPECT_FALSE(irq[SPLIT]);
    }

    void test_notify() {
        memset(mem.data(), 0, MEM_SIZE);
        negotiate(SPLIT, F_VERSION_1);
        setup_queue(SPLIT, 0, RING0, virtio_echo::VQ0_SIZE);
        echo0.kicks.clear();
        kick(SPLIT, 0);
        EXPECT_TRUE(echo0.kicks.empty());

        write(SPLIT, REG_STATUS,
              STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK |
                  STATUS_DRIVER_OK);
        kick(SPLIT, 1);
        kick(SPLIT, 5);
        kick(SPLIT, VIRTQUEUE_MAX);
        EXPECT_TRUE(echo0.kicks.empty());

        kick(SPLIT, 0);
        kick(SPLIT, 0xabcd0000);
        EXPECT_EQ(echo0.kicks, vector<u32>({ 0, 0 }));

        echo0.accept_notify = false;
        kick(SPLIT, 0);
        EXPECT_EQ(read(SPLIT, REG_STATUS), STATUS_NEEDS_RESET);
        echo0.accept_notify = true;
    }

    void test_ring_errors() {
        start(SPLIT, F_VERSION_1 | F_INDIRECT_DESC);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        vector<u8> data = pattern(4, 0x70);
        virtio_buffer in = input(data);
        u32 id = 0, len = 0;

        ring.publish(virtio_echo::VQ0_SIZE);
        kick(SPLIT, 0);
        EXPECT_FALSE(ring.pop_used(id, len));

        ring.write_desc(ring.desc, 0, in.addr, in.len, ring.F_NEXT,
                        virtio_echo::VQ0_SIZE);
        ring.publish(0);
        kick(SPLIT, 0);
        EXPECT_FALSE(ring.pop_used(id, len));

        ring.write_desc(ring.desc, 0, in.addr, in.len, ring.F_NEXT, 0);
        ring.publish(0);
        kick(SPLIT, 0);
        EXPECT_FALSE(ring.pop_used(id, len));

        ring.write_desc(ring.desc, 0, MEM_SIZE, 16, 0, 0);
        ring.publish(0);
        kick(SPLIT, 0);
        EXPECT_FALSE(ring.pop_used(id, len));

        ring.write_desc(ring.desc, 0, MEM_SIZE - 16, 4096, ring.F_WRITE, 0);
        ring.publish(0);
        kick(SPLIT, 0);
        EXPECT_FALSE(ring.pop_used(id, len));

        ring.write_desc(ring.desc, 0, INDIRECT, 0, ring.F_INDIRECT, 0);
        ring.publish(0);
        kick(SPLIT, 0);
        ring.write_desc(ring.desc, 1, INDIRECT, 17, ring.F_INDIRECT, 0);
        ring.publish(1);
        kick(SPLIT, 0);
        ring.write_desc(ring.desc, 2, MEM_SIZE, 16, ring.F_INDIRECT, 0);
        ring.publish(2);
        kick(SPLIT, 0);
        EXPECT_FALSE(ring.pop_used(id, len));
        EXPECT_EQ(ring.used_idx(), 0);

        virtio_buffer res = output(4);
        ring.write_desc(ring.desc, 3, res.addr, res.len, ring.F_WRITE, 0);
        ring.write_desc(ring.desc, 4, in.addr, in.len, ring.F_NEXT, 3);
        ring.publish(4);
        kick(SPLIT, 0);
        ASSERT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(id, 4);
        EXPECT_EQ(len, 4);
        EXPECT_EQ(content(res, 4), inverted(data));

        ring.write_desc(ring.desc, 5, res.addr, res.len,
                        ring.F_WRITE | ring.F_NEXT, 6);
        ring.write_desc(ring.desc, 6, in.addr, in.len, 0, 0);
        ring.publish(5);
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));

        vq_message msg;
        msg.index = virtio_echo::VQ0_SIZE;
        EXPECT_FALSE(echo0.virtio_in->put(0, msg));
        EXPECT_FALSE(echo0.virtio_in->get(7, msg));
        EXPECT_FALSE(echo0.virtio_in->put(7, msg));
        ack(SPLIT);
    }

    void test_config() {
        negotiate(SPLIT, F_VERSION_1);
        for (u32 i = 0; i < 16; i += 4) {
            EXPECT_EQ(read(SPLIT, REG_CONFIG + i),
                      0xc0c0c0c0 + 0x03020100 + i * 0x01010101);
        }

        u8 byte = 0;
        EXPECT_OK(out.readw(MMIO_BASE + REG_CONFIG + 5, byte));
        EXPECT_EQ(byte, 0xc5);

        EXPECT_EQ(read(SPLIT, REG_CONFIG_GEN), 0);
        write(SPLIT, REG_CONFIG + 4, 0x11223344);
        EXPECT_EQ(read(SPLIT, REG_CONFIG + 4), 0x11223344);
        EXPECT_EQ(read(SPLIT, REG_CONFIG_GEN), 1);
        EXPECT_EQ(echo0.config[4], 0x44);

        u32 data = 0;
        EXPECT_AE(out.readw(MMIO_BASE + REG_CONFIG + 16, data));
        EXPECT_AE(out.writew(MMIO_BASE + REG_CONFIG + 16, data));
        EXPECT_EQ(read(SPLIT, REG_CONFIG_GEN), 1);

        start(SPLIT, F_VERSION_1);
        EXPECT_EQ(read(SPLIT, REG_CONFIG + 4), 0x11223344);
        EXPECT_CE(out.writew(MMIO_BASE + REG_CONFIG + 4, data));
        EXPECT_EQ(read(SPLIT, REG_CONFIG + 4), 0x11223344);

        write(SPLIT, REG_STATUS, 0);
        write(SPLIT, REG_CONFIG + 4, 0xc7c6c5c4);
    }

    void test_queue_reset() {
        start(SPLIT, F_VERSION_1 | F_RING_RESET);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        u32 id = 0, len = 0;

        ring.add({ input(pattern(4, 1)), output(4) });
        kick(SPLIT, 0);
        EXPECT_TRUE(ring.pop_used(id, len));
        ack(SPLIT);

        write(SPLIT, REG_QUEUE_SEL, 0);
        write(SPLIT, REG_QUEUE_RESET, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 1);
        write(SPLIT, REG_QUEUE_RESET, 1);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_RESET), 0);
        write(SPLIT, REG_QUEUE_SEL, 0);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 0);
        write(SPLIT, REG_QUEUE_RESET, 1);

        echo0.kicks.clear();
        kick(SPLIT, 0);
        EXPECT_TRUE(echo0.kicks.empty());

        virtio_split_driver fresh(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        setup_queue(SPLIT, 0, RING0, virtio_echo::VQ0_SIZE);
        vector<u8> data = pattern(4, 9);
        virtio_buffer res = output(4);
        fresh.add({ input(data), res });
        kick(SPLIT, 0);
        ASSERT_TRUE(fresh.pop_used(id, len));
        EXPECT_EQ(content(res, 4), inverted(data));

        write(SPLIT, REG_QUEUE_SEL, 1);
        EXPECT_EQ(read(SPLIT, REG_QUEUE_READY), 1);
        ack(SPLIT);
    }

    void test_shared_memory() {
        write(SPLIT, REG_SHM_SEL, virtio_echo::SHM_ID);
        EXPECT_EQ(read(SPLIT, REG_SHM_SEL), 0xffffffff);
        EXPECT_EQ(read(SPLIT, REG_SHM_LEN_LO), 0xffffffff);
        EXPECT_EQ(read(SPLIT, REG_SHM_LEN_HI), 0xffffffff);
        EXPECT_EQ(read(SPLIT, REG_SHM_BASE_LO), 0xffffffff);
        EXPECT_EQ(read(SPLIT, REG_SHM_BASE_HI), 0xffffffff);
        u8 obj[0x100];
        EXPECT_FALSE(echo0.virtio_in->shm_map(1, 1, 0, obj, sizeof(obj)));
        EXPECT_FALSE(echo0.virtio_in->shm_unmap(1, 1));

        write(SHARED, REG_SHM_SEL, virtio_echo::SHM_ID);
        EXPECT_EQ(read(SHARED, REG_SHM_SEL), virtio_echo::SHM_ID);
        EXPECT_EQ(read(SHARED, REG_SHM_LEN_LO), SHM_REGION);
        EXPECT_EQ(read(SHARED, REG_SHM_LEN_HI), 0);
        EXPECT_EQ(read(SHARED, REG_SHM_BASE_LO), (u32)SHM_BASE);
        EXPECT_EQ(read(SHARED, REG_SHM_BASE_HI), (u32)(SHM_BASE >> 32));

        write(SHARED, REG_SHM_SEL, 7);
        EXPECT_EQ(read(SHARED, REG_SHM_LEN_LO), 0xffffffff);
        EXPECT_EQ(read(SHARED, REG_SHM_LEN_HI), 0xffffffff);

        u32 data = 0x12345678;
        EXPECT_OK(shm.writew(0x20, data));
        EXPECT_EQ(echo2.shm[0x20], 0x78);
        EXPECT_EQ(echo2.shm[0x23], 0x12);
        data = 0;
        EXPECT_OK(shm.readw(0x20, data));
        EXPECT_EQ(data, 0x12345678);
        EXPECT_AE(shm.readw(SHM_REGION, data));
        EXPECT_AE(shm.writew(SHM_REGION, data));

        memset(obj, 0x5a, sizeof(obj));
        EXPECT_TRUE(echo2.virtio_in->shm_map(1, 5, 0x1000, obj, sizeof(obj)));
        EXPECT_FALSE(echo2.virtio_in->shm_map(1, 5, 0x2000, obj, 16));
        EXPECT_FALSE(echo2.virtio_in->shm_map(1, 6, 0x10f0, obj, 32));
        EXPECT_FALSE(echo2.virtio_in->shm_map(9, 7, 0, obj, 16));

        EXPECT_OK(shm.readw(0x1010, data));
        EXPECT_EQ(data, 0x5a5a5a5a);
        data = 0xaabbccdd;
        EXPECT_OK(shm.writew(0x1010, data));
        EXPECT_EQ(obj[0x10], 0xdd);
        EXPECT_EQ(obj[0x13], 0xaa);
        EXPECT_EQ(echo2.shm[0x1010], 0);

        EXPECT_TRUE(echo2.virtio_in->shm_unmap(1, 5));
        EXPECT_FALSE(echo2.virtio_in->shm_unmap(1, 5));
        EXPECT_FALSE(echo2.virtio_in->shm_unmap(9, 5));
        data = 0x01020304;
        EXPECT_OK(shm.writew(0x1010, data));
        EXPECT_EQ(obj[0x10], 0xdd);
        EXPECT_EQ(echo2.shm[0x1010], 0x04);

        data = 0;
        EXPECT_OK(shm.writew(0x20, data));
        EXPECT_OK(shm.writew(0x1010, data));
    }

    void test_packed() {
        start(PACKED, F_VERSION_1 | F_PACKED);
        EXPECT_TRUE(mmio1.has_feature(F_PACKED));
        virtio_packed_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        u32 id = 0, len = 0;
        EXPECT_FALSE(ring.pop_used(id, len));

        for (u32 i = 0; i < 3 * virtio_echo::VQ0_SIZE; i++) {
            vector<u8> data = pattern(8 + i, i);
            virtio_buffer in = input(data);
            virtio_buffer res = output(64);
            u16 head = ring.add({ in, res });
            kick(PACKED, 0);

            ASSERT_TRUE(ring.pop_used(id, len, 2)) << "request " << i;
            EXPECT_EQ(id, head) << "request " << i;
            EXPECT_EQ(len, data.size()) << "request " << i;
            EXPECT_EQ(content(res, data.size()), inverted(data))
                << "request " << i;
            EXPECT_FALSE(ring.pop_used(id, len));
            EXPECT_TRUE(irq[PACKED]);
            ack(PACKED);
        }

        vector<u8> data = pattern(24, 0x55);
        virtio_buffer in0 = input(vector<u8>(data.begin(), data.begin() + 8));
        virtio_buffer in1 = input(vector<u8>(data.begin() + 8, data.end()));
        virtio_buffer res0 = output(12);
        virtio_buffer res1 = output(12);
        u16 head0 = ring.add({ in0, in1, res0, res1 });
        u16 head1 = ring.add({ in0 });
        kick(PACKED, 0);

        ASSERT_TRUE(ring.pop_used(id, len, 4));
        EXPECT_EQ(id, head0);
        EXPECT_EQ(len, 24);
        vector<u8> result = content(res0, 12);
        vector<u8> part = content(res1, 12);
        result.insert(result.end(), part.begin(), part.end());
        EXPECT_EQ(result, inverted(data));
        ASSERT_TRUE(ring.pop_used(id, len, 1));
        EXPECT_EQ(id, head1);
        EXPECT_EQ(len, 0);
        EXPECT_FALSE(irq[SPLIT]);
        ack(PACKED);
    }

    void test_packed_events() {
        start(PACKED, F_VERSION_1 | F_PACKED | F_EVENT_IDX);
        virtio_packed_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        virtio_buffer in = input(pattern(4, 3));
        u32 id = 0, len = 0;

        ring.set_driver_event(ring.EVENT_DISABLE);
        ring.add({ in, output(4) });
        kick(PACKED, 0);
        EXPECT_TRUE(ring.pop_used(id, len, 2));
        EXPECT_FALSE(irq[PACKED]);

        ring.set_driver_event(ring.EVENT_ENABLE);
        ring.add({ in, output(4) });
        kick(PACKED, 0);
        EXPECT_TRUE(ring.pop_used(id, len, 2));
        EXPECT_TRUE(irq[PACKED]);
        ack(PACKED);

        ring.set_driver_event(ring.EVENT_DESC, 6);
        ring.add({ in, output(4) });
        kick(PACKED, 0);
        EXPECT_TRUE(ring.pop_used(id, len, 2));
        EXPECT_FALSE(irq[PACKED]);
        ring.add({ in, output(4) });
        kick(PACKED, 0);
        EXPECT_TRUE(ring.pop_used(id, len, 2));
        EXPECT_TRUE(irq[PACKED]);
        ack(PACKED);
    }

    void test_packed_indirect() {
        start(PACKED, F_VERSION_1 | F_PACKED | F_INDIRECT_DESC);
        virtio_packed_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        u32 id = 0, len = 0;

        for (u32 i = 0; i < 2 * virtio_echo::VQ0_SIZE; i++) {
            vector<u8> data = pattern(32, 0x90 + i);
            virtio_buffer in0 = input(
                vector<u8>(data.begin(), data.begin() + 16));
            virtio_buffer in1 = input(
                vector<u8>(data.begin() + 16, data.end()));
            virtio_buffer res = output(32);
            u16 head = ring.add_indirect(INDIRECT, { in0, in1, res });
            kick(PACKED, 0);

            ASSERT_TRUE(ring.pop_used(id, len)) << "request " << i;
            EXPECT_EQ(id, head) << "request " << i;
            EXPECT_EQ(len, 32) << "request " << i;
            EXPECT_EQ(content(res, 32), inverted(data)) << "request " << i;
        }

        ring.write_desc(ring.desc, ring.next, INDIRECT, 17, 99,
                        ring.avail_flags() | ring.F_INDIRECT);
        kick(PACKED, 0);
        EXPECT_FALSE(ring.pop_used(id, len));
        ack(PACKED);
    }
};

TEST(virtio, mmio) {
    broker broker("test");
    broker.define("test.mmio1.use_packed_queues", "true");
    broker.define("test.mmio2.shm_size", to_string((u64)SHM_SIZE));
    broker.define("test.mmio2.shm_base", to_string((u64)SHM_BASE));

    virtio_mmio_test test("test");
    sc_core::sc_start();
}
