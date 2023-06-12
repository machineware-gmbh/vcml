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

#define XRES 1280
#define YRES 720
#define SIZE (XRES * YRES * 4)

class test_harness : public test_base
{
public:
    generic::fbdev fb;
    generic::memory vmem;

    test_harness(const sc_module_name& nm):
        test_base(nm), fb("fb", XRES, YRES), vmem("vmem", SIZE) {
        vmem.clk.stub();
        vmem.rst.stub();
        fb.clk.stub(60);
        fb.rst.stub();
        fb.out.bind(vmem.in);
    }

    virtual void run_test() override {
        ASSERT_EQ(fb.xres, XRES) << "unexpected screen width";
        ASSERT_EQ(fb.yres, YRES) << "unexpected screen height";
        ASSERT_EQ(fb.stride(), XRES * 4) << "wrong stride";
        ASSERT_EQ(fb.size(), XRES * YRES * 4) << "wrong size";

        wait(1.0, SC_SEC);

        EXPECT_EQ(fb.vptr(), vmem.data());
    }
};

TEST(generic_fbdev, run) {
    vcml::broker broker("test");
    broker.define("harness.fb.displays", "null:0");
    test_harness test("harness");
    sc_core::sc_start();
}
