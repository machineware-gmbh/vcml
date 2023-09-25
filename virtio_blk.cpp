/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class virtio_blk_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::blk virtio_blk;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_blk_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_blk("virtio_blk"),
        out("out"),
        irq("irq") {
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

    virtual void run_test() override {
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
};

TEST(virtio, blk) {
    virtio_blk_stim stim;
    sc_core::sc_start();
}
