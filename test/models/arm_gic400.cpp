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
        const u64 GICC_IIDR = 0xfc;             // CPU Interface Identification Register
        const u64 GICC_CTLR = 0x00;             // CPU Interface Control Register
        const u64 GICC_PMR = 0x04;              // Interrupt Priority Mask Register of CPU interface
        const u64 GICC_IAR = 0x0C;              // Interrupt Acknowledge Register of CPU interface
        const u64 GICC_EOIR = 0x10;             // End of Interrupt Register of CPU interface
        const u64 GICC_RPR = 0x14;              // Running Priority Register

        const u64 GICD_CTLR = 0x000;            // Distributor Control Register
        const u64 GICD_ISENABLER_SPI = 0x104;   // Interrupt Set-Enable Registers of Distributor
        const u64 GICD_ITARGETS_SPI = 0x820;    // Interrupt Processor Targets Registers of Distributor
        const u64 GICD_IPRIORITY_SGI = 0x400;   // Interrupt Priority Register for SGIs
        const u64 GICD_IPRIORITY_SPI = 0x420;   // Interrupt Priority Register for SPIs

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

        /**********************************************************************************************
         *                                                                                            *
         *       Software Peripheral Interrupt (SPI) Test - Interrupt triggered by Peripheral 1       *
         *                                                                                            *
         **********************************************************************************************/

        //Write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0))) << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val)) << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val)) << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val)) << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        //Read and Write ISENABLER_SPI, ITARGTES_SPI Register of DISTIF and PMR Register of CPU0
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val)) << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI+0x01, val)) << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(DISTIF_OUT.readw(GICD_ITARGETS_SPI+0x01,val)) << "failed to read ITARGETS_SPI register of distributor";
        EXPECT_EQ(val, 0x01) <<"writing to ITARGETS_SPI not successful";
        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0))) << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(CPUIF_OUT.readw(GICC_PMR, val, SBI_CPUID(0))) << "failed to read GICC_PMR for cpu0";
        EXPECT_EQ(val, 0xF) <<"writing to GICC_PMR of cpu0 not successful";
        val = 0b00000010;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val)) << "failed to read GICD_ISENABLER_SPI";
        EXPECT_EQ(val, 0b00000010) <<"writing to GICD_ISENABLER_SPI not successful";
        wait(SC_ZERO_TIME);

        //setting SPI connection of processor 1 HIGH
        SPI_OUT[1].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(NIRQ_IN[1], 0) << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of it's CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_RPR, val, SBI_CPUID(0))) << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xFF) << "GICC_RPR should be 255 (idle priority) -> no handling of interrupt";
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 33) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_RPR, val, SBI_CPUID(0))) << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x0) << "GICC_RPR should be 1023 -> handling of interrupt of priority 0";

        //cpu1 get spurious interrupt ID (1023) if it reads IAR of it's CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(1))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023) << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        //cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x21;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0))) << "cpu0 failed to write in GICC_EOIR";
        val = 0x3FF;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val, SBI_CPUID(1))) << "writing spurious interrupt ID to GICC_EOIR should not be allowed";

        //setting SPI connection of processor 1 LOW
        SPI_OUT[1].write(false);
        wait(SC_ZERO_TIME);

        //reset registers
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI+0x01, val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)));

        /**********************************************************************************************
         *                                                                                            *
         *       Software Peripheral Interrupt (SPI) Test - Interrupt triggered by Peripheral 0       *
         *                                                                                            *
         **********************************************************************************************/

        //Read CPUIF and DISTIF CTLR register
        val = ~0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val, SBI_CPUID(0))) << "failed to read GICC_CTLR for cpu0";
        val = ~0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val, SBI_CPUID(0))) << "failed to read GICD_CTLR for cpu0";

        //Write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0))) << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val)) << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val)) << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val)) << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        //Read and Write ISENABLER_SPI Register of DISTIF
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val)) << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val)) << "failed to write ITARGETS_SPI Register of Distributor";
        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0))) << "failed to set Priority Mask GICC_PMR for cpu0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val)) << "failed to enable interrupt in GICD_ISENABLER_SPI";
        wait(SC_ZERO_TIME);

        //setting SPI connection of processor 0 HIGH
        SPI_OUT[0].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(NIRQ_IN[1], 0) << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of it's CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        //cpu1 get spurious interrupt ID (1023) if it reads IAR of it's CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(1))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023) << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        //cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0))) << "cpu0 failed to write in GICC_EOIR";
        val = 0x3FF;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val, SBI_CPUID(1))) << "writing spurious interrupt ID to GICC_EOIR should not be allowed";

        //setting SPI connection of processor 0 LOW
        SPI_OUT[0].write(false);
        wait(SC_ZERO_TIME);

        //reset registers
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)));

        /******************************************************************************************************
         *                                                                                                    *
         *       Software Peripheral Interrupt (SPI) Test - trigger IPRIORITY_SPI/SGI bug in gic400::update   *
         *                                                                                                    *
         ******************************************************************************************************/

        //Write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0))) << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val, SBI_CPUID(0))) << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val)) << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val)) << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        //Read and Write ISENABLER_SPI Register of DISTIF
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val)) << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val)) << "failed to write ITARGETS_SPI Register of Distributor";
        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0))) << "failed to set Priority Mask GICC_PMR for cpu0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val)) << "failed to enable interrupt in GICD_ISENABLER_SPI";
        wait(SC_ZERO_TIME);

        //Setting priority in IPRIORITY_SGI to maximum 0xF
        val = 0xF;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val)) << "failed to write GICD_IPRIORITY_SGI register";

        //setting SPI connection of processor 0 HIGH
        SPI_OUT[0].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0 -> check IPRIORITY_SPI-IPRIORITY_SGI bug";
        EXPECT_EQ(NIRQ_IN[1], 0) << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of it's CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        //cpu1 get spurious interrupt ID (1023) if it reads IAR of it's CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(1))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023) << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        //cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0))) << "cpu0 failed to write in GICC_EOIR";
        wait(SC_ZERO_TIME);
        val = 0x3FF;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val, SBI_CPUID(1))) << "writing spurious interrupt ID to GICC_EOIR should not be allowed";

        //setting SPI connection of processor 0 LOW
        SPI_OUT[0].write(false);
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val)) << "failed to write GICD_IPRIORITY_SGI register";
        wait(SC_ZERO_TIME);

        //reset registers
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)));

        /****************************************************************************************************
         *                                                                                                  *
         *       Software Peripheral Interrupt (SPI) Test - trigger bug in gic400::get_irq_priority         *
         *                                                                                                  *
         ****************************************************************************************************/

        //Setting priority in IPRIORITY_SPI to maximum 0xFF ans in IPRIORITY_SGI to 0x1
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SPI,val)) << "failed to write GICD_IPRIORITY_SPI register";
        val = 0xFF;  //idle priority
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val)) << "failed to write GICD_IPRIORITY_SGI register";

        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0))) << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val)) << "failed to write GICD_CTLR";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val)) << "failed to write ITARGETS_SPI Register of Distributor";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val)) << "failed to enable interrupt in GICD_ISENABLER_SPI";
        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0))) << "failed to set Priority Mask GICC_PMR for cpu0";
        wait(SC_ZERO_TIME);

        //setting SPI connection of processor 0 HIGH
        SPI_OUT[0].write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0";

        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0))) << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);
        val = 0x0;
        //trigger bug in gic400::get_irq_prioriy: lookup in wrong priority register (SGI instead of SPI)
        EXPECT_OK(CPUIF_OUT.readw(GICC_RPR, val, SBI_CPUID(0))) << "failed to read GICC_RPR of cpu0";
        EXPECT_EQ(val, 0x01)<< "running priority of cpu0 in GICC_RPR is wrong, check gic400::get_irq_priority";

        val = 0x20;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0))) << "cpu0 failed to write in GICC_EOIR";
        SPI_OUT[0].write(false);
        wait(SC_ZERO_TIME);

        //reset registers
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0))) << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val)) << "failed to write ITARGETS_SPI Register of Distributor";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val)) << "failed to enable interrupt in GICD_ISENABLER_SPI";
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0))) << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val)) << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val)) << "failed to write GICD_IPRIORITY_SGI register";
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SPI,val)) << "failed to write GICD_IPRIORITY_SPI register";
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
