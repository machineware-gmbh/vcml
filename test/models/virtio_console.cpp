/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class virtio_rng_stim: public test_base
{
public:
    generic::bus    bus;
    generic::memory mem;

    virtio::mmio    virtio;
    virtio::console virtio_console;

    tlm_initiator_socket OUT;
    irq_target_socket    IRQ;

    virtio_rng_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_console("virtio_input"),
        OUT("OUT"),
        IRQ("IRQ") {

        virtio.VIRTIO_OUT.bind(virtio_console.VIRTIO_IN);

        bus.bind(mem.IN, 0, 0xfff);
        bus.bind(virtio.IN, 0x1000, 0x1fff);

        bus.bind(OUT);
        bus.bind(virtio.OUT);

        virtio.IRQ.bind(IRQ);

        bus.CLOCK.bind(CLOCK);
        mem.CLOCK.bind(CLOCK);
        virtio.CLOCK.bind(CLOCK);

        bus.RESET.bind(RESET);
        mem.RESET.bind(RESET);
        virtio.RESET.bind(RESET);
    }

    virtual void run_test() override {
        const u64 CONSOLE_BASE = 0x1000;

        const u64 CONSOLE_MAGIC   = CONSOLE_BASE + 0x00;
        const u64 CONSOLE_VERSION = CONSOLE_BASE + 0x04;
        const u64 CONSOLE_DEVID   = CONSOLE_BASE + 0x08;
        const u64 CONSOLE_VQ_SEL  = CONSOLE_BASE + 0x30;
        const u64 CONSOLE_VQ_MAX  = CONSOLE_BASE + 0x34;
        const u64 CONSOLE_STATUS  = CONSOLE_BASE + 0x70;

        u32 data;
        ASSERT_OK(OUT.readw(CONSOLE_MAGIC, data));
        ASSERT_EQ(data, fourcc("virt"));

        ASSERT_OK(OUT.readw(CONSOLE_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(OUT.readw(CONSOLE_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_CONSOLE);

        ASSERT_OK(OUT.readw(CONSOLE_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE |
               VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(OUT.writew(CONSOLE_STATUS, data));

        ASSERT_OK(OUT.readw(CONSOLE_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        data = 0;
        ASSERT_OK(OUT.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(OUT.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 1;
        ASSERT_OK(OUT.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(OUT.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 2;
        ASSERT_OK(OUT.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(OUT.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 3;
        ASSERT_OK(OUT.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(OUT.readw(CONSOLE_VQ_MAX, data));
        EXPECT_GT(data, 0);

        data = 4;
        ASSERT_OK(OUT.writew(CONSOLE_VQ_SEL, data));
        ASSERT_OK(OUT.readw(CONSOLE_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }

};

TEST(virtio, rng) {
    virtio_rng_stim stim;
    stim.CLOCK.stub(100 * MHz);
    stim.RESET.stub();

    tlm::tlm_global_quantum::instance().set(sc_time(10, SC_MS));
    sc_core::sc_start();
}
