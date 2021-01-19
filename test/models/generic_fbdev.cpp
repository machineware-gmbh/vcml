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
#define SIZE (RESX * RESY * 4)

class test_harness: public test_base
{
public:
    generic::fbdev fb;
    generic::memory vmem;

    test_harness(const sc_module_name& nm):
        test_base(nm),
        fb("fb", RESX, RESY),
        vmem("vmem", SIZE) {
        vmem.CLOCK.stub();
        vmem.RESET.stub();
        fb.CLOCK.stub(60);
        fb.RESET.stub();
        fb.OUT.bind(vmem.IN);
        fb.display = "display:0";
    }

    virtual void run_test() override {
        ASSERT_EQ(fb.resx, RESX) << "unexpect screen width";
        ASSERT_EQ(fb.resy, RESY) << "unexpected screen height";
        ASSERT_EQ(fb.stride(), RESX * 4) << "wrong stride";
        ASSERT_EQ(fb.size(), RESX * RESY * 4) << "wrong size";

        wait(1.0, SC_SEC);

        EXPECT_EQ(fb.vptr(), vmem.get_data_ptr());
    }

};

TEST(generic_fbdev, run) {
    test_harness test("harness");
    sc_core::sc_start();
}
