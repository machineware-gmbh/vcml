/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "virtio_transport.h"

enum : u64 {
    MEM_SIZE = 0x10000,
    MMAP_PCI_CFG_ADDR = 0x100000,
    MMAP_PCI_CFG_SIZE = 0x10000,
    MMAP_PCI_MMIO_ADDR = 0x200000,
    MMAP_PCI_MMIO_SIZE = 0x20000,
    BAR0_ADDR = MMAP_PCI_MMIO_ADDR,
    BAR2_ADDR = MMAP_PCI_MMIO_ADDR + 0x8000,
    BAR4_ADDR = MMAP_PCI_MMIO_ADDR + 0x10000,
    SHM_SIZE = 0x4000,
    RING0 = 0x1000,
    RING1 = 0x2000,
    BUFFERS = 0x4000,
};

enum : u64 {
    PCI_VENDOR_OFFSET = 0x00,
    PCI_DEVICE_OFFSET = 0x02,
    PCI_COMMAND_OFFSET = 0x04,
    PCI_BAR0_OFFSET = 0x10,
    PCI_BAR1_OFFSET = 0x14,
    PCI_BAR2_OFFSET = 0x18,
    PCI_BAR4_OFFSET = 0x20,
    PCI_BAR5_OFFSET = 0x24,
    PCI_SUBVENDOR_OFFSET = 0x2c,
    PCI_SUBDEVICE_OFFSET = 0x2e,
    PCI_CAP_OFFSET = 0x34,
};

enum : u64 {
    REG_DEVICE_FEATURE_SEL = BAR0_ADDR + 0x00,
    REG_DEVICE_FEATURE = BAR0_ADDR + 0x04,
    REG_DRIVER_FEATURE_SEL = BAR0_ADDR + 0x08,
    REG_DRIVER_FEATURE = BAR0_ADDR + 0x0c,
    REG_MSIX_CONFIG = BAR0_ADDR + 0x10,
    REG_NUM_QUEUES = BAR0_ADDR + 0x12,
    REG_DEVICE_STATUS = BAR0_ADDR + 0x14,
    REG_CONFIG_GEN = BAR0_ADDR + 0x15,
    REG_QUEUE_SEL = BAR0_ADDR + 0x16,
    REG_QUEUE_SIZE = BAR0_ADDR + 0x18,
    REG_QUEUE_MSIX_VECTOR = BAR0_ADDR + 0x1a,
    REG_QUEUE_ENABLE = BAR0_ADDR + 0x1c,
    REG_QUEUE_NOTIFY_OFF = BAR0_ADDR + 0x1e,
    REG_QUEUE_DESC = BAR0_ADDR + 0x20,
    REG_QUEUE_DRIVER = BAR0_ADDR + 0x28,
    REG_QUEUE_DEVICE = BAR0_ADDR + 0x30,
    REG_QUEUE_RESET = BAR0_ADDR + 0x3a,
    REG_QUEUE_NOTIFY = BAR0_ADDR + 0x1000,
    REG_IRQ_STATUS = BAR0_ADDR + 0x2000,
    REG_CONFIG = BAR0_ADDR + 0x3000,
};

enum : u8 {
    STATUS_ACK = VIRTIO_STATUS_ACKNOWLEDGE,
    STATUS_DRIVER = VIRTIO_STATUS_DRIVER,
    STATUS_DRIVER_OK = VIRTIO_STATUS_DRIVER_OK,
    STATUS_FEATURES_OK = VIRTIO_STATUS_FEATURES_OK,
    STATUS_NEEDS_RESET = VIRTIO_STATUS_DEVICE_NEEDS_RESET,
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

class virtio_pci_test : public test_base
{
public:
    tlm_initiator_socket mmio;
    generic::bus mmio_bus;
    generic::memory mem;
    pci::host pci_root;
    virtio::pci virtio_pci;
    virtio_echo echo;

    gpio_target_socket int_a;
    gpio_target_socket int_b;
    gpio_target_socket int_c;
    gpio_target_socket int_d;

    u64 next_buffer;

    virtio_pci_test(const sc_module_name& nm):
        test_base(nm),
        mmio("mmio"),
        mmio_bus("mmio_bus"),
        mem("mem", MEM_SIZE),
        pci_root("pci_root", false),
        virtio_pci("virtio_pci"),
        echo("echo", SHM_SIZE),
        int_a("int_a"),
        int_b("int_b"),
        int_c("int_c"),
        int_d("int_d"),
        next_buffer(BUFFERS) {
        pci_root.pci_out[0].bind(virtio_pci.pci_in);
        virtio_pci.virtio_out.bind(echo.virtio_in);

        const range mmap_mem(0, MEM_SIZE - 1);
        const range mmap_pci_cfg(MMAP_PCI_CFG_ADDR,
                                 MMAP_PCI_CFG_ADDR + MMAP_PCI_CFG_SIZE - 1);
        const range mmap_pci_mmio(MMAP_PCI_MMIO_ADDR,
                                  MMAP_PCI_MMIO_ADDR + MMAP_PCI_MMIO_SIZE - 1);

        mmio_bus.bind(mmio);
        mmio_bus.bind(pci_root.dma_out);
        mmio_bus.bind(mem.in, mmap_mem);
        mmio_bus.bind(pci_root.cfg_in, mmap_pci_cfg);
        mmio_bus.bind(pci_root.mmio_in[0], mmap_pci_mmio, MMAP_PCI_MMIO_ADDR);

        pci_root.irq_a.bind(int_a);
        pci_root.irq_b.bind(int_b);
        pci_root.irq_c.bind(int_c);
        pci_root.irq_d.bind(int_d);

        for (component* comp : std::initializer_list<component*>{
                 &mmio_bus, &mem, &pci_root, &virtio_pci }) {
            clk.bind(comp->clk);
            rst.bind(comp->rst);
        }

        add_test("identification", &virtio_pci_test::test_identification);
        add_test("capabilities", &virtio_pci_test::test_capabilities);
        add_test("features", &virtio_pci_test::test_features);
        add_test("status", &virtio_pci_test::test_status);
        add_test("queue_setup", &virtio_pci_test::test_queue_setup);
        add_test("transfer", &virtio_pci_test::test_transfer);
        add_test("interrupts", &virtio_pci_test::test_interrupts);
        add_test("msix", &virtio_pci_test::test_msix);
        add_test("notify", &virtio_pci_test::test_notify);
        add_test("config", &virtio_pci_test::test_config);
        add_test("queue_reset", &virtio_pci_test::test_queue_reset);
        add_test("shared_memory", &virtio_pci_test::test_shared_memory);
    }

    template <typename T>
    T pci_read_cfg(u64 offset) {
        T data = 0;
        EXPECT_OK(mmio.readw(MMAP_PCI_CFG_ADDR + offset, data))
            << "failed to read PCI config at offset " << std::hex << offset;
        return data;
    }

    template <typename T>
    void pci_write_cfg(u64 offset, T data) {
        EXPECT_OK(mmio.writew(MMAP_PCI_CFG_ADDR + offset, data))
            << "failed to write PCI config at offset " << std::hex << offset;
    }

    template <typename T>
    T read(u64 addr) {
        T data = 0;
        EXPECT_OK(mmio.readw(addr, data)) << "reading 0x" << std::hex << addr;
        return data;
    }

    template <typename T>
    void write(u64 addr, T data) {
        EXPECT_OK(mmio.writew(addr, data)) << "writing 0x" << std::hex << addr;
    }

    u8 find_cap(u8 cap_id, u8 cfg_type = 0) {
        u8 off = pci_read_cfg<u8>(PCI_CAP_OFFSET);
        while (off != 0) {
            bool match = pci_read_cfg<u8>(off) == cap_id;
            if (match && cap_id == PCI_CAPABILITY_VENDOR)
                match = pci_read_cfg<u8>(off + 3) == cfg_type;
            if (match)
                return off;
            off = pci_read_cfg<u8>(off + 1);
        }

        return 0;
    }

    void map_bars() {
        pci_write_cfg<u32>(PCI_BAR0_OFFSET, (u32)BAR0_ADDR | PCI_BAR_64);
        pci_write_cfg<u32>(PCI_BAR1_OFFSET, 0);
        pci_write_cfg<u32>(PCI_BAR2_OFFSET, (u32)BAR2_ADDR);
        pci_write_cfg<u32>(PCI_BAR4_OFFSET,
                           (u32)BAR4_ADDR | PCI_BAR_64 | PCI_BAR_PREFETCH);
        pci_write_cfg<u32>(PCI_BAR5_OFFSET, 0);
        pci_write_cfg<u16>(PCI_COMMAND_OFFSET, 7);
    }

    u64 device_features() {
        write<u32>(REG_DEVICE_FEATURE_SEL, 1);
        u64 features = (u64)read<u32>(REG_DEVICE_FEATURE) << 32;
        write<u32>(REG_DEVICE_FEATURE_SEL, 0);
        return features | read<u32>(REG_DEVICE_FEATURE);
    }

    void negotiate(u64 features) {
        map_bars();
        write<u8>(REG_DEVICE_STATUS, 0);
        write<u8>(REG_DEVICE_STATUS, STATUS_ACK);
        write<u8>(REG_DEVICE_STATUS, STATUS_ACK | STATUS_DRIVER);
        write<u32>(REG_DRIVER_FEATURE_SEL, 0);
        write<u32>(REG_DRIVER_FEATURE, (u32)features);
        write<u32>(REG_DRIVER_FEATURE_SEL, 1);
        write<u32>(REG_DRIVER_FEATURE, (u32)(features >> 32));
        write<u8>(REG_DEVICE_STATUS,
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
    }

    void setup_queue(u16 vqid, u64 base, u16 size) {
        write<u16>(REG_QUEUE_SEL, vqid);
        write<u16>(REG_QUEUE_SIZE, size);
        write<u64>(REG_QUEUE_DESC, base);
        write<u64>(REG_QUEUE_DRIVER, base + 0x400);
        write<u64>(REG_QUEUE_DEVICE, base + 0x800);
        write<u16>(REG_QUEUE_ENABLE, 1);
    }

    void start(u64 features) {
        next_buffer = BUFFERS;
        memset(mem.data(), 0, MEM_SIZE);
        echo.kicks.clear();
        negotiate(features);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
        setup_queue(0, RING0, virtio_echo::VQ0_SIZE);
        setup_queue(1, RING1, virtio_echo::VQ1_SIZE);
        write<u8>(REG_DEVICE_STATUS, STATUS_ACK | STATUS_DRIVER |
                                         STATUS_FEATURES_OK |
                                         STATUS_DRIVER_OK);
        EXPECT_FALSE(int_a);
    }

    void kick(u32 vqid) { write<u32>(REG_QUEUE_NOTIFY, vqid); }

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

    void test_identification() {
        EXPECT_STREQ(virtio_pci.kind(), "vcml::virtio::pci");
        EXPECT_EQ(pci_read_cfg<u16>(PCI_VENDOR_OFFSET), PCI_VENDOR_QUMRANET);
        EXPECT_EQ(pci_read_cfg<u16>(PCI_DEVICE_OFFSET),
                  PCI_DEVICE_VIRTIO + virtio_echo::DEVICE_ID);
        EXPECT_EQ(pci_read_cfg<u16>(PCI_SUBVENDOR_OFFSET),
                  (u16)VIRTIO_VENDOR_VCML);
        EXPECT_EQ(pci_read_cfg<u16>(PCI_SUBDEVICE_OFFSET),
                  virtio_echo::DEVICE_ID);

        map_bars();
        EXPECT_EQ(read<u16>(REG_NUM_QUEUES), 2);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), 0);
        EXPECT_EQ(read<u8>(REG_CONFIG_GEN), 0);
        EXPECT_EQ(read<u16>(REG_MSIX_CONFIG), VIRTIO_NO_VECTOR);
        EXPECT_EQ(read<u16>(REG_QUEUE_SEL), VIRTQUEUE_MAX);

        u16 data = 0;
        EXPECT_CE(mmio.writew(REG_NUM_QUEUES, data));
        EXPECT_AE(mmio.readw(BAR0_ADDR + 0x40, data));
        EXPECT_AE(mmio.writew(BAR0_ADDR + 0x40, data));
    }

    void test_capabilities() {
        struct capinfo {
            u8 type;
            u8 len;
            u8 bar;
            u32 offset;
            u32 length;
        };

        const capinfo caps[] = {
            { virtio::VIRTIO_PCI_CAP_COMMON, 16, 0, 0x0000, 0x1000 },
            { virtio::VIRTIO_PCI_CAP_NOTIFY, 20, 0, 0x1000, 0x1000 },
            { virtio::VIRTIO_PCI_CAP_ISR, 16, 0, 0x2000, 0x1000 },
            { virtio::VIRTIO_PCI_CAP_DEVICE, 16, 0, 0x3000, 0x1000 },
            { virtio::VIRTIO_PCI_CAP_SHM_CFG, 24, 4, 0x0000, SHM_SIZE },
        };

        for (const capinfo& cap : caps) {
            u8 off = find_cap(PCI_CAPABILITY_VENDOR, cap.type);
            ASSERT_NE(off, 0) << "capability " << (int)cap.type;
            EXPECT_EQ(pci_read_cfg<u8>(off + 2), cap.len) << (int)cap.type;
            EXPECT_EQ(pci_read_cfg<u8>(off + 4), cap.bar) << (int)cap.type;
            EXPECT_EQ(pci_read_cfg<u32>(off + 8), cap.offset) << (int)cap.type;
            EXPECT_EQ(pci_read_cfg<u32>(off + 12), cap.length)
                << (int)cap.type;
        }

        u8 notify = find_cap(PCI_CAPABILITY_VENDOR,
                             virtio::VIRTIO_PCI_CAP_NOTIFY);
        EXPECT_EQ(pci_read_cfg<u32>(notify + 16), 0);

        u8 shm = find_cap(PCI_CAPABILITY_VENDOR,
                          virtio::VIRTIO_PCI_CAP_SHM_CFG);
        EXPECT_EQ(pci_read_cfg<u8>(shm + 5), virtio_echo::SHM_ID);
        EXPECT_EQ(pci_read_cfg<u32>(shm + 16), 0);
        EXPECT_EQ(pci_read_cfg<u32>(shm + 20), 0);

        EXPECT_NE(find_cap(PCI_CAPABILITY_MSIX), 0);
        EXPECT_EQ(
            find_cap(PCI_CAPABILITY_VENDOR, virtio::VIRTIO_PCI_CAP_PCI_CFG),
            0);
    }

    void test_features() {
        map_bars();
        EXPECT_EQ(device_features(), F_COMMON);
        EXPECT_EQ(read<u32>(REG_DEVICE_FEATURE_SEL), 0);
        write<u32>(REG_DEVICE_FEATURE_SEL, 7);
        EXPECT_EQ(read<u32>(REG_DEVICE_FEATURE_SEL), 1);

        negotiate(F_DEVICE | F_VERSION_1);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
        EXPECT_EQ(echo.driver_features, F_DEVICE | F_VERSION_1);
        EXPECT_TRUE(virtio_pci.has_feature(F_VERSION_1));
        EXPECT_FALSE(virtio_pci.has_feature(F_EVENT_IDX));
        EXPECT_EQ(read<u32>(REG_DRIVER_FEATURE), (u32)(F_VERSION_1 >> 32));

        write<u32>(REG_DRIVER_FEATURE, 0);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_NEEDS_RESET);

        negotiate(F_VERSION_1 | bit(1));
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_ACK | STATUS_DRIVER);
        negotiate(F_VERSION_1 | F_PACKED);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_ACK | STATUS_DRIVER);

        echo.accept_features = false;
        negotiate(F_VERSION_1);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_ACK | STATUS_DRIVER);
        echo.accept_features = true;
        negotiate(F_VERSION_1);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);
    }

    void test_status() {
        size_t resets = echo.resets;
        start(F_VERSION_1);
        EXPECT_GT(echo.resets, resets);
        EXPECT_TRUE(virtio_pci.device_ready());

        write<u16>(REG_QUEUE_SEL, 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 1);

        resets = echo.resets;
        write<u8>(REG_DEVICE_STATUS, 0);
        EXPECT_EQ(echo.resets, resets + 1);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), 0);
        EXPECT_FALSE(virtio_pci.device_ready());
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 0);

        write<u8>(REG_DEVICE_STATUS,
                  STATUS_ACK | STATUS_DRIVER | STATUS_DRIVER_OK);
        write<u8>(REG_DEVICE_STATUS, STATUS_ACK);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_ACK);

        vq_message msg;
        EXPECT_FALSE(echo.virtio_in->get(0, msg));
        EXPECT_FALSE(echo.virtio_in->put(0, msg));
        EXPECT_FALSE(echo.virtio_in->notify());
    }

    void test_queue_setup() {
        negotiate(F_VERSION_1);
        write<u16>(REG_QUEUE_SEL, 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_SEL), 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_SIZE), virtio_echo::VQ0_SIZE);
        EXPECT_EQ(read<u16>(REG_QUEUE_MSIX_VECTOR), VIRTIO_NO_VECTOR);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_NOTIFY_OFF), 0);
        write<u16>(REG_QUEUE_NOTIFY_OFF, 1);
        write<u16>(REG_QUEUE_NOTIFY_OFF, 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_NOTIFY_OFF), 0);

        write<u16>(REG_QUEUE_SIZE, 2 * virtio_echo::VQ0_SIZE);
        EXPECT_EQ(read<u16>(REG_QUEUE_SIZE), virtio_echo::VQ0_SIZE);
        write<u16>(REG_QUEUE_SIZE, 4);
        EXPECT_EQ(read<u16>(REG_QUEUE_SIZE), 4);
        write<u16>(REG_QUEUE_MSIX_VECTOR, 3);
        EXPECT_EQ(read<u16>(REG_QUEUE_MSIX_VECTOR), 3);
        write<u64>(REG_QUEUE_DESC, 0x1111222233334444);
        write<u64>(REG_QUEUE_DRIVER, 0x5555666677778888);
        write<u64>(REG_QUEUE_DEVICE, 0x9999aaaabbbbcccc);
        EXPECT_EQ(read<u64>(REG_QUEUE_DESC), 0x1111222233334444);
        EXPECT_EQ(read<u64>(REG_QUEUE_DRIVER), 0x5555666677778888);
        EXPECT_EQ(read<u64>(REG_QUEUE_DEVICE), 0x9999aaaabbbbcccc);

        write<u16>(REG_QUEUE_SEL, 1);
        EXPECT_EQ(read<u16>(REG_QUEUE_SIZE), virtio_echo::VQ1_SIZE);
        EXPECT_EQ(read<u64>(REG_QUEUE_DESC), 0);

        write<u16>(REG_QUEUE_SEL, 5);
        EXPECT_EQ(read<u16>(REG_QUEUE_SIZE), 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_MSIX_VECTOR), 0);
        EXPECT_EQ(read<u64>(REG_QUEUE_DESC), 0);
        EXPECT_EQ(read<u64>(REG_QUEUE_DRIVER), 0);
        EXPECT_EQ(read<u64>(REG_QUEUE_DEVICE), 0);
        write<u16>(REG_QUEUE_SIZE, 4);
        write<u16>(REG_QUEUE_MSIX_VECTOR, 1);
        write<u64>(REG_QUEUE_DESC, RING0);
        write<u64>(REG_QUEUE_DRIVER, RING0);
        write<u64>(REG_QUEUE_DEVICE, RING0);
        write<u16>(REG_QUEUE_ENABLE, 1);
        write<u16>(REG_QUEUE_ENABLE, 0);
        write<u16>(REG_QUEUE_RESET, 1);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 0);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS),
                  STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);

        setup_queue(0, RING0, virtio_echo::VQ0_SIZE);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 1);
        write<u16>(REG_QUEUE_ENABLE, 1);
        write<u16>(REG_QUEUE_ENABLE, 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 0);
        write<u16>(REG_QUEUE_ENABLE, 0);

        setup_queue(1, MEM_SIZE, virtio_echo::VQ1_SIZE);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_NEEDS_RESET);
    }

    void test_transfer() {
        start(F_VERSION_1 | F_INDIRECT_DESC);
        virtio_split_driver ring0(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        virtio_split_driver ring1(mem.data(), RING1, virtio_echo::VQ1_SIZE);
        u32 id = 0, len = 0;

        for (u32 i = 0; i < 2 * virtio_echo::VQ0_SIZE; i++) {
            vector<u8> data = pattern(16 + i, i);
            virtio_buffer in = input(data);
            virtio_buffer res = output(64);
            u16 head = ring0.add({ in, res });
            kick(0);

            ASSERT_TRUE(ring0.pop_used(id, len)) << "request " << i;
            EXPECT_EQ(id, head) << "request " << i;
            EXPECT_EQ(len, data.size()) << "request " << i;
            EXPECT_EQ(content(res, len), inverted(data)) << "request " << i;
        }

        vector<u8> data = pattern(32, 0x80);
        virtio_buffer in0 = input(vector<u8>(data.begin(), data.begin() + 16));
        virtio_buffer in1 = input(vector<u8>(data.begin() + 16, data.end()));
        virtio_buffer res = output(32);
        u16 head = ring1.add_indirect(alloc(64), { in0, in1, res });
        kick(1);
        ASSERT_TRUE(ring1.pop_used(id, len));
        EXPECT_EQ(id, head);
        EXPECT_EQ(len, 32);
        EXPECT_EQ(content(res, 32), inverted(data));
        EXPECT_FALSE(ring0.pop_used(id, len));
        read<u32>(REG_IRQ_STATUS);
    }

    void test_interrupts() {
        start(F_VERSION_1);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        virtio_buffer in = input(pattern(4, 1));
        u32 id = 0, len = 0;

        EXPECT_EQ(read<u32>(REG_IRQ_STATUS), 0);
        ring.add({ in, output(4) });
        kick(0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_TRUE(int_a);
        EXPECT_FALSE(int_b);
        EXPECT_EQ(read<u32>(REG_IRQ_STATUS), VIRTIO_IRQSTATUS_VQUEUE);
        EXPECT_FALSE(int_a);
        EXPECT_EQ(read<u32>(REG_IRQ_STATUS), 0);

        ring.set_no_interrupt(true);
        ring.add({ in, output(4) });
        kick(0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_FALSE(int_a);
        EXPECT_EQ(read<u32>(REG_IRQ_STATUS), 0);
        ring.set_no_interrupt(false);

        EXPECT_TRUE(echo.virtio_in->notify());
        EXPECT_TRUE(int_a);
        ring.add({ in, output(4) });
        kick(0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_EQ(read<u32>(REG_IRQ_STATUS),
                  VIRTIO_IRQSTATUS_VQUEUE | VIRTIO_IRQSTATUS_CONFIG);
        EXPECT_FALSE(int_a);
    }

    void test_msix() {
        u8 msix = find_cap(PCI_CAPABILITY_MSIX);
        ASSERT_NE(msix, 0);
        u16 control = pci_read_cfg<u16>(msix + 2);
        EXPECT_EQ(control & PCI_MSIX_TABLE_SIZE_MASK, 15);

        start(F_VERSION_1);
        pci_write_cfg<u16>(msix + 2, control | PCI_MSIX_ENABLE);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        u32 id = 0, len = 0;

        ring.add({ input(pattern(4, 1)), output(4) });
        kick(0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_FALSE(int_a);
        EXPECT_EQ(read<u32>(REG_IRQ_STATUS), 0);

        pci_write_cfg<u16>(msix + 2, control);
        ring.add({ input(pattern(4, 2)), output(4) });
        kick(0);
        EXPECT_TRUE(ring.pop_used(id, len));
        EXPECT_TRUE(int_a);
        EXPECT_EQ(read<u32>(REG_IRQ_STATUS), VIRTIO_IRQSTATUS_VQUEUE);
        EXPECT_FALSE(int_a);
    }

    void test_notify() {
        memset(mem.data(), 0, MEM_SIZE);
        negotiate(F_VERSION_1);
        setup_queue(0, RING0, virtio_echo::VQ0_SIZE);
        echo.kicks.clear();
        kick(0);
        EXPECT_TRUE(echo.kicks.empty());

        write<u8>(REG_DEVICE_STATUS, STATUS_ACK | STATUS_DRIVER |
                                         STATUS_FEATURES_OK |
                                         STATUS_DRIVER_OK);
        kick(1);
        kick(5);
        kick(VIRTQUEUE_MAX);
        EXPECT_TRUE(echo.kicks.empty());

        kick(0);
        kick(0xabcd0000);
        EXPECT_EQ(echo.kicks, vector<u32>({ 0, 0 }));
        EXPECT_EQ(read<u32>(REG_QUEUE_NOTIFY), 0xabcd0000);

        echo.accept_notify = false;
        kick(0);
        EXPECT_EQ(read<u8>(REG_DEVICE_STATUS), STATUS_NEEDS_RESET);
        echo.accept_notify = true;

        vq_message msg;
        start(F_VERSION_1);
        EXPECT_FALSE(echo.virtio_in->get(7, msg));
        EXPECT_FALSE(echo.virtio_in->put(7, msg));
    }

    void test_config() {
        negotiate(F_VERSION_1);
        for (u32 i = 0; i < 16; i += 4) {
            EXPECT_EQ(read<u32>(REG_CONFIG + i),
                      0xc0c0c0c0 + 0x03020100 + i * 0x01010101);
        }

        EXPECT_EQ(read<u8>(REG_CONFIG + 5), 0xc5);
        EXPECT_EQ(read<u8>(REG_CONFIG_GEN), 0);
        write<u32>(REG_CONFIG + 4, 0x11223344);
        EXPECT_EQ(read<u32>(REG_CONFIG + 4), 0x11223344);
        EXPECT_EQ(read<u8>(REG_CONFIG_GEN), 1);
        EXPECT_EQ(echo.config[4], 0x44);

        u32 data = 0;
        EXPECT_AE(mmio.readw(REG_CONFIG + 16, data));
        EXPECT_AE(mmio.writew(REG_CONFIG + 16, data));
        EXPECT_EQ(read<u8>(REG_CONFIG_GEN), 1);
        write<u32>(REG_CONFIG + 4, 0xc7c6c5c4);
    }

    void test_queue_reset() {
        start(F_VERSION_1 | F_RING_RESET);
        virtio_split_driver ring(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        u32 id = 0, len = 0;

        ring.add({ input(pattern(4, 1)), output(4) });
        kick(0);
        EXPECT_TRUE(ring.pop_used(id, len));
        read<u32>(REG_IRQ_STATUS);

        write<u16>(REG_QUEUE_SEL, 0);
        write<u16>(REG_QUEUE_RESET, 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 1);
        write<u16>(REG_QUEUE_RESET, 1);
        EXPECT_EQ(read<u16>(REG_QUEUE_RESET), 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_ENABLE), 0);
        EXPECT_EQ(read<u64>(REG_QUEUE_DESC), 0);
        EXPECT_EQ(read<u16>(REG_QUEUE_MSIX_VECTOR), VIRTIO_NO_VECTOR);
        write<u16>(REG_QUEUE_RESET, 1);

        echo.kicks.clear();
        kick(0);
        EXPECT_TRUE(echo.kicks.empty());

        virtio_split_driver fresh(mem.data(), RING0, virtio_echo::VQ0_SIZE);
        setup_queue(0, RING0, virtio_echo::VQ0_SIZE);
        vector<u8> data = pattern(4, 9);
        virtio_buffer res = output(4);
        fresh.add({ input(data), res });
        kick(0);
        ASSERT_TRUE(fresh.pop_used(id, len));
        EXPECT_EQ(content(res, 4), inverted(data));
        read<u32>(REG_IRQ_STATUS);
    }

    void test_shared_memory() {
        map_bars();
        EXPECT_EQ(virtio_pci.shm_size, SHM_SIZE);

        write<u32>(BAR4_ADDR + 0x20, 0x12345678);
        EXPECT_EQ(echo.shm[0x20], 0x78);
        EXPECT_EQ(echo.shm[0x23], 0x12);
        EXPECT_EQ(read<u32>(BAR4_ADDR + 0x20), 0x12345678);

        u8 obj[0x100];
        memset(obj, 0x5a, sizeof(obj));
        EXPECT_TRUE(echo.virtio_in->shm_map(1, 5, 0x1000, obj, sizeof(obj)));
        EXPECT_FALSE(echo.virtio_in->shm_map(1, 5, 0x2000, obj, 16));
        EXPECT_FALSE(echo.virtio_in->shm_map(1, 6, 0x10f0, obj, 32));
        EXPECT_FALSE(echo.virtio_in->shm_map(9, 7, 0, obj, 16));

        EXPECT_EQ(read<u32>(BAR4_ADDR + 0x1010), 0x5a5a5a5a);
        write<u32>(BAR4_ADDR + 0x1010, 0xaabbccdd);
        EXPECT_EQ(obj[0x10], 0xdd);
        EXPECT_EQ(obj[0x13], 0xaa);
        EXPECT_EQ(echo.shm[0x1010], 0);

        EXPECT_TRUE(echo.virtio_in->shm_unmap(1, 5));
        EXPECT_FALSE(echo.virtio_in->shm_unmap(1, 5));
        EXPECT_FALSE(echo.virtio_in->shm_unmap(9, 5));
        write<u32>(BAR4_ADDR + 0x1010, 0x01020304);
        EXPECT_EQ(obj[0x10], 0xdd);
        EXPECT_EQ(echo.shm[0x1010], 0x04);

        write<u32>(BAR4_ADDR + 0x20, 0);
        write<u32>(BAR4_ADDR + 0x1010, 0);
    }
};

TEST(virtio, pci) {
    broker broker("test");
    broker.define("virtio_pci.virtio_pci.shm_size", to_string((u64)SHM_SIZE));

    virtio_pci_test test("virtio_pci");
    sc_core::sc_start();
}
