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

class bus_harness: public test_base
{
public:
    generic::memory mem1;
    generic::memory mem2;
    generic::bus bus;

    master_socket OUT;

    bus_harness(const sc_module_name& nm):
        test_base(nm),
        mem1("MEM1", 0x2000),
        mem2("MEM2", 0x2000),
        bus("BUS"),
        OUT("OUT") {

        mem1.CLOCK.stub(100 * MHz);
        mem2.CLOCK.stub(100 * MHz);
        bus.CLOCK.stub(100 * MHz);
        CLOCK.stub(100 * MHz);

        mem1.RESET.stub();
        mem2.RESET.stub();
        bus.RESET.stub();
        RESET.stub();

        bus.bind(OUT);
        bus.bind(mem1.IN, 0x0000, 0x1fff, 0);
        bus.bind(mem2.IN, 0x2000, 0x3fff, 0);
    }

    virtual void run_test() override {
        ASSERT_OK(OUT.writew<u32>(0x0000, 0x11111111ul))
            << "cannot write 0x0000 (mem1 + 0x0)";
        ASSERT_OK(OUT.writew<u32>(0x0004, 0xfffffffful))
            << "cannot write 0x0004 (mem1 + 0x4)";
        ASSERT_OK(OUT.writew<u32>(0x2000, 0x55555555ul))
            << "cannot write 0x2000 (mem2 + 0x0)";
        ASSERT_OK(OUT.writew<u32>(0x2004, 0xbbbbbbbbul))
            << "cannot write 0x2004 (mem2 + 0x4)";
        ASSERT_AE(OUT.writew<u16>(0x4000, 0x1234ul))
            << "bus reported success for writing to unmapped address";

        u32 data;
        ASSERT_OK(OUT.readw<u32>(0x0000, data))
            << "cannot read 0x0000 (mem1 + 0x0)";
        EXPECT_EQ(data, 0x11111111ul)
            << "read invalid data from 0x0000 (mem1 + 0x0)";
        ASSERT_OK(OUT.readw<u32>(0x0004, data))
            << "cannot read 0x0004 (mem1 + 0x4)";
        EXPECT_EQ(data, 0xfffffffful)
            << "read invalid data from 0x0004 (mem1 + 0x4)";
        ASSERT_OK(OUT.readw<u32>(0x2000, data))
            << "cannot read 0x2000 (mem2 + 0x0)";
        EXPECT_EQ(data, 0x55555555ul)
            << "read invalid data from 0x2000 (mem2 + 0x0)";
        ASSERT_OK(OUT.readw<u32>(0x2004, data))
            << "cannot read 0x2004 (mem2 + 0x4)";
        EXPECT_EQ(data, 0xbbbbbbbbul)
            << "read invalid data from 0x2000 (mem2 + 0x4)";
        ASSERT_AE(OUT.readw<u32>(0x4000, data))
            << "bus reported success for reading from unmapped address";

        ASSERT_EQ(OUT.dmi().get_entries().size(), 2)
            << "bus did not forward DMI regions for both memories";
        EXPECT_NE(OUT.dmi().get_entries()[0].get_start_address(),
                  OUT.dmi().get_entries()[1].get_start_address())
            << "bus forwarded overlapping DMI regions";
        EXPECT_NE(OUT.dmi().get_entries()[0].get_dmi_ptr(),
                  OUT.dmi().get_entries()[1].get_dmi_ptr())
            << "bus forwarded overlapping DMI pointers";

        mem1.unmap_dmi(0, 0x1fff);
        ASSERT_EQ(OUT.dmi().get_entries().size(), 1)
            << "bus did not forward DMI invalidation";
        EXPECT_EQ(OUT.dmi().get_entries()[0].get_start_address(), 0x2000)
            << "bus invalidated wrong DMI region";
    }

};

TEST(generic_bus, transfer) {
    bus_harness test("harness");
    sc_core::sc_start();
}
