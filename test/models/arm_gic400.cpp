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
    tlm_initiator_socket DISTIF_OUT;
    tlm_initiator_socket CPUIF_OUT;
    tlm_initiator_socket VIFCTRL_OUT;
    tlm_initiator_socket VCPUIF_OUT;

    sc_vector<irq_initiator_socket> PPI_OUT;
    sc_vector<irq_initiator_socket> SPI_OUT;

    sc_vector<irq_target_socket> FIRQ_IN;
    sc_vector<irq_target_socket> NIRQ_IN;

    sc_vector<irq_target_socket> VFIRQ_IN;
    sc_vector<irq_target_socket> VNIRQ_IN;

    gic400_stim(const sc_module_name& nm):
        test_base(nm),
        DISTIF_OUT("DISTIF_OUT"),
        CPUIF_OUT("CPUIF_OUT"),
        VIFCTRL_OUT("VCTRLIF_OUT"),
        VCPUIF_OUT("VCPUIF_OUT"),
        PPI_OUT("PPI_OUT", 2),
        SPI_OUT("SPI_OUT", 3),
        FIRQ_IN("FIRQ_IN", 2),
        NIRQ_IN("NIRQ_IN", 2),
        VFIRQ_IN("VFIRQ_IN", 2),
        VNIRQ_IN("VNIRQ_IN", 2) {
    }

    virtual void run_test() override {
        const u64 GICC_IIDR = 0xfc; // CPU Interface Identification Register
        const u64 GICC_CTLR = 0x00; // CPU Interface Control Register
        const u64 GICC_PMR = 0x04;  // Interrupt Priority Mask Register
        const u64 GICC_IAR = 0x0c;  // Interrupt Acknowledge Register
        const u64 GICC_EOIR = 0x10; // End of Interrupt Register
        const u64 GICC_RPR = 0x14;  // Running Priority Register
        const u64 GICC_HPPIR = 0x18;// Highest Pending IRQ register

        const u64 GICD_CTLR = 0x000;          // Distributor Control Register
        const u64 GICD_ISENABLER_SPI = 0x104; // Interrupt Set-Enable Registers
        const u64 GICD_ITARGETS_SPI = 0x820;  // Interrupt Target Registers
        const u64 GICD_IPRIORITY_SGI = 0x400; // SGI Priority Register
        const u64 GICD_IPRIORITY_SPI = 0x420; // SPI Priority Register

        const u32 GICV_CTLR = 0x00;     // VM Control Register
        const u32 GICV_PMR = 0x04;      // VM Priority Mask Register
        const u32 GICV_IAR = 0x0C;      // VM Interrupt Acknowledge Register
        const u32 GICV_EOIR = 0x10;     // VM End of Interrupt Register
        const u32 GICV_HPPIR = 0x18;    // VM Highest Priority Pending Interrupt Register

        const u32 GICH_HCR = 0x00;     // Hypervisor Control Register
        const u32 GICH_LR = 0x100;     // List Registers

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

        /**********************************************************************
         *                                                                    *
         * SPI Test - Interrupt triggered by Peripheral 1                     *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        // read and write ISENABLER_SPI, ITARGTES_SPI Register of DISTIF and
        // PMR Register of CPU0
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI+0x01, val))
            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(DISTIF_OUT.readw(GICD_ITARGETS_SPI+0x01,val))
            << "failed to read ITARGETS_SPI register of distributor";
        EXPECT_EQ(val, 0x01) << "writing to ITARGETS_SPI not successful";

        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(CPUIF_OUT.readw(GICC_PMR, val, SBI_CPUID(0)))
            << "failed to read GICC_PMR for cpu0";
        EXPECT_EQ(val, 0xF) <<"writing to GICC_PMR of cpu0 not successful";

        val = 0b00000010;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI";
        EXPECT_EQ(val, 0b00000010)
            <<"writing to GICD_ISENABLER_SPI not successful";
        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 1 HIGH
        SPI_OUT[1].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(NIRQ_IN[1], 0) << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_RPR, val, SBI_CPUID(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xFF)
            << "GICC_RPR should be 255 (idle priority)"
            << " -> no handling of interrupt";

        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 33) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_RPR, val, SBI_CPUID(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x0)
            << "GICC_RPR should be 1023"
            << " -> handling of interrupt of priority 0";

        // cpu1 get spurious interrupt ID (1023) if it reads IAR of its CPU
        // interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";
        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x21;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0)))
            << "cpu0 failed to write in GICC_EOIR";
        val = 0x3FF;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val, SBI_CPUID(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

        // setting SPI connection of processor 1 LOW
        SPI_OUT[1].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI+0x01, val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)));

        /**********************************************************************
         *                                                                    *
         * SPI Test - Interrupt triggered by Peripheral 0                     *
         *                                                                    *
         **********************************************************************/

        //Read CPUIF and DISTIF CTLR register
        val = ~0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to read GICC_CTLR for cpu0";

        val = ~0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val, SBI_CPUID(0)))
            << "failed to read GICD_CTLR for cpu0";

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        // read and write ISENABLER_SPI Register of DISTIF
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";
        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 0 HIGH
        SPI_OUT[0].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(NIRQ_IN[1], 0) << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        // cpu1 get spurious interrupt (1023) if it reads IAR of its CPUIF
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0)))
            << "cpu0 failed to write in GICC_EOIR";
        val = 0x3ff;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val, SBI_CPUID(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

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

        /**********************************************************************
         *                                                                    *
         * SPI Test - trigger IPRIORITY_SPI/SGI bug in gic400::update         *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(CPUIF_OUT.readw(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.readw(GICD_CTLR, val))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        // read and write ISENABLER_SPI Register of DISTIF
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";

        val = 0xf;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        wait(SC_ZERO_TIME);

        // setting priority in IPRIORITY_SGI to maximum 0xF
        val = 0xf;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val))
            << "failed to write GICD_IPRIORITY_SGI register";

        // setting SPI connection of processor 0 HIGH
        SPI_OUT[0].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0"
                                 << " -> check IPRIORITY_SPI-IPRIORITY_SGI bug";
        EXPECT_EQ(NIRQ_IN[1], 0) << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        // cpu1 get spurious interrupt if it reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0)))
            << "cpu0 failed to write in GICC_EOIR";
        wait(SC_ZERO_TIME);
        val = 0x3ff;
        EXPECT_CE(CPUIF_OUT.writew(GICC_IIDR, val, SBI_CPUID(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

        // setting SPI connection of processor 0 LOW
        SPI_OUT[0].write(false);
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val))
            << "failed to write GICD_IPRIORITY_SGI register";
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)));
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val));
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val));
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)));

        /**********************************************************************
         *                                                                    *
         * SPI Test - trigger bug in gic400::get_irq_priority                 *
         *                                                                    *
         **********************************************************************/

        // setting priority in IPRIORITY_SPI to maximum 0xff and in
        // IPRIORITY_SGI to 0x1
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SPI,val))
            << "failed to write GICD_IPRIORITY_SPI register";
        val = 0xff;  // idle priority
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val))
            << "failed to write GICD_IPRIORITY_SGI register";

        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";

        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        val = 0xf;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 0 HIGH
        SPI_OUT[0].write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled to cpu0";

        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);
        val = 0x0;

        // trigger bug in gic400::get_irq_prioriy: lookup in wrong priority
        // register (SGI instead of SPI)
        EXPECT_OK(CPUIF_OUT.readw(GICC_RPR, val, SBI_CPUID(0)))
            << "failed to read GICC_RPR of cpu0";
        EXPECT_EQ(val, 0x01)
            << "running priority of cpu0 in GICC_RPR is wrong, "
            << "check gic400::get_irq_priority";

        val = 0x20;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0)))
            << "cpu0 failed to write in GICC_EOIR";
        SPI_OUT[0].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SGI,val))
            << "failed to write GICD_IPRIORITY_SGI register";
        EXPECT_OK(DISTIF_OUT.writew(GICD_IPRIORITY_SPI,val))
            << "failed to write GICD_IPRIORITY_SPI register";

        /**********************************************************************
         *                                                                    *
         *              Virtual Interrupt Test                                *
         *                                                                    *
         **********************************************************************/

        // allow forwarding virtual interrupts
        val = 0x01;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_CTLR, val, SBI_CPUID(0)))
                            << "failed to write GICV_CTLR in VCPUIF";
        EXPECT_OK(VCPUIF_OUT.readw(GICV_CTLR, val))
                            << "failed to read GICV_CTLR from VCPUIF";
        EXPECT_EQ(val, 0x01) << "GICV_CTLR of VCPUIF should be 1";
        val = 0x01;
        EXPECT_OK(VIFCTRL_OUT.writew(GICH_HCR, val, SBI_CPUID(0)))
                            << "failed to write GICH_HCR";
        EXPECT_OK(VIFCTRL_OUT.readw(GICH_HCR, val))
                            << "failed to read GICH_HCR from VIFCTRL";
        EXPECT_EQ(val, 0x01) << "GICH_HCR of VIFCTRL should be 1";

        // set GICV_PMR register
        val = 0b11110000;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_PMR, val))
                            << "failed to write GICV_PMR register";
        // set Virtual Interrupt (HW=0, prio=20, virtID=27) pending (01) in List Register of VIFCTRL
        val = 0b00011010000000000000000000011011;
        EXPECT_OK(VIFCTRL_OUT.writew(GICH_LR, val))
                            << "failed to write GICH_LR register";
        // read GICV_HPPIR register of VCPUIF
        val = 0x00;
        EXPECT_OK(VCPUIF_OUT.readw(GICV_HPPIR, val))
                            << "failed to read GICV_HPPIR register of VCPUIF";
        EXPECT_EQ(val, 0b00011011)
                            << "GICV_HPPIR of VCPUIF should be 0b00011011=27";

        // check if vIRQ is signaled to GuestOS
        wait(SC_ZERO_TIME);
        EXPECT_EQ(VNIRQ_IN[0], 1)
                            << "vIRQ should have been signaled to GuestOS";

        // GuestOS reads IAR of its VCPUIF
        val = 0x00;
        EXPECT_OK(VCPUIF_OUT.readw(GICV_IAR, val))
                            << "failed to read GICV_IAR register of VCPUIF";
        EXPECT_EQ(val, 0b00011011)
                            << "GICV_IAR of VCPUIF should be 0b00011011=27";
        EXPECT_EQ(VNIRQ_IN[0], 0)
                            << "vIRQ should be 0 after GuestOS has read IAR";
        // GuestOS handles interrupt and writes to EOIR of its VCPUIF
        val = 27;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_EOIR, val))
                            << "failed to write GICV_EOIR register";

        // reset registers
        val = 0x0;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_CTLR, val, SBI_CPUID(0)))
                            << "failed to write GICV_CTLR register of VCPUIF";
        EXPECT_OK(VIFCTRL_OUT.writew(GICH_HCR, val))
                            << "failed to write GICH_HCR register of VIFCTRL";
        EXPECT_OK(VCPUIF_OUT.writew(GICV_PMR, val))
                            << "failed to write GICV_PMR register of VCPUIF";


        /**********************************************************************
         *                                                                    *
         *           Virtual Interrupt Test - Hardware interrupt              *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
                            << "failed to set GICC_CTLR for cpu0 HIGH";
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
                            << "failed to write GICD_CTLR";

        // write GICD_ITARGETS_SPI (irq 42 targets cpu0), GICC_PMR and GICD_ISENABLER_SPI (enable irq 42)
        val = 0x1;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI+0xA, val))
                            << "failed to write ITARGETS_SPI register of distributor";
        val = 0xF;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
                            << "failed to set Priority Mask GICC_PMR for cpu0";
        val = 1<<10;
        EXPECT_OK(DISTIF_OUT.writew(GICD_ISENABLER_SPI, val));
        wait(SC_ZERO_TIME);

        // allow forwarding virtual interrupts (CTLR), enable Virtual CPU interface operation (HCR)
        val = 0x01;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_CTLR, val, SBI_CPUID(0)))
                            << "failed to write GICV_CTLR in VCPUIF";
        val = 0x01;
        EXPECT_OK(VIFCTRL_OUT.writew(GICH_HCR, val, SBI_CPUID(0)))
                            << "failed to write GICH_HCR";
        // set GICV_PMR register
        val = 0b11110000;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_PMR, val))
                            << "failed to write GICV_PMR register";

        // setting peripheral connection high (connected to irq 42)
        SPI_OUT[2].write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(NIRQ_IN[0], 1) << "IRQ should have been signaled";

        // check HPPIR register in cpuif
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_HPPIR, val))
                            << "failed to read GICC_HPPIR register of CPUIF";
        EXPECT_EQ(val, 42)
                            << "GICC_HPPIR of CPUIF should be 42";

        // hypervisor acks and eois interrupt 42 in cpuif
        val = 0x00;
        EXPECT_OK(CPUIF_OUT.readw(GICC_IAR, val, SBI_CPUID(0)))
                            << "failed to read GICC_IAR register of CPUIF";
        EXPECT_EQ(val, 42)
                            << "GICC_IAR of VCPUIF should be 42";
        val = 42;
        EXPECT_OK(CPUIF_OUT.writew(GICC_EOIR, val, SBI_CPUID(0)))
                            << "cpu0 failed to write in GICC_EOIR";
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.readw(GICC_HPPIR, val))
                            << "failed to read GICC_HPPIR register of CPUIF";
        EXPECT_EQ(val, 1023)
                            << "GICC_HPPIR of CPUIF should be 1023";

        // hypervisor updates list registers: add pending physical hardware interrupt with virtualID 42
        val =     ( 1 << 31) // HW = 1 hardware interrupt
                | ( 0 << 30) // group0 interrupt
                | ( 1 << 28) // state: pending
                | (20 << 23) // priority = 20
                | (42 << 10) // phys id
                |  42;       // virt id
        EXPECT_OK(VIFCTRL_OUT.writew(GICH_LR, val))
                            << "failed to write GICH_LR register";

        // check GICV_HPPIR register of VCPUIF
        val = 0x0;
        EXPECT_OK(VCPUIF_OUT.readw(GICV_HPPIR, val))
                            << "failed to read GICV_HPPIR register of VCPUIF";
        EXPECT_EQ(val, 42)
                            << "GICV_HPPIR of VCPUIF should be 42";

        // check if vIRQ is signaled to GuestOS
        wait(SC_ZERO_TIME);
        EXPECT_EQ(VNIRQ_IN[0], 1)
                            << "vIRQ should have been signaled to GuestOS";

        // GuestOS reads IAR of its VCPUIF and handles interrupt
        val = 0x00;
        EXPECT_OK(VCPUIF_OUT.readw(GICV_IAR, val))
                            << "failed to read GICV_IAR register of VCPUIF";
        EXPECT_EQ(val, 42)
                            << "GICV_IAR of VCPUIF should be 42";
        EXPECT_EQ(VNIRQ_IN[0], 0)
                            << "vIRQ should be 0 after GuestOS has read IAR";

        // deactivate interrupt
        val = 42;
        EXPECT_OK(VCPUIF_OUT.writew(GICV_EOIR, val))
                            << "failed to write GICV_EOIR register";

        // check GICV_HPPIR register of VCPUIF
        val = 0x0;
        EXPECT_OK(VCPUIF_OUT.readw(GICV_HPPIR, val))
                            << "failed to read GICV_HPPIR register of VCPUIF";
        EXPECT_EQ(val, 1023)
                            << "GICV_HPPIR of VCPUIF should be 1023";

        // reset registers
        val = 0x0;
        EXPECT_OK(CPUIF_OUT.writew(GICC_PMR, val, SBI_CPUID(0)))
                            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(DISTIF_OUT.writew(GICD_ITARGETS_SPI+0xA, val))
                            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(DISTIF_OUT.writew(GICD_CTLR, val))
                            << "failed to write GICD_CTLR";
        EXPECT_OK(CPUIF_OUT.writew(GICC_CTLR, val, SBI_CPUID(0)))
                            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(VCPUIF_OUT.writew(GICV_CTLR, val, SBI_CPUID(0)))
                            << "failed to write GICV_CTLR in VCPUIF";
        EXPECT_OK(VIFCTRL_OUT.writew(GICH_HCR, val, SBI_CPUID(0)))
                            << "failed to write GICH_HCR";
        EXPECT_OK(VCPUIF_OUT.writew(GICV_PMR, val))
                            << "failed to write GICV_PMR register";
    }
};

TEST(gic400, gic400) {
    sc_vector<sc_signal<bool>> firqs("FIRQ", 2);
    sc_vector<sc_signal<bool>> nirqs("NIRQ", 2);
    sc_vector<sc_signal<bool>> vfirqs("VFIRQ", 2);
    sc_vector<sc_signal<bool>> vnirqs("VNIRQ", 2);
    sc_vector<sc_signal<bool>> spis("SPI", 3);
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
        gic400.FIQ_OUT[cpu].bind(stim.FIRQ_IN[cpu]);
        gic400.IRQ_OUT[cpu].bind(stim.NIRQ_IN[cpu]);
        gic400.VFIQ_OUT[cpu].bind(stim.VFIRQ_IN[cpu]);
        gic400.VIRQ_OUT[cpu].bind(stim.VNIRQ_IN[cpu]);
        stim.SPI_OUT[cpu].bind(gic400.SPI_IN[cpu]);
        stim.PPI_OUT[cpu].bind(gic400.ppi_in(cpu, 0));

    }

    stim.SPI_OUT[2].bind(gic400.SPI_IN[10]);

    sc_core::sc_start();
}
