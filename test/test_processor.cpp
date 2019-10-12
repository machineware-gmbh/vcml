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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

class mock_processor: public vcml::processor
{
public:
    vcml::u64 cycles;

    mock_processor(const sc_core::sc_module_name& nm):
        vcml::processor(nm), cycles(0) {}
    virtual ~mock_processor() {}

    virtual vcml::u64 cycle_count() const override {
        return cycles;
    }

    virtual void simulate(unsigned int n) override {
        sc_core::sc_time now = sc_core::sc_time_stamp();
        ASSERT_EQ(local_time_stamp(), now);

        simulatem(n);
        cycles += n;
        update_local_time();

        ASSERT_EQ(local_time(), clock_cycles(n));
    }

    MOCK_METHOD2(interrupt, void(unsigned int,bool));
    MOCK_METHOD1(simulatem, void(unsigned int));
    MOCK_METHOD0(reset, void(void));
    MOCK_METHOD2(handle_clock_update, void(clock_t,clock_t));
};

TEST(processor, processor) {
    sc_core::sc_signal<clock_t> clk("CLK");
    sc_core::sc_signal<bool> rst("RST");

    sc_core::sc_signal<bool> irq0("IRQ0");
    sc_core::sc_signal<bool> irq1("IRQ1");

    vcml::generic::memory imem("IMEM", 0x1000);
    vcml::generic::memory dmem("DMEM", 0x1000);

    mock_processor cpu("CPU");

    cpu.CLOCK.bind(clk);
    cpu.RESET.bind(rst);
    imem.CLOCK.bind(clk);
    imem.RESET.bind(rst);
    dmem.CLOCK.bind(clk);
    dmem.RESET.bind(rst);

    cpu.INSN.bind(imem.IN);
    cpu.DATA.bind(dmem.IN);
    cpu.IRQ[0].bind(irq0);
    cpu.IRQ[1].bind(irq1);

    vcml::clock_t defclk = 1 * vcml::kHz;
    clk.write(defclk);
    rst.write(false);

    // finish elaboration
    EXPECT_CALL(cpu, handle_clock_update(0, defclk)).Times(1);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    sc_core::sc_time quantum(1.0, sc_core::SC_SEC);
    sc_core::sc_time cycle = cpu.clock_cycle();
    tlm::tlm_global_quantum::instance().set(quantum);



    // test processor::simulate
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(10);
    sc_core::sc_start(10 * quantum);



    // test processor::interrupt
    irq0.write(true);
    EXPECT_CALL(cpu, interrupt(0, true)).Times(1);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    irq0.write(false);
    EXPECT_CALL(cpu, interrupt(0, false)).Times(1);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    irq1.write(true);
    EXPECT_CALL(cpu, interrupt(1, true)).Times(1);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    irq1.write(false);
    EXPECT_CALL(cpu, interrupt(1, false)).Times(1);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);



    // test processor::reset
    rst.write(true);
    EXPECT_CALL(cpu, reset()).Times(AtMost(1));
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(1);
    sc_core::sc_start(10 * quantum);

    rst.write(false);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(AtLeast(9));
    sc_core::sc_start(10 * quantum);



    // test processor::handle_clock_update
    clk.write(0);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(AtMost(1));
    EXPECT_CALL(cpu, handle_clock_update(Eq(defclk), Eq(0))).Times(1);
    sc_core::sc_start(10 * quantum);

    clk.write(defclk);
    EXPECT_CALL(cpu, simulatem(quantum / cycle)).Times(AtLeast(9));
    EXPECT_CALL(cpu, handle_clock_update(Eq(0), Eq(defclk))).Times(1);
    sc_core::sc_start(10 * quantum);
}
