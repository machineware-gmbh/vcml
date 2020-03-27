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

#include "testing.h"

class gic400_stim: public test_base
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


    gic400_stim(const sc_module_name& nm):
        test_base(nm),
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
    }

    virtual void run_test() override {
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

    gic400_stim stim("STIM");
    arm::gic400 gic400("GIC400");

    stim.CLOCK.stub(100 * MHz);
    stim.RESET.stub();

    gic400.CLOCK.stub(100 * MHz);
    gic400.RESET.stub();

    stim.DISTIF_OUT.bind(gic400.DISTIF.IN);
    stim.CPUIF_OUT.bind(gic400.CPUIF.IN);
    stim.VIFCTRL_OUT.bind(gic400.VIFCTRL.IN);
    stim.VCPUIF_OUT.bind(gic400.VCPUIF.IN);

    for (int cpu = 0; cpu < 2; cpu++) {
        gic400.FIQ_OUT[cpu].bind(firqs[cpu]);
        gic400.IRQ_OUT[cpu].bind(nirqs[cpu]);
        gic400.VFIQ_OUT[cpu].bind(vfirqs[cpu]);
        gic400.VIRQ_OUT[cpu].bind(vnirqs[cpu]);
        gic400.SPI_IN[cpu].bind(spis[cpu]);
        gic400.ppi_in(cpu, 0).bind(ppis[cpu]);

        stim.FIRQ_IN[cpu].bind(firqs[cpu]);
        stim.NIRQ_IN[cpu].bind(nirqs[cpu]);
        stim.VFIRQ_IN[cpu].bind(vfirqs[cpu]);
        stim.VNIRQ_IN[cpu].bind(vnirqs[cpu]);
        stim.SPI_OUT[cpu].bind(spis[cpu]);
        stim.PPI_OUT[cpu].bind(ppis[cpu]);
    }

    sc_core::sc_start();
}

