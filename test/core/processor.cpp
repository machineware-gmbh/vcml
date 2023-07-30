/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

const vcml::hz_t DEFCLK = 1 * vcml::kHz;

class mock_processor : public vcml::processor
{
public:
    vcml::u64 cycles;

    vcml::gpio_initiator_socket rst_out;
    vcml::clk_initiator_socket clk_out;

    vcml::gpio_initiator_socket irq0;
    vcml::gpio_initiator_socket irq1;

    MOCK_METHOD(void, interrupt, (size_t, bool), (override));
    MOCK_METHOD(void, simulate2, (size_t));
    MOCK_METHOD(void, reset, (), (override));
    MOCK_METHOD(void, handle_clock_update, (vcml::hz_t, vcml::hz_t),
                (override));

    mock_processor(const sc_core::sc_module_name& nm):
        vcml::processor(nm, "mock"),
        cycles(0),
        rst_out("rst_out"),
        clk_out("clk_out"),
        irq0("irq0"),
        irq1("irq1") {
        // nothing to do
    }

    virtual ~mock_processor() {
        // nothing to do
    }

    virtual vcml::u64 cycle_count() const override { return cycles; }

    virtual void simulate(size_t n) override {
        const sc_core::sc_time& now = sc_core::sc_time_stamp();
        ASSERT_EQ(local_time_stamp(), now);

        simulate2(n);
        cycles += n;

        ASSERT_EQ(local_time(), clock_cycles(n));
    }

    virtual void end_of_elaboration() override {
        clk_out = DEFCLK;
        rst_out.pulse();
    }
};

TEST(processor, processor) {
    vcml::generic::memory imem("IMEM", 0x1000);
    vcml::generic::memory dmem("DMEM", 0x1000);

    mock_processor cpu("CPU");

    cpu.clk_out.bind(cpu.clk);
    cpu.rst_out.bind(cpu.rst);
    cpu.clk_out.bind(imem.clk);
    cpu.rst_out.bind(imem.rst);
    cpu.clk_out.bind(dmem.clk);
    cpu.rst_out.bind(dmem.rst);

    cpu.insn.bind(imem.in);
    cpu.data.bind(dmem.in);
    cpu.irq[0].bind(cpu.irq0);
    cpu.irq[1].bind(cpu.irq1);

    // finish elaboration
    EXPECT_CALL(cpu, reset()).Times(1);
    EXPECT_CALL(cpu, handle_clock_update(0, DEFCLK)).Times(1);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    sc_core::sc_time quantum(1.0, sc_core::SC_SEC);
    sc_core::sc_time cycle = cpu.clock_cycle();
    tlm::tlm_global_quantum::instance().set(quantum);

    // test processor::simulate
    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(10);
    sc_core::sc_start(10 * quantum);

    // test processor::interrupt
    EXPECT_CALL(cpu, interrupt(0, true)).Times(1);
    cpu.irq0 = true;
    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, interrupt(0, false)).Times(1);
    cpu.irq0 = false;
    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, interrupt(1, true)).Times(1);
    cpu.irq1 = true;
    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    EXPECT_CALL(cpu, interrupt(1, false)).Times(1);
    cpu.irq1 = false;
    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(1);
    sc_core::sc_start(quantum);

    vcml::irq_stats stats[2];
    cpu.get_irq_stats(0, stats[0]);
    cpu.get_irq_stats(1, stats[1]);
    EXPECT_EQ(stats[0].irq_count, 1);
    EXPECT_EQ(stats[1].irq_count, 1);
    EXPECT_EQ(stats[0].irq_uptime, quantum);
    EXPECT_EQ(stats[1].irq_uptime, quantum);
    EXPECT_FALSE(stats[0].irq_status);
    EXPECT_FALSE(stats[1].irq_status);

    // test processor::reset
    EXPECT_CALL(cpu, reset()).Times(1);
    EXPECT_CALL(cpu, simulate2(_)).Times(0);
    cpu.rst_out = true;
    sc_core::sc_start(10 * quantum);
    cpu.rst_out = false;

    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(10);
    sc_core::sc_start(10 * quantum);

    // test processor::handle_clock_update
    EXPECT_CALL(cpu, simulate2(_)).Times(0);
    EXPECT_CALL(cpu, handle_clock_update(DEFCLK, 0)).Times(1);
    cpu.clk_out = 0;
    sc_core::sc_start(10 * quantum);

    EXPECT_CALL(cpu, simulate2(quantum / cycle)).Times(10);
    EXPECT_CALL(cpu, handle_clock_update(0, DEFCLK)).Times(1);
    cpu.clk_out = DEFCLK;
    sc_core::sc_start(10 * quantum);
}
