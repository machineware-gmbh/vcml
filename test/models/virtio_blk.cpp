/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "virtio_test.h"

class virtio_blk_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::blk virtio_blk;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_stim stim;
    virtio::blk blk;

    enum : u32 {
        T_IN = 0,
        T_OUT = 1,
        T_FLUSH = 4,
        T_GET_ID = 8,
        T_DISCARD = 11,
        T_WRITE_ZEROES = 13,
        S_OK = 0,
        S_IOERR = 1,
        S_UNSUPP = 2,
        S_NONE = 0xff,
        SECTOR = 512,
    };

    enum : u64 {
        F_SIZE_MAX = bit(1),
        F_SEG_MAX = bit(2),
        F_RO = bit(5),
        F_BLK_SIZE = bit(6),
        F_FLUSH = bit(9),
        F_DISCARD = bit(13),
        F_WRITE_ZEROES = bit(14),
    };

    struct request {
        u32 type;
        u32 reserved;
        u64 sector;
    };

    struct range_request {
        u64 sector;
        u32 num_sectors;
        u32 flags;
    };

    virtio_blk_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_blk("virtio_blk"),
        out("out"),
        irq("irq"),
        stim("stim"),
        blk("blk") {
        stim.virtio_out.bind(blk.virtio_in);

        add_test("mmio", &virtio_blk_stim::test_mmio);
        add_test("identify", &virtio_blk_stim::test_identify);
        add_test("config", &virtio_blk_stim::test_config);
        add_test("read_write", &virtio_blk_stim::test_read_write);
        add_test("discard", &virtio_blk_stim::test_discard);
        add_test("misc", &virtio_blk_stim::test_misc);
        add_test("errors", &virtio_blk_stim::test_errors);

        virtio.virtio_out.bind(virtio_blk.virtio_in);

        bus.bind(mem.in, 0, 0xfff);
        bus.bind(virtio.in, 0x1000, 0x1fff);

        bus.bind(out);
        bus.bind(virtio.out);

        virtio.irq.bind(irq);

        clk.bind(bus.clk);
        clk.bind(mem.clk);
        clk.bind(virtio.clk);

        rst.bind(bus.rst);
        rst.bind(mem.rst);
        rst.bind(virtio.rst);
    }

    void test_mmio() {
        enum addresses : u64 {
            BLK_BASE = 0x1000,
            BLK_MAGIC = BLK_BASE + 0x00,
            BLK_VERSION = BLK_BASE + 0x04,
            BLK_DEVID = BLK_BASE + 0x08,
            BLK_DEVF = BLK_BASE + 0x10,
            BLK_DEVF_SEL = BLK_BASE + 0x14,
            BLK_DRVF = BLK_BASE + 0x20,
            BLK_DRVF_SEL = BLK_BASE + 0x24,
            BLK_VQ_SEL = BLK_BASE + 0x30,
            BLK_VQ_MAX = BLK_BASE + 0x34,
            BLK_STATUS = BLK_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(BLK_MAGIC, data));
        ASSERT_EQ(data, 0x74726976);

        ASSERT_OK(out.readw(BLK_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(BLK_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_BLOCK);

        ASSERT_OK(out.readw(BLK_STATUS, data));
        ASSERT_EQ(data, 0);

        ASSERT_OK(out.writew(BLK_DEVF_SEL, 0u));
        ASSERT_OK(out.readw(BLK_DEVF, data));
        ASSERT_TRUE(data & (bit(5) | bit(6))); // readonly + block size
        ASSERT_OK(out.writew(BLK_DRVF_SEL, 0u));
        ASSERT_OK(out.writew(BLK_DRVF, data));

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(BLK_STATUS, data));

        ASSERT_OK(out.readw(BLK_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        data = 0;
        ASSERT_OK(out.writew(BLK_VQ_SEL, data));
        ASSERT_OK(out.readw(BLK_VQ_MAX, data));
        EXPECT_EQ(data, 256);

        data = 1;
        ASSERT_OK(out.writew(BLK_VQ_SEL, data));
        ASSERT_OK(out.readw(BLK_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }

    static vector<u8> pattern(size_t len, u8 seed) {
        vector<u8> data(len);
        for (size_t i = 0; i < len; i++)
            data[i] = (u8)(seed + i * 3);
        return data;
    }

    vector<u8> execute(u32 type, u64 sector, const vector<u8>& data,
                       size_t outlen) {
        request req = { type, 0, sector };
        size_t done = stim.used[0].size();
        vq_message msg = stim.submit(0, { stim.bytes(req), data },
                                     { outlen, 1 });
        EXPECT_TRUE(stim.kick(0));
        EXPECT_EQ(stim.used[0].size(), done + 1);
        return stim.response(msg);
    }

    u8 write(u64 sector, const vector<u8>& data) {
        return execute(T_OUT, sector, data, 0).back();
    }

    vector<u8> read(u64 sector, size_t len, u8 status = S_OK) {
        vector<u8> data = execute(T_IN, sector, {}, len);
        EXPECT_EQ(data.back(), status);
        data.pop_back();
        return data;
    }

    void test_identify() {
        virtio_device_desc desc = stim.identify();
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_BLOCK);
        EXPECT_EQ(desc.vendor_id, VIRTIO_VENDOR_VCML);
        EXPECT_EQ(desc.pci_class, PCI_CLASS_STORAGE_SCSI);
        ASSERT_EQ(desc.virtqueues.size(), 1);
        EXPECT_EQ(desc.virtqueues.at(0).limit, 256);

        u64 features = stim.features();
        EXPECT_EQ(features, F_SIZE_MAX | F_SEG_MAX | F_BLK_SIZE | F_FLUSH |
                                F_DISCARD | F_WRITE_ZEROES);
        EXPECT_TRUE(stim.virtio_out->write_features(features));
        EXPECT_FALSE(stim.virtio_out->write_features(features | F_RO));
    }

    void test_config() {
        u64 capacity = 0;
        u32 size_max = 0;
        u32 seg_max = 0;
        u32 blk_size = 0;
        u32 max_discard = 0;
        u32 max_zeroes = 0;
        EXPECT_TRUE(stim.read_config(0, capacity));
        EXPECT_TRUE(stim.read_config(8, size_max));
        EXPECT_TRUE(stim.read_config(12, seg_max));
        EXPECT_TRUE(stim.read_config(20, blk_size));
        EXPECT_TRUE(stim.read_config(36, max_discard));
        EXPECT_TRUE(stim.read_config(48, max_zeroes));
        EXPECT_EQ(capacity, blk.disk.capacity() / SECTOR);
        EXPECT_EQ(size_max, blk.max_size);
        EXPECT_EQ(seg_max, 254);
        EXPECT_EQ(blk_size, SECTOR);
        EXPECT_EQ(max_discard, blk.max_discard_sectors);
        EXPECT_EQ(max_zeroes, blk.max_write_zeroes_sectors);

        u8 data = 0;
        EXPECT_TRUE(stim.read_config(59, data));
        EXPECT_FALSE(stim.read_config(64, data));
        EXPECT_FALSE(stim.write_config(0, capacity));
    }

    void test_read_write() {
        stim.clear();
        vector<u8> zero(2 * SECTOR, 0);
        vector<u8> data = pattern(2 * SECTOR, 0x11);

        EXPECT_EQ(write(10, zero), S_OK);
        EXPECT_EQ(write(12, zero), S_OK);
        EXPECT_EQ(write(11, data), S_OK);
        EXPECT_EQ(read(11, 2 * SECTOR), data);
        EXPECT_EQ(read(10, SECTOR), vector<u8>(SECTOR, 0));
        EXPECT_EQ(read(13, SECTOR), vector<u8>(SECTOR, 0));
        EXPECT_EQ(read(12, SECTOR),
                  vector<u8>(data.begin() + SECTOR, data.end()));

        request req = { T_IN, 0, 11 };
        vq_message msg = stim.submit(0, { stim.bytes(req) },
                                     { 100, SECTOR - 100, SECTOR, 1 });
        EXPECT_TRUE(stim.kick(0));
        vector<u8> resp = stim.response(msg);
        EXPECT_EQ(resp.back(), S_OK);
        resp.pop_back();
        EXPECT_EQ(resp, data);

        u64 last = blk.disk.capacity() / SECTOR - 1;
        EXPECT_EQ(write(last, vector<u8>(data.begin(), data.begin() + SECTOR)),
                  S_OK);
        EXPECT_EQ(read(last, SECTOR),
                  vector<u8>(data.begin(), data.begin() + SECTOR));

        EXPECT_EQ(write(last, vector<u8>(SECTOR, 0)), S_OK);
        EXPECT_EQ(write(11, zero), S_OK);
    }

    void test_discard() {
        stim.clear();
        vector<u8> data = pattern(4 * SECTOR, 0x22);
        EXPECT_EQ(write(20, data), S_OK);

        range_request zeroes = { 21, 2, 0 };
        EXPECT_EQ(execute(T_WRITE_ZEROES, 0, stim.bytes(zeroes), 0).back(),
                  S_OK);
        vector<u8> expected = data;
        std::fill(expected.begin() + SECTOR, expected.begin() + 3 * SECTOR, 0);
        EXPECT_EQ(read(20, 4 * SECTOR), expected);

        range_request unmap = { 20, 1, 1 };
        EXPECT_EQ(execute(T_WRITE_ZEROES, 0, stim.bytes(unmap), 0).back(),
                  S_OK);
        EXPECT_EQ(read(20, SECTOR), vector<u8>(SECTOR, 0));

        range_request discard = { 23, 1, 0 };
        EXPECT_EQ(execute(T_DISCARD, 0, stim.bytes(discard), 0).back(), S_OK);

        u64 nsectors = blk.disk.capacity() / SECTOR;
        range_request outside = { nsectors + 1, 1, 0 };
        EXPECT_EQ(execute(T_DISCARD, 0, stim.bytes(outside), 0).back(),
                  S_IOERR);
        EXPECT_EQ(execute(T_WRITE_ZEROES, 0, stim.bytes(outside), 0).back(),
                  S_IOERR);

        EXPECT_EQ(execute(T_DISCARD, 0, { 1, 2, 3 }, 0).back(), S_NONE);
        EXPECT_EQ(execute(T_WRITE_ZEROES, 0, { 1, 2, 3 }, 0).back(), S_NONE);

        EXPECT_EQ(write(20, vector<u8>(4 * SECTOR, 0)), S_OK);
    }

    void test_misc() {
        stim.clear();
        EXPECT_EQ(execute(T_FLUSH, 0, {}, 0).back(), S_OK);

        vector<u8> id = execute(T_GET_ID, 0, {}, 20);
        EXPECT_EQ(id.back(), S_OK);
        id.pop_back();
        string serial = blk.disk.serial;
        serial.resize(20, 0);
        serial[19] = 0;
        EXPECT_EQ(string(id.begin(), id.end()), serial);

        EXPECT_EQ(execute(99, 0, {}, 0).back(), S_UNSUPP);
    }

    void test_errors() {
        stim.clear();
        u64 nsectors = blk.disk.capacity() / SECTOR;

        EXPECT_EQ(read(0, 100, S_IOERR).size(), 100);
        EXPECT_EQ(read(nsectors + 1, SECTOR, S_IOERR).size(), SECTOR);
        EXPECT_EQ(write(0, vector<u8>(100)), S_IOERR);
        EXPECT_EQ(write(nsectors + 1, vector<u8>(SECTOR)), S_IOERR);

        size_t done = stim.used[0].size();
        vq_message msg = stim.submit(0, { { 1, 2, 3 } }, { 1 });
        EXPECT_TRUE(stim.kick(0));
        EXPECT_EQ(stim.used[0].size(), done + 1);
        EXPECT_EQ(stim.response(msg), vector<u8>({ S_NONE }));

        request req = { T_FLUSH, 0, 0 };
        msg = stim.submit(0, { stim.bytes(req) }, {});
        EXPECT_TRUE(stim.kick(0));
        EXPECT_EQ(stim.used[0].size(), done + 2);

        EXPECT_TRUE(stim.kick(0));
        EXPECT_EQ(stim.used[0].size(), done + 2);

        stim.submit(0, { stim.bytes(req) }, { 1 });
        stim.accept = false;
        EXPECT_FALSE(stim.kick(0));
        stim.accept = true;
    }
};

TEST(virtio, blk) {
    virtio_blk_stim stim;
    sc_core::sc_start();
}
