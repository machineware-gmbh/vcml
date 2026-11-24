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

enum : u64 {
    VIC_IRQS = 0x000,
    VIC_FIQS = 0x004,
    VIC_RISR = 0x008,
    VIC_INTS = 0x00c,
    VIC_INTE = 0x010,
    VIC_IECR = 0x014,
    VIC_SINT = 0x018,
    VIC_SICR = 0x01c,
    VIC_PROT = 0x020,
    VIC_ADDR = 0x030,
    VIC_DEFA = 0x034,
    VIC_VADDR = 0x100,
    VIC_VCTRL = 0x200,
    VIC_PID = 0xfe0,
    VIC_CID = 0xff0,
};

class pl190vic_test : public test_base
{
public:
    arm::pl190vic vic;
    tlm_initiator_socket out;
    gpio_initiator_array<32> src;
    gpio_target_socket irq;
    gpio_target_socket fiq;

    pl190vic_test(const sc_module_name& nm):
        test_base(nm),
        vic("vic"),
        out("out"),
        src("src"),
        irq("irq"),
        fiq("fiq") {
        out.bind(vic.in);
        rst.bind(vic.rst);
        clk.bind(vic.clk);
        vic.irq_out[0].bind(irq);
        vic.fiq_out[0].bind(fiq);
        for (size_t i = 0; i < 32; i++)
            src[i].bind(vic.irq_in[i]);

        add_test("strings", &pl190vic_test::test_strings);
        add_test("ids", &pl190vic_test::test_ids);
        add_test("irq", &pl190vic_test::test_irq);
        add_test("fiq", &pl190vic_test::test_fiq);
        add_test("soft", &pl190vic_test::test_soft);
        add_test("registers", &pl190vic_test::test_registers);
        add_test("vectors", &pl190vic_test::test_vectors);
    }

    u32 read(u64 addr) {
        u32 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void test_strings() {
        EXPECT_STREQ(vic.kind(), "vcml::arm::pl190vic");
        EXPECT_STREQ(vic.version(), VCML_VERSION_STRING);
    }

    void test_ids() {
        u32 pid = 0;
        u32 cid = 0;
        for (size_t i = 0; i < 4; i++) {
            pid |= read(VIC_PID + i * 4) << (i * 8);
            cid |= read(VIC_CID + i * 4) << (i * 8);
        }

        EXPECT_EQ(pid, arm::pl190vic::AMBA_PID);
        EXPECT_EQ(cid, arm::pl190vic::AMBA_CID);

        u32 data = 0;
        EXPECT_CE(out.writew(VIC_PID, data));
        EXPECT_CE(out.writew(VIC_CID, data));
        EXPECT_CE(out.writew(VIC_IRQS, data));
        EXPECT_CE(out.writew(VIC_FIQS, data));
        EXPECT_CE(out.writew(VIC_RISR, data));
        EXPECT_CE(out.readw(VIC_SICR, data));
    }

    void test_irq() {
        EXPECT_FALSE(irq);
        EXPECT_FALSE(fiq);

        src[3] = true;
        EXPECT_EQ(read(VIC_RISR), bit(3));
        EXPECT_EQ(read(VIC_IRQS), 0);
        EXPECT_FALSE(irq);

        write(VIC_INTE, bit(3));
        EXPECT_EQ(read(VIC_INTE), bit(3));
        EXPECT_EQ(read(VIC_IRQS), bit(3));
        EXPECT_EQ(read(VIC_FIQS), 0);
        EXPECT_TRUE(irq);
        EXPECT_FALSE(fiq);

        write(VIC_INTE, bit(31));
        EXPECT_EQ(read(VIC_INTE), bit(31) | bit(3));
        src[31] = true;
        EXPECT_EQ(read(VIC_RISR), bit(31) | bit(3));
        EXPECT_EQ(read(VIC_IRQS), bit(31) | bit(3));

        src[3] = false;
        EXPECT_EQ(read(VIC_IRQS), bit(31));
        EXPECT_TRUE(irq);

        write(VIC_IECR, bit(31));
        EXPECT_EQ(read(VIC_INTE), bit(3));
        EXPECT_EQ(read(VIC_RISR), bit(31));
        EXPECT_EQ(read(VIC_IRQS), 0);
        EXPECT_FALSE(irq);

        src[31] = false;
        write(VIC_IECR, ~0u);
        EXPECT_EQ(read(VIC_INTE), 0);
        EXPECT_EQ(read(VIC_RISR), 0);
    }

    void test_fiq() {
        write(VIC_INTS, bit(7));
        write(VIC_INTE, bit(7) | bit(8));
        EXPECT_EQ(read(VIC_INTS), bit(7));

        src[7] = true;
        EXPECT_EQ(read(VIC_FIQS), bit(7));
        EXPECT_EQ(read(VIC_IRQS), 0);
        EXPECT_TRUE(fiq);
        EXPECT_FALSE(irq);

        src[8] = true;
        EXPECT_EQ(read(VIC_FIQS), bit(7));
        EXPECT_EQ(read(VIC_IRQS), bit(8));
        EXPECT_TRUE(fiq);
        EXPECT_TRUE(irq);

        write(VIC_INTS, bit(8));
        EXPECT_EQ(read(VIC_FIQS), bit(8));
        EXPECT_EQ(read(VIC_IRQS), bit(7));
        EXPECT_TRUE(fiq);
        EXPECT_TRUE(irq);

        write(VIC_INTS, 0);
        EXPECT_EQ(read(VIC_FIQS), 0);
        EXPECT_EQ(read(VIC_IRQS), bit(7) | bit(8));
        EXPECT_FALSE(fiq);
        EXPECT_TRUE(irq);

        src[7] = false;
        src[8] = false;
        write(VIC_IECR, ~0u);
        EXPECT_FALSE(irq);
    }

    void test_soft() {
        write(VIC_INTE, bit(4));
        write(VIC_SINT, bit(4));
        write(VIC_SINT, bit(9));
        EXPECT_EQ(read(VIC_SINT), bit(4) | bit(9));
        EXPECT_EQ(read(VIC_RISR), bit(4) | bit(9));
        EXPECT_EQ(read(VIC_IRQS), bit(4));
        EXPECT_TRUE(irq);

        write(VIC_SICR, bit(4));
        EXPECT_EQ(read(VIC_SINT), bit(9));
        EXPECT_EQ(read(VIC_RISR), bit(9));
        EXPECT_EQ(read(VIC_IRQS), 0);
        EXPECT_FALSE(irq);

        src[9] = true;
        write(VIC_SICR, bit(9));
        EXPECT_EQ(read(VIC_SINT), 0);
        EXPECT_EQ(read(VIC_RISR), bit(9));
        src[9] = false;
        EXPECT_EQ(read(VIC_RISR), 0);

        write(VIC_IECR, ~0u);
    }

    void test_registers() {
        write(VIC_PROT, 1);
        EXPECT_EQ(read(VIC_PROT), 1);
        write(VIC_DEFA, 0xdef0);
        EXPECT_EQ(read(VIC_DEFA), 0xdef0);

        for (u32 i = 0; i < arm::pl190vic::NVEC; i++) {
            write(VIC_VADDR + i * 4, 0x1000 + i);
            write(VIC_VCTRL + i * 4, 0xffffff00 | i);
        }

        for (u32 i = 0; i < arm::pl190vic::NVEC; i++) {
            EXPECT_EQ(read(VIC_VADDR + i * 4), 0x1000 + i) << "vector " << i;
            EXPECT_EQ(read(VIC_VCTRL + i * 4), i) << "vector " << i;
        }

        write(VIC_VCTRL, 0xff);
        EXPECT_EQ(read(VIC_VCTRL), arm::pl190vic::VCTRL_M);
        EXPECT_EQ(read(VIC_VCTRL + 4), 1);
    }

    void test_vectors() {
        write(VIC_DEFA, 0xdef0);
        write(VIC_VADDR + 0 * 4, 0x1000);
        write(VIC_VADDR + 1 * 4, 0x2000);
        write(VIC_VADDR + 2 * 4, 0x3000);
        write(VIC_VCTRL + 0 * 4, arm::pl190vic::VCTRL_ENABLED | 5);
        write(VIC_VCTRL + 1 * 4, arm::pl190vic::VCTRL_ENABLED | 6);
        write(VIC_VCTRL + 2 * 4, 7);
        write(VIC_INTE, bit(5) | bit(6) | bit(7));

        src[6] = true;
        EXPECT_TRUE(irq);
        EXPECT_EQ(read(VIC_ADDR), 0x2000);

        src[5] = true;
        EXPECT_EQ(read(VIC_ADDR), 0x1000);

        write(VIC_ADDR, 0);
        EXPECT_EQ(read(VIC_VADDR + 0 * 4), 0x1000);
        EXPECT_EQ(read(VIC_VADDR + 1 * 4), 0x2000);
        EXPECT_EQ(read(VIC_VADDR + 2 * 4), 0x3000);

        src[5] = false;
        src[6] = false;
        write(VIC_IECR, ~0u);
        EXPECT_FALSE(irq);
    }
};

TEST(arm, pl190vic) {
    pl190vic_test test("test");
    sc_core::sc_start();
}
