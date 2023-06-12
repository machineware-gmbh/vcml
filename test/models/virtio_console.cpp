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
    virtio::console virtio_console;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_rng_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_console("virtio_input"),
        out("out"),
        irq("irq") {
        virtio.virtio_out.bind(virtio_console.virtio_in);
        virtio_console.serial_tx.stub();
        virtio_console.serial_rx.stub();

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
            CONSOLE_BASE = 0x1000,
            CONSOLE_MAGIC = CONSOLE_BASE + 0x00,
            CONSOLE_VERSION = CONSOLE_BASE + 0x04,
            CONSOLE_DEVID = CONSOLE_BASE + 0x08,
            CONSOLE_VQ_SEL = CONSOLE_BASE + 0x30,
            CONSOLE_VQ_MAX = CONSOLE_BASE + 0x34,
            CONSOLE_STATUS = CONSOLE_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(CONSOLE_MAGIC, data));
        ASSERT_EQ(data, fourcc("virt"));

        ASSERT_OK(out.readw(CONSOLE_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(CONSOLE_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_CONSOLE);

        ASSERT_OK(out.readw(CONSOLE_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(CONSOLE_STATUS, data));

        ASSERT_OK(out.readw(CONSOLE_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        data = 0;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 1;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 2;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 3;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 4;
        ASSERT_OK(out.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(out.readw(CONSOLE_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }
};

TEST(virtio, rng) {
    virtio_rng_stim stim;
    stim.clk.stub(100 * MHz);
    stim.rst.stub();

    tlm::tlm_global_quantum::instance().set(sc_time(10, SC_MS));
    sc_core::sc_start();
}
