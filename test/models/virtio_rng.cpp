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

class virtio_rng_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::rng virtio_rng;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_rng_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_rng("virtio_rng"),
        out("out"),
        irq("irq") {
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

    virtual void run_test() override {
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
};

TEST(virtio, rng) {
    virtio_rng_stim stim;
    sc_core::sc_start();
}
