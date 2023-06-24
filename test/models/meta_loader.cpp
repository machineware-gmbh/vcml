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

class loader_test : public test_base
{
public:
    generic::memory imem;
    generic::memory dmem;

    generic::bus ibus;
    generic::bus dbus;

    meta::loader loader;

    loader_test(const sc_module_name& nm = sc_gen_unique_name("test")):
        test_base(nm),
        imem("imem", 0x2000),
        dmem("dmem", 0x2000),
        ibus("ibus"),
        dbus("dbus"),
        loader("loader", { get_resource_path("elf.elf") }) {
        ibus.bind(loader.insn);
        ibus.bind(imem.in, { 0x400000, 0x400fff });

        dbus.bind(loader.data);
        dbus.bind(dmem.in, { 0x601000, 0x601fff });

        clk.bind(imem.clk);
        clk.bind(dmem.clk);
        clk.bind(ibus.clk);
        clk.bind(dbus.clk);

        rst.bind(imem.rst);
        rst.bind(dmem.rst);
        rst.bind(ibus.rst);
        rst.bind(dbus.rst);
    }

    virtual void run_test() override {
        u32 code_start = 0;
        u32 global_a = 0;
        u64 global_b = 0;

        ASSERT_OK(loader.insn.readw(0x400000, code_start));
        ASSERT_OK(loader.data.readw(0x601000, global_b));
        ASSERT_OK(loader.data.readw(0x601008, global_a));

        EXPECT_EQ(code_start, fourcc("\x7f"
                                     "ELF"));
        EXPECT_EQ(global_a, 4);
        EXPECT_EQ(global_b, 0x42);
    }
};

TEST(loader, simulate) {
    loader_test stim;
    sc_core::sc_start();
}
