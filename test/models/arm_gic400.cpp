/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml.h"

using namespace ::testing;
using namespace ::sc_core;
using namespace ::vcml;

#define EXPECT_OK(tlmcall) EXPECT_EQ(tlmcall, tlm::TLM_OK_RESPONSE)
#define EXPECT_AE(tlmcall) EXPECT_EQ(tlmcall, tlm::TLM_ADDRESS_ERROR_RESPONSE)
#define EXPECT_CE(tlmcall) EXPECT_EQ(tlmcall, tlm::TLM_COMMAND_ERROR_RESPONSE)

class test_harness: public component
{
public:
    master_socket DISTIF_OUT;
    master_socket CPUIF_OUT;
    master_socket VIFCTRL_OUT;
    master_socket VCPUIF_OUT;

    sc_vector<sc_out<bool>> PPI_OUT;
    sc_vector<sc_out<bool>> SPI_OUT;

    sc_vector<sc_in<bool>> FIRQ_IN;
    sc_vector<sc_in<bool>> NIRQ_IN;

    sc_vector<sc_in<bool>> VFIRQ_IN;
    sc_vector<sc_in<bool>> VNIRQ_IN;

    SC_HAS_PROCESS(test_harness);

    test_harness(const sc_module_name& nm):
        component(nm),
        DISTIF_OUT("DISTIF_OUT"),
        CPUIF_OUT("CPUIF_OUT"),
        VIFCTRL_OUT("VCTRLIF_OUT"),
        VCPUIF_OUT("VCPUIF_OUT"),
        PPI_OUT("PPI_OUT", 2),
        SPI_OUT("SPI_OUT", 2),
        FIRQ_IN("FIRQ_IN", 2),
        NIRQ_IN("NIRQ_IN", 2),
        VFIRQ_IN("VFIRQ_IN", 2),
        VNIRQ_IN("VNIRQ_IN", 2) {
        SC_THREAD(run);
    }

    void run() {
        run_test();
        sc_stop();
    }

    void run_test() {
        const u64 GICC_IIDR = 0xfc;

        u32 val = ~0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IIDR, val, SBI_CPUID(0)))
            << "failed to read GICC_IIDR for cpu0";
        EXPECT_EQ(val, (u32)arm::gic400::IFID)
            << "received erroneous gic400 interface ID";

        val = ~0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IIDR, val, SBI_CPUID(1)))
            << "failed to read GICC_IIDR for cpu1";
        EXPECT_EQ(val, (u32)arm::gic400::IFID)
            << "received erroneous gic400 interface ID";

        val = ~0;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val))
            << "writing to GICC_IIDR should not be allowed";
    }

};

TEST(gic400, gic400) {
    sc_vector<sc_signal<bool>> firqs("FIRQ", 2);
    sc_vector<sc_signal<bool>> nirqs("NIRQ", 2);
    sc_vector<sc_signal<bool>> vfirqs("VFIRQ", 2);
    sc_vector<sc_signal<bool>> vnirqs("VNIRQ", 2);
    sc_vector<sc_signal<bool>> spis("SPI", 2);
    sc_vector<sc_signal<bool>> ppis("PPI", 2);

    test_harness harness("HARNESS");
    arm::gic400 gic400("GIC400");

    harness.CLOCK.stub(100 * MHz);
    harness.RESET.stub();

    gic400.CLOCK.stub(100 * MHz);
    gic400.RESET.stub();

    harness.DISTIF_OUT.bind(gic400.DISTIF.IN);
    harness.CPUIF_OUT.bind(gic400.CPUIF.IN);
    harness.VIFCTRL_OUT.bind(gic400.VIFCTRL.IN);
    harness.VCPUIF_OUT.bind(gic400.VCPUIF.IN);

    for (int cpu = 0; cpu < 2; cpu++) {
        gic400.FIQ_OUT[cpu].bind(firqs[cpu]);
        gic400.IRQ_OUT[cpu].bind(nirqs[cpu]);
        gic400.VFIQ_OUT[cpu].bind(vfirqs[cpu]);
        gic400.VIRQ_OUT[cpu].bind(vnirqs[cpu]);
        gic400.SPI_IN[cpu].bind(spis[cpu]);
        gic400.ppi_in(cpu, 0).bind(ppis[cpu]);

        harness.FIRQ_IN[cpu].bind(firqs[cpu]);
        harness.NIRQ_IN[cpu].bind(nirqs[cpu]);
        harness.VFIRQ_IN[cpu].bind(vfirqs[cpu]);
        harness.VNIRQ_IN[cpu].bind(vnirqs[cpu]);
        harness.SPI_OUT[cpu].bind(spis[cpu]);
        harness.PPI_OUT[cpu].bind(ppis[cpu]);
    }

    sc_start();

    ASSERT_EQ(sc_get_status(), SC_STOPPED);
}

