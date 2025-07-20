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

class test_harness : public test_base, public spi_host
{
public:
    spi::sifive model;

    fifo<u8> mosi;
    fifo<u8> miso;

    tlm_initiator_socket out;
    spi_target_socket spi;
    gpio_target_socket cs;
    gpio_target_socket irq;
    clk_target_socket sclk;

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& tx) override {
        mosi.push(tx.mosi);
        if (!miso.empty())
            tx.miso = miso.pop();
    }

    test_harness(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        model("model"),
        mosi(16),
        miso(16),
        out("out"),
        spi("spi"),
        cs("cs"),
        irq("irq"),
        sclk("sclk") {
        out.bind(model.in);
        model.spi_out.bind(spi);
        model.sclk.bind(sclk);
        model.cs[2].bind(cs);
        model.irq.bind(irq);
        rst.bind(model.rst);
        clk.bind(model.clk);

        EXPECT_STREQ(model.kind(), "vcml::spi::sifive");
    }

    enum : u64 {
        ADDR_SCKDIV = 0x00,
        ADDR_SCKMODE = 0x04,
        ADDR_CSID = 0x10,
        ADDR_CSDEF = 0x14,
        ADDR_CSMODE = 0x18,
        ADDR_FMT = 0x40,
        ADDR_TXDATA = 0x48,
        ADDR_RXDATA = 0x4c,
        ADDR_TXMARK = 0x50,
        ADDR_RXMARK = 0x54,
        ADDR_IE = 0x70,
        ADDR_IP = 0x74,
    };

    void test_serial_clock() {
        GTEST_LOG_(INFO) << "begin testing serial clock";

        EXPECT_EQ(sclk.read(), clk.read() / 8);
        ASSERT_OK(out.writew<u32>(ADDR_SCKDIV, 7));
        EXPECT_EQ(sclk.read(), clk.read() / 16);
        ASSERT_OK(out.writew<u32>(ADDR_SCKDIV, 15));
        EXPECT_EQ(sclk.read(), clk.read() / 32);

        GTEST_LOG_(INFO) << "finished testing serial clock";
    }

    void test_transmit() {
        u32 data;
        GTEST_LOG_(INFO) << "begin testing transmit";

        EXPECT_OK(out.writew<u32>(ADDR_FMT, 0x0008000c));
        EXPECT_OK(out.readw<u32>(ADDR_IP, data));
        EXPECT_EQ(data, 0u);

        // test transmission watermark
        ASSERT_OK(out.writew<u32>(ADDR_TXMARK, 1));
        ASSERT_OK(out.readw<u32>(ADDR_IP, data));
        EXPECT_EQ(data, 1u);
        EXPECT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_IE, 1));
        EXPECT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_TXMARK, 0));
        EXPECT_FALSE(irq);
        EXPECT_OK(out.writew<u32>(ADDR_IE, 0));

        // test chip-select
        ASSERT_OK(out.writew<u32>(ADDR_CSMODE, 2));
        ASSERT_OK(out.writew<u32>(ADDR_CSDEF, 0));
        ASSERT_OK(out.writew<u32>(ADDR_CSID, 2));

        // test transmission
        ASSERT_OK(out.writew<u32>(ADDR_TXDATA, 0x4321));
        ASSERT_OK(out.writew<u32>(ADDR_TXDATA, 0x8765));
        ASSERT_EQ(mosi.num_used(), 2);
        EXPECT_EQ(mosi.pop(), 0x21);
        EXPECT_EQ(mosi.pop(), 0x65);

        // check and clear chip-select
        ASSERT_TRUE(cs);
        ASSERT_OK(out.writew<u32>(ADDR_CSMODE, 0));
        EXPECT_FALSE(cs);

        // test nothing was received
        ASSERT_OK(out.readw<u32>(ADDR_RXDATA, data));
        EXPECT_EQ(data, 0x80000000);

        GTEST_LOG_(INFO) << "finished testing transmit";
    }

    void test_receive() {
        u32 data;
        GTEST_LOG_(INFO) << "begin testing receive";

        EXPECT_OK(out.writew<u32>(ADDR_FMT, 0x00080004));
        EXPECT_OK(out.readw<u32>(ADDR_IP, data));
        EXPECT_EQ(data, 0u);

        miso.push(0x11);
        miso.push(0x22);

        ASSERT_OK(out.writew<u32>(ADDR_RXMARK, 1));
        ASSERT_OK(out.readw<u32>(ADDR_IP, data));
        EXPECT_EQ(data, 0u);
        EXPECT_FALSE(irq);

        // push data to trigger reception
        ASSERT_EQ(mosi.num_used(), 0);
        ASSERT_OK(out.writew<u32>(ADDR_TXDATA, 0xff));
        ASSERT_OK(out.writew<u32>(ADDR_TXDATA, 0xff));
        ASSERT_EQ(mosi.num_used(), 2);
        EXPECT_EQ(mosi.pop(), 0xff);
        EXPECT_EQ(mosi.pop(), 0xff);
        EXPECT_EQ(miso.num_used(), 0);

        // check receive interrupt
        ASSERT_OK(out.readw<u32>(ADDR_IP, data));
        EXPECT_EQ(data, 2u);
        EXPECT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_IE, 2));
        EXPECT_TRUE(irq);

        // read data, interrupt should get cleared
        ASSERT_OK(out.readw<u32>(ADDR_RXDATA, data));
        EXPECT_EQ(data, 0x11);
        ASSERT_OK(out.readw<u32>(ADDR_IP, data));
        EXPECT_EQ(data, 0u);
        EXPECT_FALSE(irq);

        // read remaining data until fifo empty
        ASSERT_OK(out.readw<u32>(ADDR_RXDATA, data));
        EXPECT_EQ(data, 0x22);
        ASSERT_OK(out.readw<u32>(ADDR_RXDATA, data));
        EXPECT_EQ(data, 0x80000000);

        GTEST_LOG_(INFO) << "finished testing receive";
    }

    virtual void run_test() override {
        wait(SC_ZERO_TIME);
        test_serial_clock();
        wait(SC_ZERO_TIME);
        test_transmit();
        wait(SC_ZERO_TIME);
        test_receive();
    }
};

TEST(spi, sifive) {
    test_harness test("test");
    sc_core::sc_start();
}
