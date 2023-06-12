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

class plic_stim : public test_base
{
public:
    tlm_initiator_socket out;

    gpio_target_socket irqt1;
    gpio_target_socket irqt2;

    gpio_initiator_socket irqs1;
    gpio_initiator_socket irqs2;

    plic_stim(const sc_module_name& nm):
        test_base(nm),
        out("out"),
        irqt1("irqt1"),
        irqt2("irqt2"),
        irqs1("irqs1"),
        irqs2("irqs2") {}

    virtual void run_test() override {
        // test that interrupts are reset
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irqt1.read()) << "irqt1 not reset";
        EXPECT_FALSE(irqt2.read()) << "irqt2 not reset";
        EXPECT_FALSE(irqs1.read()) << "irqs1 not reset";
        EXPECT_FALSE(irqs2.read()) << "irqs2 not reset";

        // test that interrupts are disabled
        irqs1.write(true);
        irqs2.write(true);
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irqt1.read()) << "irqt1 not disabled";
        EXPECT_FALSE(irqt2.read()) << "irqt2 not disabled";

        // test that irq propagation works
        EXPECT_OK(out.writew(0x201000, 0u)); // context 1 threshold
        EXPECT_OK(out.writew(0x202000, 0u)); // context 2 threshold
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irqt1.read()) << "irqt1 received though disabled";
        EXPECT_FALSE(irqt2.read()) << "irqt2 received though disabled";

        EXPECT_OK(out.writew(0x000004, 1u)); // irq 1 priority
        EXPECT_OK(out.writew(0x000008, 1u)); // irq 2 priority
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irqt1.read()) << "irqt1 received though disabled";
        EXPECT_FALSE(irqt2.read()) << "irqt2 received though disabled";

        EXPECT_OK(out.writew(0x002080, 6u)); // enable irq1+2 on ctx 1
        wait(SC_ZERO_TIME);
        EXPECT_TRUE(irqt1.read()) << "irqt1 not received";
        EXPECT_FALSE(irqt2.read()) << "irqt2 received though disabled";

        EXPECT_OK(out.writew(0x002100, 6u)); // enable irq1+2 on ctx 2
        wait(SC_ZERO_TIME);
        EXPECT_TRUE(irqt1.read()) << "irqt1 not received";
        EXPECT_TRUE(irqt2.read()) << "irqt2 not received";

        // test that claiming works
        vcml::u32 claim1, claim2;
        EXPECT_OK(out.readw(0x201004, claim1)) << "cannot read CTX1_CLAIM";
        EXPECT_OK(out.readw(0x202004, claim2)) << "cannot read CTX2_CLAIM";
        EXPECT_EQ(claim1, 1) << "irqs1 should have been claimed";
        EXPECT_EQ(claim2, 2) << "irqs2 should have been claimed";
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irqt1.read()) << "irqt1 not reset";
        EXPECT_FALSE(irqt2.read()) << "irqt2 not reset";

        // test that completing works
        irqs1.write(false);
        wait(SC_ZERO_TIME);
        EXPECT_OK(out.writew(0x201004, claim1))
            << "cannot write CTX1_COMPLETE";
        EXPECT_OK(out.writew(0x202004, claim2))
            << "cannot write CTX2_COMPLETE";
        wait(SC_ZERO_TIME);
        EXPECT_TRUE(irqt1.read()) << "irqt1 should be active due to irqs2";
        EXPECT_TRUE(irqt2.read()) << "irqt2 should be active due to irqs2";
        irqs2.write(false);
        wait(SC_ZERO_TIME);
        EXPECT_OK(out.writew(0x202004, claim2))
            << "cannot write CTX2_COMPLETE";
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irqt1.read()) << "irqt1 not disabled";
        EXPECT_FALSE(irqt2.read()) << "irqt2 not disabled";
    }
};

TEST(plic, plic) {
    plic_stim stim("STIM");
    riscv::plic plic("PLIC");

    stim.clk.bind(plic.clk);
    stim.rst.bind(plic.rst);

    stim.out.bind(plic.in);

    plic.irqt[1].bind(stim.irqt1);
    plic.irqt[2].bind(stim.irqt2);

    stim.irqs1.bind(plic.irqs[1]);
    stim.irqs2.bind(plic.irqs[2]);

    sc_core::sc_start();
}
