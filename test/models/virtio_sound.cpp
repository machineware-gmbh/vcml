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

class virtio_sound_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::sound virtio_sound;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_sound_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_sound("virtio_sound"),
        out("out"),
        irq("irq") {
        virtio.virtio_out.bind(virtio_sound.virtio_in);

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
            SOUND_BASE = 0x1000,
            SOUND_MAGIC = SOUND_BASE + 0x00,
            SOUND_VERSION = SOUND_BASE + 0x04,
            SOUND_DEVID = SOUND_BASE + 0x08,
            SOUND_VQ_SEL = SOUND_BASE + 0x30,
            SOUND_VQ_MAX = SOUND_BASE + 0x34,
            SOUND_STATUS = SOUND_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(SOUND_MAGIC, data));
        ASSERT_EQ(data, 0x74726976);

        ASSERT_OK(out.readw(SOUND_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(SOUND_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_SOUND);

        ASSERT_OK(out.readw(SOUND_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(SOUND_STATUS, data));

        ASSERT_OK(out.readw(SOUND_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        // control virtqueue
        data = 0;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // event virtqueue
        data = 1;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // tx virtqueue
        data = 2;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // rx virtqueue
        data = 3;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // no more queues
        data = 4;
        ASSERT_OK(out.writew(SOUND_VQ_SEL, data));
        ASSERT_OK(out.readw(SOUND_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }
};

TEST(virtio, sound) {
    virtio_sound_stim stim;
    sc_core::sc_start();
}
