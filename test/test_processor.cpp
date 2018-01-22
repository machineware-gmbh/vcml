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
    mock_processor(const sc_core::sc_module_name& nm, clock_t clk):
        vcml::processor(nm, clk) {}
    virtual ~mock_processor() {}

    MOCK_METHOD2(interrupt, void(unsigned int,bool));
    MOCK_METHOD1(simulate, void(unsigned int&));
};

TEST(processor, processor) {
    vcml::generic::memory imem("IMEM", 0x1000);
    vcml::generic::memory dmem("DMEM", 0x1000);

    sc_core::sc_signal<bool> irq0("IRQ0");
    sc_core::sc_signal<bool> irq1("IRQ1");

    mock_processor cpu("CPU", 100000000);
    cpu.INSN.bind(imem.IN);
    cpu.DATA.bind(dmem.IN);
    cpu.IRQ[0].bind(irq0);
    cpu.IRQ[1].bind(irq1);

    sc_core::sc_time quantum(1.0, sc_core::SC_SEC);
    sc_core::sc_time cycle(1.0 / cpu.clock, sc_core::SC_SEC);

    tlm::tlm_global_quantum::instance().set(quantum);

    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(1);
    sc_core::sc_start(quantum);

    irq0.write(true);
    EXPECT_CALL(cpu, interrupt(0,true)).Times(1);
    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(1);
    sc_core::sc_start(quantum);

    irq0.write(false);
    EXPECT_CALL(cpu, interrupt(0,false)).Times(1);
    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(1);
    sc_core::sc_start(quantum);

    irq1.write(true);
    EXPECT_CALL(cpu, interrupt(1,true)).Times(1);
    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(1);
    sc_core::sc_start(quantum);

    irq1.write(false);
    EXPECT_CALL(cpu, interrupt(1,false)).Times(1);
    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(1);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).WillOnce(SetArgReferee<0>((quantum / cycle) * 2));
    sc_core::sc_start(quantum);
    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(0);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, simulate(Eq(quantum / cycle))).Times(1);
    sc_core::sc_start(quantum);
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
