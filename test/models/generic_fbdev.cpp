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

#define RESX 1280
#define RESY  720

class test_harness: public test_base
{
public:
    u8 vmem[RESX * RESY * 4];
    generic::fbdev fb;
    slave_socket IN;

    test_harness(const sc_core::sc_module_name& nm):
        test_base(nm),
        fb("fb", RESX, RESY),
        IN("IN") {
        fb.OUT.bind(IN);
        fb.CLOCK.stub(60);
        fb.RESET.stub();
        fb.display = "display:44444";
        map_dmi(vmem, 0, sizeof(vmem) - 1, VCML_ACCESS_READ);
    }

    virtual void run_test() override {
        ASSERT_EQ(fb.resx, RESX) << "unexpect screen width";
        ASSERT_EQ(fb.resy, RESY) << "unexpected screen height";
        ASSERT_EQ(fb.stride(), RESX * 4) << "wrong stride";
        ASSERT_EQ(fb.size(), RESX * RESY * 4) << "wrong size";

        wait(1.0, SC_SEC);

        EXPECT_EQ(fb.vptr(), vmem);
    }

};

TEST(generic_memory, access) {
    test_harness test("harness");
    sc_core::sc_start();
}
