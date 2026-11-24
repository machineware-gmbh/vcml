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

class virtio_rng_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::rng virtio_rng;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_stim stim;
    virtio::rng rng;

    virtio_rng_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_rng("virtio_rng"),
        out("out"),
        irq("irq"),
        stim("stim"),
        rng("rng") {
        stim.virtio_out.bind(rng.virtio_in);
        rng.pseudo = true;
        rng.seed = 42;

        add_test("mmio", &virtio_rng_stim::test_mmio);
        add_test("identify", &virtio_rng_stim::test_identify);
        add_test("pseudo", &virtio_rng_stim::test_pseudo);
        add_test("random", &virtio_rng_stim::test_random);
        add_test("errors", &virtio_rng_stim::test_errors);

        virtio.virtio_out.bind(virtio_rng.virtio_in);

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
            RNG_BASE = 0x1000,
            RNG_MAGIC = RNG_BASE + 0x00,
            RNG_VERSION = RNG_BASE + 0x04,
            RNG_DEVID = RNG_BASE + 0x08,
            RNG_VQ_SEL = RNG_BASE + 0x30,
            RNG_VQ_MAX = RNG_BASE + 0x34,
            RNG_STATUS = RNG_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(RNG_MAGIC, data));
        ASSERT_EQ(data, 0x74726976);

        ASSERT_OK(out.readw(RNG_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(RNG_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_RNG);

        ASSERT_OK(out.readw(RNG_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(RNG_STATUS, data));

        ASSERT_OK(out.readw(RNG_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        data = 0;
        ASSERT_OK(out.writew(RNG_VQ_SEL, data));
        ASSERT_OK(out.readw(RNG_VQ_MAX, data));
        EXPECT_EQ(data, 8);

        data = 1;
        ASSERT_OK(out.writew(RNG_VQ_SEL, data));
        ASSERT_OK(out.readw(RNG_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }

    void test_identify() {
        virtio_device_desc desc = stim.identify();
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_RNG);
        EXPECT_EQ(desc.vendor_id, VIRTIO_VENDOR_VCML);
        EXPECT_EQ(desc.pci_class, PCI_CLASS_OTHERS);
        ASSERT_EQ(desc.virtqueues.size(), 1);
        EXPECT_EQ(desc.virtqueues.at(0).limit, 8);

        EXPECT_EQ(stim.features(), 0);
        EXPECT_TRUE(stim.virtio_out->write_features(0));

        u32 data = 0;
        EXPECT_FALSE(stim.read_config(0, data));
        EXPECT_FALSE(stim.write_config(0, data));
    }

    void test_pseudo() {
        stim.clear();
        vector<u8> expected(48);
        srand(rng.seed);
        for (u8& val : expected)
            val = rand();

        stim.identify();
        vq_message msg0 = stim.submit(0, {}, { 16 });
        vq_message msg1 = stim.submit(0, {}, { 8, 24 });
        EXPECT_TRUE(stim.kick(0));
        ASSERT_EQ(stim.used[0].size(), 2);
        EXPECT_TRUE(stim.avail[0].empty());
        EXPECT_EQ(stim.used[0][0].index, msg0.index);
        EXPECT_EQ(stim.used[0][1].index, msg1.index);

        vector<u8> random = stim.response(msg0);
        vector<u8> more = stim.response(msg1);
        random.insert(random.end(), more.begin(), more.end());
        EXPECT_EQ(random, expected);

        stim.identify();
        vq_message msg2 = stim.submit(0, {}, { 48 });
        EXPECT_TRUE(stim.kick(0));
        EXPECT_EQ(stim.response(msg2), expected);
    }

    void test_random() {
        stim.clear();
        rng.pseudo = false;
        vq_message msg0 = stim.submit(0, {}, { 64 });
        vq_message msg1 = stim.submit(0, {}, { 64 });
        EXPECT_TRUE(stim.kick(0));
        ASSERT_EQ(stim.used[0].size(), 2);
        EXPECT_NE(stim.response(msg0), vector<u8>(64, 0xff));
        EXPECT_NE(stim.response(msg1), vector<u8>(64, 0xff));
        EXPECT_NE(stim.response(msg0), stim.response(msg1));
        rng.pseudo = true;
    }

    void test_errors() {
        stim.clear();
        EXPECT_TRUE(stim.kick(0));
        EXPECT_TRUE(stim.used[0].empty());

        stim.submit(0, {}, { 16 });
        stim.submit(0, {}, { 16 });
        stim.accept = false;
        EXPECT_FALSE(stim.kick(0));
        EXPECT_TRUE(stim.used[0].empty());
        EXPECT_EQ(stim.avail[0].size(), 1);

        stim.accept = true;
        EXPECT_TRUE(stim.kick(0));
        EXPECT_EQ(stim.used[0].size(), 1);
        EXPECT_EQ(stim.irqs, 0);
    }
};

TEST(virtio, rng) {
    virtio_rng_stim stim;
    EXPECT_STREQ(stim.rng.kind(), "vcml::virtio::rng");
    sc_core::sc_start();
}
