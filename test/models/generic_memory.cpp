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

class test_harness : public test_base
{
public:
    generic::memory ram;
    generic::memory rom;

    tlm_initiator_socket ram_port;
    tlm_initiator_socket rom_port;

    test_harness(const sc_module_name& nm):
        test_base(nm),
        ram("ram", 4 * KiB, false, VCML_ALIGN_2M),
        rom("rom", 4 * KiB, true, VCML_ALIGN_NONE),
        ram_port("ram_port"),
        rom_port("rom_port") {
        ram_port.bind(ram.in);
        rom_port.bind(rom.in);
        ram.rst.stub();
        rom.rst.stub();
        ram.clk.stub(10 * MHz);
        rom.clk.stub(10 * MHz);
    }

    virtual void run_test() override {
        ASSERT_OK(ram_port.writew(0x0, 0x11223344))
            << "cannot write 32bits to address 0";
        ASSERT_OK(ram_port.writew(0x4, 0x55667788))
            << "cannot write 32bits to address 4";

        u64 data;
        ASSERT_OK(ram_port.readw(0x0, data))
            << "cannot read 64bits from address 0";
        EXPECT_EQ(data, 0x5566778811223344ull)
            << "data read from address 4 is invalid";

        EXPECT_GT(ram.in.dmi_cache().get_entries().size(), 0)
            << "memory does not provide DMI access";
        EXPECT_GT(ram_port.dmi_cache().get_entries().size(), 0)
            << "did not get DMI access to memory";

        ASSERT_CE(rom_port.writew(0x0, 0xfefefefe, SBI_NODMI))
            << "read-only memory permitted write access";
        ASSERT_OK(rom_port.writew(0x0, 0xfefefefe, SBI_DEBUG))
            << "read-only memory did not permit debug write access";

        ASSERT_CE(rom_port.writew(0x0, 0xfefefefe))
            << "read-only memory permitted DMI write access";
        rom_port.dmi_cache().invalidate(0, -1);
        ASSERT_CE(rom_port.writew(0x0, 0xfefefefe))
            << "read-only memory permitted write access after DMI invalidate";

        ASSERT_TRUE(is_aligned(ram.data(), VCML_ALIGN_2M))
            << "memory is not 21 bit aligned";
    }
};

TEST(generic_memory, access) {
    test_harness test("harness");
    sc_core::sc_start();
}
