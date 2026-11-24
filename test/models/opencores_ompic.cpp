/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : u32 {
    NCPUS = 4,
    CTRL_IRQ_ACK = 1u << 31,
    CTRL_IRQ_GEN = 1u << 30,
};

class ompic_test : public test_base
{
public:
    model ompic;
    tlm_initiator_socket out;
    gpio_target_array<NCPUS> irq;

    ompic_test(const sc_module_name& nm):
        test_base(nm),
        ompic("ompic", "vcml::opencores::ompic 4"),
        out("out"),
        irq("irq") {
        tlm_bind(*this, "out", ompic, "in");
        gpio_bind(*this, "rst", ompic, "rst");
        clk_bind(*this, "clk", ompic, "clk");
        for (size_t i = 0; i < NCPUS; i++)
            gpio_bind(ompic, "irq", i, *this, "irq", i);

        add_test("strings", &ompic_test::test_strings);
        add_test("interrupt", &ompic_test::test_interrupt);
        add_test("self", &ompic_test::test_self);
        add_test("invalid", &ompic_test::test_invalid);
    }

    static constexpr u64 control(u32 cpu) { return cpu * 8; }
    static constexpr u64 status(u32 cpu) { return cpu * 8 + 4; }
    static constexpr u32 ipi(u32 dest, u32 data) { return dest << 16 | data; }

    u32 read(u64 addr) {
        u32 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void test_strings() {
        EXPECT_STREQ(ompic->kind(), "vcml::opencores::ompic");
        EXPECT_STREQ(ompic->version(), VCML_VERSION_STRING);
    }

    void test_interrupt() {
        for (u32 cpu = 0; cpu < NCPUS; cpu++) {
            EXPECT_EQ(read(control(cpu)), 0);
            EXPECT_EQ(read(status(cpu)), 0);
            EXPECT_FALSE(irq[cpu]);
        }

        write(control(1), CTRL_IRQ_GEN | ipi(2, 0x1234));
        EXPECT_EQ(read(control(1)), CTRL_IRQ_GEN | ipi(2, 0x1234));
        EXPECT_FALSE(irq[0]);
        EXPECT_FALSE(irq[1]);
        EXPECT_TRUE(irq[2]);
        EXPECT_FALSE(irq[3]);
        EXPECT_EQ(read(status(2)), CTRL_IRQ_GEN | ipi(1, 0x1234));
        EXPECT_EQ(read(status(1)), 0);

        write(control(3), CTRL_IRQ_GEN | ipi(2, 0x5678));
        EXPECT_TRUE(irq[2]);
        EXPECT_EQ(read(status(2)), CTRL_IRQ_GEN | ipi(3, 0x5678));

        write(control(2), CTRL_IRQ_ACK);
        EXPECT_FALSE(irq[2]);
        EXPECT_EQ(read(status(2)), ipi(3, 0x5678));
        EXPECT_EQ(read(control(2)), CTRL_IRQ_ACK);

        write(control(2), CTRL_IRQ_ACK);
        EXPECT_FALSE(irq[2]);

        write(control(0), ipi(3, 0xffff));
        EXPECT_FALSE(irq[3]);
        EXPECT_EQ(read(status(3)), 0);

        u32 data = 0;
        EXPECT_CE(out.writew(status(0), data));
    }

    void test_self() {
        write(control(0), CTRL_IRQ_GEN | ipi(0, 0x1111));
        EXPECT_TRUE(irq[0]);
        EXPECT_EQ(read(status(0)), CTRL_IRQ_GEN | ipi(0, 0x1111));

        write(control(0), CTRL_IRQ_ACK | CTRL_IRQ_GEN | ipi(3, 0x2222));
        EXPECT_FALSE(irq[0]);
        EXPECT_TRUE(irq[3]);
        EXPECT_EQ(read(status(3)), CTRL_IRQ_GEN | ipi(0, 0x2222));

        write(control(3), CTRL_IRQ_ACK);
        EXPECT_FALSE(irq[3]);
    }

    void test_invalid() {
        write(control(0), ipi(1, 0x4321));
        write(control(0), CTRL_IRQ_GEN | ipi(NCPUS, 0xbeef));
        EXPECT_EQ(read(control(0)), ipi(1, 0x4321));
        for (u32 cpu = 0; cpu < NCPUS; cpu++)
            EXPECT_FALSE(irq[cpu]);

        u32 data = 0;
        EXPECT_AE(out.readw(control(NCPUS), data));
        EXPECT_AE(out.writew(control(NCPUS), data));
    }
};

TEST(opencores, ompic) {
    ompic_test test("test");
    sc_core::sc_start();
}
