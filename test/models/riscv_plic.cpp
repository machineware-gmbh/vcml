/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

class plic_stim: public test_base
{
public:
    master_socket OUT;

    sc_in<bool> IRQT1;
    sc_in<bool> IRQT2;

    sc_out<bool> IRQS1;
    sc_out<bool> IRQS2;

    plic_stim(const sc_module_name& nm):
        test_base(nm),
        OUT("OUT"),
        IRQT1("IRQT1"),
        IRQT2("IRQT2"),
        IRQS1("IRQS1"),
        IRQS2("IRQS2") {
    }

    virtual void run_test() override {
        // test that interrupts are reset
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(IRQT1.read()) << "irqt1 not reset";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 not reset";
        EXPECT_FALSE(IRQS1.read()) << "irqs1 not reset";
        EXPECT_FALSE(IRQS2.read()) << "irqs2 not reset";


        // test that interrupts are disabled
        IRQS1.write(true);
        IRQS2.write(true);
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(IRQT1.read()) << "irqt1 not disabled";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 not disabled";


        // test that irq propagation works
        EXPECT_OK(OUT.writew(0x201000, 0u)); // context 1 threshold
        EXPECT_OK(OUT.writew(0x202000, 0u)); // context 2 threshold
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(IRQT1.read()) << "irqt1 received though disabled";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 received though disabled";

        EXPECT_OK(OUT.writew(0x000004, 1u)); // irq 1 priority
        EXPECT_OK(OUT.writew(0x000008, 1u)); // irq 2 priority
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(IRQT1.read()) << "irqt1 received though disabled";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 received though disabled";

        EXPECT_OK(OUT.writew(0x002080, 6u)); // enable irq1+2 on ctx 1
        wait(SC_ZERO_TIME);
        EXPECT_TRUE(IRQT1.read()) << "irqt1 not received";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 received though disabled";

        EXPECT_OK(OUT.writew(0x002100, 6u)); // enable irq1+2 on ctx 2
        wait(SC_ZERO_TIME);
        EXPECT_TRUE(IRQT1.read()) << "irqt1 not received";
        EXPECT_TRUE(IRQT2.read()) << "irqt2 not received";


        // test that claiming works
        vcml::u32 claim1, claim2;
        EXPECT_OK(OUT.readw(0x201004, claim1)) << "cannot read CTX1_CLAIM";
        EXPECT_OK(OUT.readw(0x202004, claim2)) << "cannot read CTX2_CLAIM";
        EXPECT_EQ(claim1, 1) << "irqs1 should have been claimed";
        EXPECT_EQ(claim2, 2) << "irqs2 should have been claimed";
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(IRQT1.read()) << "irqt1 not reset";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 not reset";


        // test that completing works
        IRQS1.write(false);
        wait(SC_ZERO_TIME);
        EXPECT_OK(OUT.writew(0x201004, claim1)) << "cannot write CTX1_COMPLETE";
        EXPECT_OK(OUT.writew(0x202004, claim2)) << "cannot write CTX2_COMPLETE";
        wait(SC_ZERO_TIME);
        EXPECT_TRUE(IRQT1.read()) << "irqt1 should be active due to irqs2";
        EXPECT_TRUE(IRQT2.read()) << "irqt2 should be active due to irqs2";
        IRQS2.write(false);
        wait(SC_ZERO_TIME);
        EXPECT_OK(OUT.writew(0x202004, claim2)) << "cannot write CTX2_COMPLETE";
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(IRQT1.read()) << "irqt1 not disabled";
        EXPECT_FALSE(IRQT2.read()) << "irqt2 not disabled";
    }

};

TEST(plic, plic) {
    sc_signal<bool> irqs1("irqs1");
    sc_signal<bool> irqs2("irqs2");

    sc_signal<bool> irqt1("irqt1");
    sc_signal<bool> irqt2("irqt2");

    sc_signal<clock_t> clk("clk");

    plic_stim stim("STIM");
    riscv::plic plic("PLIC");
    generic::clock sysclk("SYSCLK", 100 * MHz);

    stim.CLOCK.bind(clk);
    plic.CLOCK.bind(clk);
    sysclk.CLOCK.bind(clk);

    EXPECT_EQ(stim.CLOCK.size(), 1);

    stim.RESET.stub();
    plic.RESET.stub();

    stim.OUT.bind(plic.IN);

    plic.IRQS[1].bind(irqs1);
    plic.IRQS[2].bind(irqs2);

    plic.IRQT[1].bind(irqt1);
    plic.IRQT[2].bind(irqt2);

    stim.IRQS1.bind(irqs1);
    stim.IRQS2.bind(irqs2);

    stim.IRQT1.bind(irqt1);
    stim.IRQT2.bind(irqt2);

    sc_core::sc_start();
}
