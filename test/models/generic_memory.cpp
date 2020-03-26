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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <systemc>
#include "vcml.h"

using namespace ::testing;
using namespace ::sc_core;
using namespace ::vcml;

#define ASSERT_OK(tlmcall) ASSERT_EQ(tlmcall, TLM_OK_RESPONSE)
#define ASSERT_AE(tlmcall) ASSERT_EQ(tlmcall, TLM_ADDRESS_ERROR_RESPONSE)
#define ASSERT_CE(tlmcall) ASSERT_EQ(tlmcall, TLM_COMMAND_ERROR_RESPONSE)

class test_harness: public component
{
public:
    vcml::generic::memory mem;
    master_socket OUT;

    test_harness(const sc_core::sc_module_name& nm):
        component(nm),
        mem("mem", 0x1000),
        OUT("OUT") {
        OUT.bind(mem.IN);

        CLOCK.stub(10 * MHz);
        RESET.stub();

        mem.CLOCK.stub(10 * MHz);
        mem.RESET.stub();

        SC_HAS_PROCESS(test_harness);
        SC_THREAD(run_test);
    }

    void run_test() {
        wait(SC_ZERO_TIME);

        ASSERT_OK(OUT.writew(0x0, 0x11223344))
            << "cannot write 32bits to address 0";
        ASSERT_OK(OUT.writew(0x4, 0x55667788))
            << "cannot write 32bits to address 4";

        u64 data;
        ASSERT_OK(OUT.readw(0x0, data))
            << "cannot read 64bits from address 0";
        EXPECT_EQ(data, 0x5566778811223344ull)
            << "data read from address 4 is invalid";

        EXPECT_GT(mem.IN.dmi().get_entries().size(), 0)
            << "memory does not provide DMI access";
        EXPECT_GT(OUT.dmi().get_entries().size(), 0)
            << "did not get DMI access to memory";

        mem.readonly = true;

        ASSERT_CE(OUT.writew(0x0, 0xfefefefe, SBI_NODMI))
            << "read-only memory permitted write access";
        ASSERT_OK(OUT.writew(0x0, 0xfefefefe, SBI_DEBUG))
            << "read-only memory did not permit debug write access";

        ASSERT_OK(OUT.writew(0x0, 0xfefefefe))
            << "DMI was denied or invalidated prematurely";
        OUT.dmi().invalidate(0, -1);
        ASSERT_CE(OUT.writew(0x0, 0xfefefefe))
            << "read-only memory permitted write access after DMI invalidate";

        sc_stop();
        return;
    }
};

TEST(generic_memory, access) {
    test_harness test("harness");

    sc_start();

    ASSERT_EQ(sc_get_status(), SC_STOPPED) << "simulation incomplete";
}
