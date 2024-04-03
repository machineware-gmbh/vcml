/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class xhci_test : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    usb::xhci xhci;
    usb::keyboard keyboard2;
    usb::keyboard keyboard3;
    usb::drive drive2;
    usb::hostdev hostdev;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    xhci_test(const sc_module_name& nm):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        xhci("xhci"),
        keyboard2("keyboard2"),
        keyboard3("keyboard3"),
        drive2("drive2"),
        hostdev("hostdev"),
        out("out"),
        irq("irq") {
        xhci.usb_out[0].bind(keyboard2.usb_in);
        xhci.usb_out[1].bind(keyboard3.usb_in);
        xhci.usb_out[2].bind(drive2.usb_in);
        xhci.usb_out[3].bind(hostdev.usb_in);

        bus.bind(mem.in, 0, 0xfff);
        bus.bind(xhci.in, 0x1000, 0x1fff);

        bus.bind(out);
        bus.bind(xhci.dma);

        xhci.irq.bind(irq);

        clk.bind(bus.clk);
        clk.bind(mem.clk);
        clk.bind(xhci.clk);

        rst.bind(bus.rst);
        rst.bind(mem.rst);
        rst.bind(xhci.rst);

        EXPECT_STREQ(xhci.kind(), "vcml::usb::xhci");
        EXPECT_STREQ(keyboard2.kind(), "vcml::usb::keyboard");
        EXPECT_STREQ(keyboard3.kind(), "vcml::usb::keyboard");
        EXPECT_STREQ(drive2.kind(), "vcml::usb::drive");
        EXPECT_STREQ(hostdev.kind(), "vcml::usb::hostdev");
    }

    void test_capabilities() {
        u32 hciversion;
        ASSERT_OK(out.readw(0x1000, hciversion));
        EXPECT_EQ(hciversion >> 16, 0x100); // hci version 1.0

        u32 hcsparams1;
        ASSERT_OK(out.readw(0x1004, hcsparams1));
        EXPECT_EQ(hcsparams1, (2 * 5) << 24 | 1 << 8 | 64);

        u32 excaps[2];
        ASSERT_OK(out.readw(0x1020, excaps[0]));
        ASSERT_OK(out.readw(0x1030, excaps[1]));
        EXPECT_EQ(excaps[0] >> 24, 0x02); // usb2 support
        EXPECT_EQ(excaps[1] >> 24, 0x03); // usb3 support
    }

    static constexpr u64 addr_portsc(size_t portidx) {
        return 0x1480 + portidx * 0x10;
    }

    void test_ports() {
        u32 portsc[6];
        ASSERT_OK(out.readw(addr_portsc(0), portsc[0])); // usb2 port 0
        ASSERT_OK(out.readw(addr_portsc(1), portsc[1])); // usb2 port 1
        ASSERT_OK(out.readw(addr_portsc(2), portsc[2])); // usb2 port 2
        ASSERT_OK(out.readw(addr_portsc(5), portsc[3])); // usb3 port 0
        ASSERT_OK(out.readw(addr_portsc(6), portsc[4])); // usb3 port 1
        ASSERT_OK(out.readw(addr_portsc(7), portsc[5])); // usb3 port 2

        EXPECT_EQ((portsc[0] >> 10) & 0xf, 3); // usb2 speed
        EXPECT_EQ((portsc[1] >> 10) & 0xf, 0); // disconnected
        EXPECT_EQ((portsc[2] >> 10) & 0xf, 3); // usb2 speed

        EXPECT_EQ((portsc[3] >> 10) & 0xf, 0); // disconnected
        EXPECT_EQ((portsc[4] >> 10) & 0xf, 4); // usb3 speed
        EXPECT_EQ((portsc[5] >> 10) & 0xf, 0); // disconnected
    }

    void test_microframes() {
        u32 start, end;
        ASSERT_OK(out.readw(0x1600, start));
        wait(125, SC_US);
        ASSERT_OK(out.readw(0x1600, end));
        EXPECT_EQ(end - start, 1);
    }

    virtual void run_test() override {
        wait(SC_ZERO_TIME);
        test_capabilities();
        wait(SC_ZERO_TIME);
        test_ports();
        wait(SC_ZERO_TIME);
        test_microframes();
    }
};

TEST(xhci, simulate) {
    broker brkr("brkr");
    brkr.define("system.xhci.num_ports", 5);
    brkr.define("system.keyboard2.usb3", false);
    brkr.define("system.keyboard3.usb3", true);
    brkr.define("system.drive2.usb3", false);
    xhci_test test("system");
    sc_core::sc_start();
}
