/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

class bus_harness : public test_base
{
public:
    generic::memory mem1;
    generic::memory mem2;
    generic::bus bus;

    tlm_initiator_socket out;

    bus_harness(const sc_module_name& nm):
        test_base(nm),
        mem1("mem1", 0x2000),
        mem2("mem2", 0x2000),
        bus("bus"),
        out("out") {
        mem1.clk.stub(100 * MHz);
        mem2.clk.stub(100 * MHz);
        bus.clk.stub(100 * MHz);
        clk.stub(100 * MHz);

        mem1.rst.stub();
        mem2.rst.stub();
        bus.rst.stub();
        rst.stub();

        bus.bind(out);
        bus.bind(mem1.in, 0x0000, 0x1fff, 0);
        bus.bind(mem2.in, 0x2000, 0x3fff, 0);
    }

    virtual void run_test() override {
        ASSERT_OK(out.writew<u32>(0x0000, 0x11111111ul))
            << "cannot write 0x0000 (mem1 + 0x0)";
        ASSERT_OK(out.writew<u32>(0x0004, 0xfffffffful))
            << "cannot write 0x0004 (mem1 + 0x4)";
        ASSERT_OK(out.writew<u32>(0x2000, 0x55555555ul))
            << "cannot write 0x2000 (mem2 + 0x0)";
        ASSERT_OK(out.writew<u32>(0x2004, 0xbbbbbbbbul))
            << "cannot write 0x2004 (mem2 + 0x4)";
        ASSERT_AE(out.writew<u16>(0x4000, 0x1234ul))
            << "bus reported success for writing to unmapped address";

        u32 data;
        ASSERT_OK(out.readw<u32>(0x0000, data))
            << "cannot read 0x0000 (mem1 + 0x0)";
        EXPECT_EQ(data, 0x11111111ul)
            << "read invalid data from 0x0000 (mem1 + 0x0)";
        ASSERT_OK(out.readw<u32>(0x0004, data))
            << "cannot read 0x0004 (mem1 + 0x4)";
        EXPECT_EQ(data, 0xfffffffful)
            << "read invalid data from 0x0004 (mem1 + 0x4)";
        ASSERT_OK(out.readw<u32>(0x2000, data))
            << "cannot read 0x2000 (mem2 + 0x0)";
        EXPECT_EQ(data, 0x55555555ul)
            << "read invalid data from 0x2000 (mem2 + 0x0)";
        ASSERT_OK(out.readw<u32>(0x2004, data))
            << "cannot read 0x2004 (mem2 + 0x4)";
        EXPECT_EQ(data, 0xbbbbbbbbul)
            << "read invalid data from 0x2000 (mem2 + 0x4)";
        ASSERT_AE(out.readw<u32>(0x4000, data))
            << "bus reported success for reading from unmapped address";

        tlm_dmi dmi;
        EXPECT_TRUE(out.dmi().lookup(0x0000, 0x2000, TLM_READ_COMMAND, dmi))
            << "bus did not forward DMI region of mem1";
        EXPECT_TRUE(out.dmi().lookup(0x2000, 0x2000, TLM_READ_COMMAND, dmi))
            << "bus did not forward DMI region of mem2";

        if (out.dmi().get_entries().size() > 1) {
            EXPECT_NE(out.dmi().get_entries()[0].get_start_address(),
                      out.dmi().get_entries()[1].get_start_address())
                << "bus forwarded overlapping DMI regions";
            EXPECT_NE(out.dmi().get_entries()[0].get_dmi_ptr(),
                      out.dmi().get_entries()[1].get_dmi_ptr())
                << "bus forwarded overlapping DMI pointers";
        }

        mem1.unmap_dmi(0, 0x1fff);
        ASSERT_EQ(out.dmi().get_entries().size(), 1)
            << "bus did not forward DMI invalidation";
        EXPECT_EQ(out.dmi().get_entries()[0].get_start_address(), 0x2000)
            << "bus invalidated wrong DMI region";
    }
};

TEST(generic_bus, transfer) {
    bus_harness test("harness");
    sc_core::sc_start();
}
