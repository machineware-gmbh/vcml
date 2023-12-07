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

class gic400_stim : public test_base
{
public:
    tlm_initiator_socket distif_out;
    tlm_initiator_socket cpuif_out;
    tlm_initiator_socket vifctrl_out;
    tlm_initiator_socket vcpuif_out;

    sc_vector<gpio_initiator_socket> ppi_out;
    sc_vector<gpio_initiator_socket> spi_out;

    sc_vector<gpio_target_socket> nfirq_in;
    sc_vector<gpio_target_socket> nirq_in;

    sc_vector<gpio_target_socket> vfirq_in;
    sc_vector<gpio_target_socket> vnirq_in;

    gic400_stim(const sc_module_name& nm):
        test_base(nm),
        distif_out("distif_out"),
        cpuif_out("cpuif_out"),
        vifctrl_out("vifctrl_out"),
        vcpuif_out("vcpuif_out"),
        ppi_out("ppi_out", 2),
        spi_out("spi_out", 3),
        nfirq_in("nfirq_in", 2),
        nirq_in("nirq_in", 2),
        vfirq_in("vfirq_in", 2),
        vnirq_in("vnirq_in", 2) {}

    virtual void run_test() override {
        enum addresses : u64 {
            GICC_IIDR = 0xfc,   // CPU Interface Identification
            GICC_CTLR = 0x00,   // CPU Interface Control Register
            GICC_PMR = 0x04,    // Interrupt Priority Mask Register
            GICC_BPR = 0x08,    // Binary Point Register
            GICC_IAR = 0x0c,    // Interrupt Acknowledge Register
            GICC_EOIR = 0x10,   // End of Interrupt Register
            GICC_RPR = 0x14,    // Running Priority Register
            GICC_HPPIR = 0x18,  // Highest Pending IRQ register
            GICC_AIAR = 0x20,   // Alias Interrupt Acknowledge Register
            GICC_AEOIR = 0x24,  // Alias End of Interrupt Register
            GICC_AHPPIR = 0x28, // Alias Highest Pending IRQ register

            GICD_CTLR = 0x000,          // Distributor Control Register
            GICD_IGROUPR_SPI = 0x084,   // Interrupt Group Register
            GICD_ISENABLER_SPI = 0x104, // Interrupt Set-Enable Registers
            GICD_ICENABLER_SPI = 0x184, // Interrupt Clear-Enable Registers
            GICD_ITARGETS_SPI = 0x820,  // Interrupt Target Registers
            GICD_IPRIORITY_SGI = 0x400, // SGI Priority Register
            GICD_IPRIORITY_SPI = 0x420, // SPI Priority Register
            GICD_ICFGR_SPI = 0xc08,     // SPI Configuration Register

            GICV_CTLR = 0x00,  // VM Control Register
            GICV_PMR = 0x04,   // VM Priority Mask Register
            GICV_IAR = 0x0c,   // VM Interrupt Acknowledge Register
            GICV_EOIR = 0x10,  // VM End of Interrupt Register
            GICV_HPPIR = 0x18, // VM Highest Priority Pending IRQ

            GICH_HCR = 0x00, // Hypervisor Control Register
            GICH_LR = 0x100, // List Registers
        };

        u32 val = ~0;
        EXPECT_OK(cpuif_out.readw(GICC_IIDR, val, sbi_cpuid(0)))
            << "failed to read GICC_IIDR for cpu0";
        EXPECT_EQ(val, (u32)arm::gic400::AMBA_IFID)
            << "received erroneous gic400 interface ID";

        val = ~0;
        EXPECT_OK(cpuif_out.readw(GICC_IIDR, val, sbi_cpuid(1)))
            << "failed to read GICC_IIDR for cpu1";
        EXPECT_EQ(val, (u32)arm::gic400::AMBA_IFID)
            << "received erroneous gic400 interface ID";

        val = ~0;
        EXPECT_CE(cpuif_out.writew(GICC_IIDR, val))
            << "writing to GICC_IIDR should not be allowed";

        /**********************************************************************
         *                                                                    *
         * SPI Test - Interrupt triggered by Peripheral 1                     *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(cpuif_out.readw(GICC_CTLR, val))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";

        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(distif_out.readw(GICD_CTLR, val))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        // read and write ISENABLER_SPI, ITARGTES_SPI Register of DISTIF and
        // PMR Register of CPU0
        val = 0x0;
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0x01, val))
            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(distif_out.readw(GICD_ITARGETS_SPI + 0x01, val))
            << "failed to read ITARGETS_SPI register of distributor";
        EXPECT_EQ(val, 0x01) << "writing to ITARGETS_SPI not successful";

        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(cpuif_out.readw(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to read GICC_PMR for cpu0";
        EXPECT_EQ(val, 0xf) << "writing to GICC_PMR of cpu0 not successful";

        val = 0b00000010;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI";
        EXPECT_EQ(val, 0b00000010)
            << "writing to GICD_ISENABLER_SPI not successful";
        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 1 HIGH
        spi_out[1].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xfF) << "GICC_RPR should be 255 (idle priority)"
                             << " -> no handling of interrupt";

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 33) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x0) << "GICC_RPR should be 1023"
                            << " -> handling of interrupt of priority 0";

        // cpu1 get spurious interrupt ID (1023) if it reads IAR of its CPU
        // interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";
        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x21;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        val = 0x3FF;
        EXPECT_CE(cpuif_out.writew(GICC_IIDR, val, sbi_cpuid(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

        // setting SPI connection of processor 1 LOW
        spi_out[1].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0x01, val));
        EXPECT_OK(distif_out.writew(GICD_CTLR, val));
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)));

        /**********************************************************************
         *                                                                    *
         * SPI Test - Interrupt triggered by Peripheral 0                     *
         *                                                                    *
         **********************************************************************/

        // Read CPUIF and DISTIF CTLR register
        val = ~0;
        EXPECT_OK(cpuif_out.readw(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICC_CTLR for cpu0";

        val = ~0;
        EXPECT_OK(distif_out.readw(GICD_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICD_CTLR for cpu0";

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(cpuif_out.readw(GICC_CTLR, val))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";

        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(distif_out.readw(GICD_CTLR, val))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        // read and write ISENABLER_SPI Register of DISTIF
        val = 0x0;
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";
        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 0 HIGH
        spi_out[0].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        // cpu1 get spurious interrupt (1023) if it reads IAR of its CPUIF
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        val = 0x3ff;
        EXPECT_CE(cpuif_out.writew(GICC_IIDR, val, sbi_cpuid(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

        // setting SPI connection of processor 0 LOW
        spi_out[0].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val));
        EXPECT_OK(distif_out.writew(GICD_CTLR, val));
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)));

        /**********************************************************************
         *                                                                    *
         *        Trigger Bug with Re-raising Level Triggered SPIs            *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, 0x1, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_CTLR, 0x1));

        // read and write ISENABLER_SPI Register of DISTIF
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, 0x1));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, 0xf, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, 0x1));
        EXPECT_OK(distif_out.writew(GICD_ICFGR_SPI, 0xaaaaaaa8));

        // setting SPI connection of processor 0 HIGH
        spi_out[0].write(true);

        EXPECT_EQ(nirq_in[0], 1) << "IRQ should have been signaled to cpu0";
        EXPECT_EQ(nirq_in[1], 0) << "IRQ shouldn't have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)));
        EXPECT_EQ(val, 0x20);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        val = 0x20; // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)));

        // since SPI is still pending, irq should also still be raised
        EXPECT_EQ(nirq_in[0], 1) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 reads IAR of its CPU interface (again)
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)));
        EXPECT_EQ(val, 0x20);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // now lower the SPI
        spi_out[0].write(false);

        val = 0x20; // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)));
        EXPECT_FALSE(nirq_in[0]);

        // reset registers
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val));
        EXPECT_OK(distif_out.writew(GICD_CTLR, val));
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ICFGR_SPI, 0xaaaaaaaa));

        /**********************************************************************
         *                                                                    *
         * SPI Test - trigger IPRIORITY_SPI/SGI bug in gic400::update         *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(cpuif_out.readw(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x1) << "GICC_CTLR of CPUIF should be 1";

        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(distif_out.readw(GICD_CTLR, val))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x1) << "GICD_CTLR of DISTIF should be 1";

        // read and write ISENABLER_SPI Register of DISTIF
        val = 0x0;
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";

        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";

        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";

        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        wait(SC_ZERO_TIME);

        // setting priority in IPRIORITY_SGI to maximum 0xf
        val = 0xf;
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SGI, val))
            << "failed to write GICD_IPRIORITY_SGI register";

        // setting SPI connection of processor 0 HIGH
        spi_out[0].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 1)
            << "IRQ should have been signaled to cpu0"
            << " -> check IPRIORITY_SPI-IPRIORITY_SGI bug";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        // cpu1 get spurious interrupt if it reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";

        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        wait(SC_ZERO_TIME);
        val = 0x3ff;
        EXPECT_CE(cpuif_out.writew(GICC_IIDR, val, sbi_cpuid(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

        // setting SPI connection of processor 0 LOW
        spi_out[0].write(false);
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SGI, val))
            << "failed to write GICD_IPRIORITY_SGI register";
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SGI, val));
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val));
        EXPECT_OK(distif_out.writew(GICD_CTLR, val));
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)));

        /**********************************************************************
         *                                                                    *
         * SPI Test - trigger bug in gic400::get_irq_priority                 *
         *                                                                    *
         **********************************************************************/

        // setting priority in IPRIORITY_SPI to maximum 0xff and in
        // IPRIORITY_SGI to 0x1
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SPI, val))
            << "failed to write GICD_IPRIORITY_SPI register";
        val = 0xff; // idle priority
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SGI, val))
            << "failed to write GICD_IPRIORITY_SGI register";

        val = 0x1;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";

        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 0 HIGH
        spi_out[0].write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 1) << "IRQ should have been signaled to cpu0";

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);
        val = 0x0;

        // trigger bug in gic400::get_irq_prioriy: lookup in wrong priority
        // register (SGI instead of SPI)
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR of cpu0";
        EXPECT_EQ(val, 0x01)
            << "running priority of cpu0 in GICC_RPR is wrong, "
            << "check gic400::get_irq_priority";

        val = 0x20;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        spi_out[0].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI Register of Distributor";
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val))
            << "failed to enable interrupt in GICD_ISENABLER_SPI";
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SGI, val))
            << "failed to write GICD_IPRIORITY_SGI register";
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SPI, val))
            << "failed to write GICD_IPRIORITY_SPI register";

        /**********************************************************************
         *                                                                    *
         *              Virtual Interrupt Test                                *
         *                                                                    *
         **********************************************************************/

        // allow forwarding virtual interrupts
        val = 0x01;
        EXPECT_OK(vcpuif_out.writew(GICV_CTLR, val, sbi_cpuid(0)))
            << "failed to write GICV_CTLR in VCPUIF";
        EXPECT_OK(vcpuif_out.readw(GICV_CTLR, val))
            << "failed to read GICV_CTLR from VCPUIF";
        EXPECT_EQ(val, 0x01) << "GICV_CTLR of VCPUIF should be 1";
        val = 0x01;
        EXPECT_OK(vifctrl_out.writew(GICH_HCR, val, sbi_cpuid(0)))
            << "failed to write GICH_HCR";
        EXPECT_OK(vifctrl_out.readw(GICH_HCR, val))
            << "failed to read GICH_HCR from VIFCTRL";
        EXPECT_EQ(val, 0x01) << "GICH_HCR of VIFCTRL should be 1";

        // set GICV_PMR register
        val = 0b11110000;
        EXPECT_OK(vcpuif_out.writew(GICV_PMR, val))
            << "failed to write GICV_PMR register";
        // set Virtual Interrupt (HW=0, prio=20, virtID=27) pending (01)
        // in List Register of VIFCTRL
        val = 0b00011010000000000000000000011011;
        EXPECT_OK(vifctrl_out.writew(GICH_LR, val))
            << "failed to write GICH_LR register";
        // read GICV_HPPIR register of VCPUIF
        val = 0x00;
        EXPECT_OK(vcpuif_out.readw(GICV_HPPIR, val))
            << "failed to read GICV_HPPIR register of VCPUIF";
        EXPECT_EQ(val, 0b00011011)
            << "GICV_HPPIR of VCPUIF should be 0b00011011=27";

        // check if vIRQ is signaled to GuestOS
        wait(SC_ZERO_TIME);
        EXPECT_EQ(vnirq_in[0], 1)
            << "vIRQ should have been signaled to GuestOS";

        // GuestOS reads IAR of its VCPUIF
        val = 0x00;
        EXPECT_OK(vcpuif_out.readw(GICV_IAR, val))
            << "failed to read GICV_IAR register of VCPUIF";
        EXPECT_EQ(val, 0b00011011)
            << "GICV_IAR of VCPUIF should be 0b00011011=27";
        EXPECT_EQ(vnirq_in[0], 0)
            << "vIRQ should be 0 after GuestOS has read IAR";
        // GuestOS handles interrupt and writes to EOIR of its VCPUIF
        val = 27;
        EXPECT_OK(vcpuif_out.writew(GICV_EOIR, val))
            << "failed to write GICV_EOIR register";

        // reset registers
        val = 0x0;
        EXPECT_OK(vcpuif_out.writew(GICV_CTLR, val, sbi_cpuid(0)))
            << "failed to write GICV_CTLR register of VCPUIF";
        EXPECT_OK(vifctrl_out.writew(GICH_HCR, val))
            << "failed to write GICH_HCR register of VIFCTRL";
        EXPECT_OK(vcpuif_out.writew(GICV_PMR, val))
            << "failed to write GICV_PMR register of VCPUIF";

        /**********************************************************************
         *                                                                    *
         *           Virtual Interrupt Test - Hardware interrupt              *
         *                                                                    *
         **********************************************************************/

        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x1;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";

        // write GICD_ITARGETS_SPI (irq 42 targets cpu0), GICC_PMR and
        // GICD_ISENABLER_SPI (enable irq 42)
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0xa, val))
            << "failed to write ITARGETS_SPI register of distributor";
        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        val = 1 << 10;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        wait(SC_ZERO_TIME);

        // allow forwarding virtual interrupts (CTLR), enable Virtual CPU
        // interface operation (HCR)
        val = 0x01;
        EXPECT_OK(vcpuif_out.writew(GICV_CTLR, val, sbi_cpuid(0)))
            << "failed to write GICV_CTLR in VCPUIF";
        val = 0x01;
        EXPECT_OK(vifctrl_out.writew(GICH_HCR, val, sbi_cpuid(0)))
            << "failed to write GICH_HCR";
        // set GICV_PMR register
        val = 0b11110000;
        EXPECT_OK(vcpuif_out.writew(GICV_PMR, val))
            << "failed to write GICV_PMR register";

        // setting peripheral connection high (connected to irq 42)
        spi_out[2].write(true);
        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 1) << "IRQ should have been signaled";

        // check HPPIR register in cpuif
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_HPPIR, val))
            << "failed to read GICC_HPPIR register of CPUIF";
        EXPECT_EQ(val, 42) << "GICC_HPPIR of CPUIF should be 42";

        // hypervisor acks and eois interrupt 42 in cpuif
        val = 0x00;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR register of CPUIF";
        EXPECT_EQ(val, 42) << "GICC_IAR of VCPUIF should be 42";
        val = 42;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_HPPIR, val))
            << "failed to read GICC_HPPIR register of CPUIF";
        EXPECT_EQ(val, 1023) << "GICC_HPPIR of CPUIF should be 1023";

        // hypervisor updates list registers: add pending physical hardware
        // interrupt with virtualID 42
        val = (1 << 31)    // HW = 1 hardware interrupt
              | (0 << 30)  // group0 interrupt
              | (1 << 28)  // state: pending
              | (20 << 23) // priority = 20
              | (42 << 10) // phys id
              | 42;        // virt id
        EXPECT_OK(vifctrl_out.writew(GICH_LR, val))
            << "failed to write GICH_LR register";

        // check GICV_HPPIR register of VCPUIF
        val = 0x0;
        EXPECT_OK(vcpuif_out.readw(GICV_HPPIR, val))
            << "failed to read GICV_HPPIR register of VCPUIF";
        EXPECT_EQ(val, 42) << "GICV_HPPIR of VCPUIF should be 42";

        // check if vIRQ is signaled to GuestOS
        wait(SC_ZERO_TIME);
        EXPECT_EQ(vnirq_in[0], 1)
            << "vIRQ should have been signaled to GuestOS";

        // GuestOS reads IAR of its VCPUIF and handles interrupt
        val = 0x00;
        EXPECT_OK(vcpuif_out.readw(GICV_IAR, val))
            << "failed to read GICV_IAR register of VCPUIF";
        EXPECT_EQ(val, 42) << "GICV_IAR of VCPUIF should be 42";
        EXPECT_EQ(vnirq_in[0], 0)
            << "vIRQ should be 0 after GuestOS has read IAR";

        // deactivate interrupt
        val = 42;
        EXPECT_OK(vcpuif_out.writew(GICV_EOIR, val))
            << "failed to write GICV_EOIR register";

        // check GICV_HPPIR register of VCPUIF
        val = 0x0;
        EXPECT_OK(vcpuif_out.readw(GICV_HPPIR, val))
            << "failed to read GICV_HPPIR register of VCPUIF";
        EXPECT_EQ(val, 1023) << "GICV_HPPIR of VCPUIF should be 1023";

        spi_out[2].write(false);

        // reset registers
        val = 0x0;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0xa, 1))
            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(distif_out.writew(GICD_CTLR, val))
            << "failed to write GICD_CTLR";
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(vcpuif_out.writew(GICV_CTLR, val, sbi_cpuid(0)))
            << "failed to write GICV_CTLR in VCPUIF";
        EXPECT_OK(vifctrl_out.writew(GICH_HCR, val, sbi_cpuid(0)))
            << "failed to write GICH_HCR";
        EXPECT_OK(vcpuif_out.writew(GICV_PMR, val))
            << "failed to write GICV_PMR register";

        /**********************************************************************
         *                                                                    *
         *              Interrupt Grouping Test                               *
         *                                                                    *
         **********************************************************************/

        // Testing Interrupt in Group 1
        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0x3;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(cpuif_out.readw(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0x3) << "GICC_CTLR of CPUIF should be 3";

        val = 0x3;
        EXPECT_OK(distif_out.writew(GICD_CTLR, val, sbi_cpuid(0)))
            << "failed to write GICD_CTLR";
        EXPECT_OK(distif_out.readw(GICD_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x3) << "GICD_CTLR of DISTIF should be 3";

        // read and write ISENABLER_SPI, ITARGTES_SPI Register of DISTIF and
        // PMR Register of CPU0
        val = 0x0;
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI for peripheral0";
        val = 0x1;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0x01, val))
            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(distif_out.readw(GICD_ITARGETS_SPI + 0x01, val))
            << "failed to read ITARGETS_SPI register of distributor";
        EXPECT_EQ(val, 0x01) << "writing to ITARGETS_SPI not successful";

        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(cpuif_out.readw(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to read GICC_PMR for cpu0";
        EXPECT_EQ(val, 0xf) << "writing to GICC_PMR of cpu0 not successful";

        val = 0b00000010;
        EXPECT_OK(distif_out.writew(GICD_ICENABLER_SPI, -1));
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI";
        EXPECT_EQ(val, 0b00000010)
            << "writing to GICD_ISENABLER_SPI not successful";

        val = 0b00000010;
        EXPECT_OK(distif_out.writew(GICD_IGROUPR_SPI, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.readw(GICD_IGROUPR_SPI, val, sbi_cpuid(0)))
            << "failed to read GICD_IGROUPR";
        EXPECT_EQ(val, 0b00000010) << "writing to GICD_IGROUPR not successful";

        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 1 HIGH
        spi_out[1].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 1)
            << "IRQ should have been signaled to irq on cpu0";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to irq on cpu1";
        EXPECT_EQ(nfirq_in[0], 0)
            << "IRQ should not have been signaled to firq on cpu0";
        EXPECT_EQ(nfirq_in[1], 0)
            << "IRQ should not have been signaled to firq on cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xff) << "GICC_RPR should be 255 (idle priority)"
                             << " -> no handling of interrupt";

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_AIAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 33) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x0) << "GICC_RPR should be 1023"
                            << " -> handling of interrupt of priority 0";

        // cpu1 get spurious interrupt ID (1023) if it reads IAR of its CPU
        // interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_AIAR, val, sbi_cpuid(1)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 1023)
            << "expected to read spurious interrupt ID from GICC_IAR";
        wait(SC_ZERO_TIME);
        EXPECT_EQ(nirq_in[0], 0) << "IRQ should be 0 after reading GICC_IAR";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x21;
        EXPECT_OK(cpuif_out.writew(GICC_AEOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        val = 0x3ff;
        EXPECT_CE(cpuif_out.writew(GICC_IIDR, val, sbi_cpuid(1)))
            << "writing spurious interrupt ID to GICC_EOIR should"
            << " not be allowed";

        // setting SPI connection of processor 1 LOW
        spi_out[1].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0x01, val));
        EXPECT_OK(distif_out.writew(GICD_CTLR, val));
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)));

        // Testing interrupts in different groups
        // write CPUIF0 and DISTIF CTLR register -> allow forwarding for CPU0
        val = 0xb;
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to set GICC_CTLR for cpu0 HIGH";
        EXPECT_OK(cpuif_out.readw(GICC_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICC_CTLR from CPUIF";
        EXPECT_EQ(val, 0xb) << "GICC_CTLR of CPUIF should be 3";

        val = 0x3;
        EXPECT_OK(distif_out.writew(GICD_CTLR, val, sbi_cpuid(0)))
            << "failed to write GICD_CTLR";
        EXPECT_OK(distif_out.readw(GICD_CTLR, val, sbi_cpuid(0)))
            << "failed to read GICD_CTLR from DISTIF";
        EXPECT_EQ(val, 0x3) << "GICD_CTLR of DISTIF should be 3";

        // read and write ISENABLER_SPI, ITARGTES_SPI Register of DISTIF and
        // PMR Register of CPU0
        val = 0x3;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI, val))
            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(distif_out.readw(GICD_ITARGETS_SPI, val))
            << "failed to read ITARGETS_SPI register of distributor";
        EXPECT_EQ(val, 0x03) << "writing to ITARGETS_SPI not successful";
        val = 0x3;
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0x1, val))
            << "failed to write ITARGETS_SPI register of distributor";
        EXPECT_OK(distif_out.readw(GICD_ITARGETS_SPI + 0x1, val))
            << "failed to read ITARGETS_SPI register of distributor";
        EXPECT_EQ(val, 0x03) << "writing to ITARGETS_SPI not successful";

        val = 0xf;
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to set Priority Mask GICC_PMR for cpu0";
        EXPECT_OK(cpuif_out.readw(GICC_PMR, val, sbi_cpuid(0)))
            << "failed to read GICC_PMR for cpu0";
        EXPECT_EQ(val, 0xf) << "writing to GICC_PMR of cpu0 not successful";

        val = 0b10000000011;
        EXPECT_OK(distif_out.writew(GICD_ICENABLER_SPI, -1));
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(distif_out.readw(GICD_ISENABLER_SPI, val))
            << "failed to read GICD_ISENABLER_SPI";
        EXPECT_EQ(val, 0b10000000011)
            << "writing to GICD_ISENABLER_SPI not successful";

        val = 0b00000001;
        EXPECT_OK(distif_out.writew(GICD_IGROUPR_SPI, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.readw(GICD_IGROUPR_SPI, val, sbi_cpuid(0)))
            << "failed to read GICD_IGROUPR";
        EXPECT_EQ(val, 0b00000001) << "writing to GICD_IGROUPR not successful";

        val = 0x00000102;
        EXPECT_OK(distif_out.writew(GICD_IPRIORITY_SPI, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.readw(GICD_IPRIORITY_SPI, val, sbi_cpuid(0)))
            << "failed to read GICD_IPRIORITY";
        EXPECT_EQ(val, 0x00000102)
            << "writing to GICD_IPRIORITY not successful";

        wait(SC_ZERO_TIME);

        // setting SPI connection of processor 1 HIGH
        spi_out[0].write(true);
        spi_out[1].write(true);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 0)
            << "IRQ should not have been signaled to irq on cpu0";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to cpu1";
        EXPECT_EQ(nfirq_in[0], 1)
            << "IRQ should have been signaled to fiq on cpu0";
        EXPECT_EQ(nfirq_in[1], 0)
            << "IRQ should not have been signaled to fiq on cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xff) << "GICC_RPR should be 255 (idle priority)"
                             << " -> no handling of interrupt";

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 33) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        // Test that current interrupt is not preempted by interrupt in same
        // group
        spi_out[2].write(true);

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x1) << "GICC_RPR should be 1023"
                            << " -> handling of interrupt of priority 1";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x21;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 0)
            << "IRQ should not have been signaled to irq on cpu0";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to cpu1";
        EXPECT_EQ(nfirq_in[0], 1)
            << "IRQ should have been signaled to fiq on cpu0";
        EXPECT_EQ(nfirq_in[1], 0)
            << "IRQ should not have been signaled to fiq on cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xff) << "GICC_RPR should be 255 (idle priority)"
                             << " -> no handling of interrupt";

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_IAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 42) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x0) << "GICC_RPR should be 1023"
                            << " -> handling of interrupt of priority 1";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x2a;
        EXPECT_OK(cpuif_out.writew(GICC_EOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";
        wait(SC_ZERO_TIME);

        EXPECT_EQ(nirq_in[0], 1)
            << "IRQ should have been signaled to irq on cpu0";
        EXPECT_EQ(nirq_in[1], 0)
            << "IRQ should not have been signaled to cpu1";
        EXPECT_EQ(nfirq_in[0], 0)
            << "IRQ should not have been signaled to fiq on cpu0";
        EXPECT_EQ(nfirq_in[1], 0)
            << "IRQ should not have been signaled to fiq on cpu1";

        // cpu0 reads IAR of its CPU interface
        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0xff) << "GICC_RPR should be 255 (idle priority)"
                             << " -> no handling of interrupt";

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_AIAR, val, sbi_cpuid(0)))
            << "failed to read GICC_IAR of cpu0";
        EXPECT_EQ(val, 32) << "read wrong interrupt value from GICC_IAR";
        wait(SC_ZERO_TIME);

        val = 0x0;
        EXPECT_OK(cpuif_out.readw(GICC_RPR, val, sbi_cpuid(0)))
            << "failed to read GICC_RPR";
        EXPECT_EQ(val, 0x2) << "GICC_RPR should be 1023"
                            << " -> handling of interrupt of priority 1";

        // cpu0 writes interrupt ID 32 = 0x20 to EOIR register
        val = 0x20;
        EXPECT_OK(cpuif_out.writew(GICC_AEOIR, val, sbi_cpuid(0)))
            << "cpu0 failed to write in GICC_EOIR";

        // setting SPI connection of processor 1 LOW
        spi_out[0].write(false);
        spi_out[1].write(false);
        spi_out[2].write(false);
        wait(SC_ZERO_TIME);

        // reset registers
        val = 0x0;
        EXPECT_OK(distif_out.writew(GICD_ISENABLER_SPI, val));
        EXPECT_OK(cpuif_out.writew(GICC_PMR, val, sbi_cpuid(0)));
        EXPECT_OK(distif_out.writew(GICD_ITARGETS_SPI + 0x01, val));
        EXPECT_OK(distif_out.writew(GICD_CTLR, val));
        EXPECT_OK(cpuif_out.writew(GICC_CTLR, val, sbi_cpuid(0)));
    }
};

TEST(gic400, gic400) {
    gic400_stim stim("stim");
    arm::gic400 gic400("gic400");

    EXPECT_STREQ(gic400.kind(), "vcml::arm::gic400");
    EXPECT_STREQ(gic400.cpuif.kind(), "vcml::arm::gic400::cpuif");
    EXPECT_STREQ(gic400.vcpuif.kind(), "vcml::arm::gic400::vcpuif");
    EXPECT_STREQ(gic400.distif.kind(), "vcml::arm::gic400::distif");
    EXPECT_STREQ(gic400.vifctrl.kind(), "vcml::arm::gic400::vifctrl");

    stim.clk.bind(gic400.clk);
    stim.rst.bind(gic400.rst);

    stim.distif_out.bind(gic400.distif.in);
    stim.cpuif_out.bind(gic400.cpuif.in);
    stim.vifctrl_out.bind(gic400.vifctrl.in);
    stim.vcpuif_out.bind(gic400.vcpuif.in);

    for (int cpu = 0; cpu < 2; cpu++) {
        gic400.fiq_out[cpu].bind(stim.nfirq_in[cpu]);
        gic400.irq_out[cpu].bind(stim.nirq_in[cpu]);
        gic400.vfiq_out[cpu].bind(stim.vfirq_in[cpu]);
        gic400.virq_out[cpu].bind(stim.vnirq_in[cpu]);
        stim.spi_out[cpu].bind(gic400.spi_in[cpu]);
        stim.ppi_out[cpu].bind(gic400.ppi(cpu, 0));
    }

    stim.spi_out[2].bind(gic400.spi_in[10]);

    sc_core::sc_start();
}
