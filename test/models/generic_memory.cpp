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

class test_harness: public test_base
{
public:
    generic::memory mem;
    tlm_initiator_socket OUT;

    test_harness(const sc_core::sc_module_name& nm):
        test_base(nm),
        mem("mem", 0x1000, false, 21),
        OUT("OUT") {
        OUT.bind(mem.IN);
        mem.CLOCK.stub(10 * MHz);
        mem.RESET.stub();
    }

    virtual void run_test() override {
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

        unsigned char* data_ptr = mem.get_data_ptr();
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(data_ptr) & 0x1FFFFF, 0)
            << "memory is not 21 bit aligned";
    }

};

TEST(generic_memory, access) {
    test_harness test("harness");
    sc_core::sc_start();
}
