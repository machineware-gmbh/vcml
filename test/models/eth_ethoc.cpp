/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : u64 {
    ETHOC_MODER = 0x00,
    ETHOC_INT_SOURCE = 0x04,
    ETHOC_INT_MASK = 0x08,
    ETHOC_IPGT = 0x0c,
    ETHOC_IPGR1 = 0x10,
    ETHOC_IPGR2 = 0x14,
    ETHOC_PACKETLEN = 0x18,
    ETHOC_COLLCONF = 0x1c,
    ETHOC_TX_BD_NUM = 0x20,
    ETHOC_CTRLMODER = 0x24,
    ETHOC_MIIMODER = 0x28,
    ETHOC_MIICOMMAND = 0x2c,
    ETHOC_MIIADDRESS = 0x30,
    ETHOC_MIITX_DATA = 0x34,
    ETHOC_MIIRX_DATA = 0x38,
    ETHOC_MIISTATUS = 0x3c,
    ETHOC_MAC_ADDR0 = 0x40,
    ETHOC_MAC_ADDR1 = 0x44,
    ETHOC_HASH0 = 0x48,
    ETHOC_HASH1 = 0x4c,
    ETHOC_TXCTRL = 0x50,
    ETHOC_BD = 0x400,
    ETHOC_END = 0x800,
};

enum : u32 {
    NUM_BD = 128,
    NUM_TXBD = 4,
    MEM_SIZE = 0x10000,
    TX_BUF = 0x1000,
    RX_BUF = 0x8000,
    BUF_SIZE = 0x800,
    FCS_LEN = 4,
    MAX_LEN = 1518,
};

enum : u32 {
    MODER_RXEN = ethernet::ethoc::MODER_RXEN,
    MODER_TXEN = ethernet::ethoc::MODER_TXEN,
    MODER_PRO = ethernet::ethoc::MODER_PRO,
    MODER_RST = ethernet::ethoc::MODER_RST,
    INT_TXB = ethernet::ethoc::INT_SOURCE_TXB,
    INT_TXE = ethernet::ethoc::INT_SOURCE_TXE,
    INT_RXB = ethernet::ethoc::INT_SOURCE_RXB,
    INT_RXE = ethernet::ethoc::INT_SOURCE_RXE,
    TXBD_RD = ethernet::ethoc::TXBD_RD,
    TXBD_IRQ = ethernet::ethoc::TXBD_IRQ,
    TXBD_WR = ethernet::ethoc::TXBD_WR,
    RXBD_E = ethernet::ethoc::RXBD_E,
    RXBD_IRQ = ethernet::ethoc::RXBD_IRQ,
    RXBD_WRAP = ethernet::ethoc::RXBD_WRAP,
    MII_RSTAT = ethernet::ethoc::MIICOMMAND_RSTAT,
    MII_WCTRLDATA = ethernet::ethoc::MIICOMMAND_WCTRLDATA,
};

class ethoc_test : public test_base, public eth_host
{
public:
    ethernet::ethoc ethoc;
    generic::memory mem;
    tlm_initiator_socket out;
    gpio_target_socket irq;
    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    const mac_addr mac;
    const mac_addr peer;
    vector<eth_frame> sent;

    virtual void eth_receive(const eth_target_socket& socket,
                             const eth_frame& frame) override {
        sent.push_back(frame);
    }

    ethoc_test(const sc_module_name& nm):
        test_base(nm),
        eth_host(),
        ethoc("ethoc"),
        mem("mem", MEM_SIZE),
        out("out"),
        irq("irq"),
        eth_tx("eth_tx"),
        eth_rx("eth_rx"),
        mac("12:34:56:78:9a:bc"),
        peer("aa:bb:cc:dd:ee:ff"),
        sent() {
        out.bind(ethoc.in);
        ethoc.out.bind(mem.in);
        ethoc.irq.bind(irq);
        ethoc.eth_tx.bind(eth_rx);
        eth_tx.bind(ethoc.eth_rx);
        rst.bind(ethoc.rst);
        clk.bind(ethoc.clk);
        rst.bind(mem.rst);
        clk.bind(mem.clk);

        add_test("strings", &ethoc_test::test_strings);
        add_test("defaults", &ethoc_test::test_defaults);
        add_test("registers", &ethoc_test::test_registers);
        add_test("mac_address", &ethoc_test::test_mac_address);
        add_test("mii", &ethoc_test::test_mii);
        add_test("descriptors", &ethoc_test::test_descriptors);
        add_test("transmit", &ethoc_test::test_transmit);
        add_test("transmit_errors", &ethoc_test::test_transmit_errors);
        add_test("receive", &ethoc_test::test_receive);
        add_test("receive_filter", &ethoc_test::test_receive_filter);
        add_test("receive_errors", &ethoc_test::test_receive_errors);
        add_test("soft_reset", &ethoc_test::test_soft_reset);
    }

    u32 read(u64 addr) {
        u32 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void write_bd(u32 idx, u32 info, u32 addr) {
        write(ETHOC_BD + idx * 8, info);
        write(ETHOC_BD + idx * 8 + 4, addr);
    }

    u32 read_bd(u32 idx) { return read(ETHOC_BD + idx * 8); }

    void settle() { wait(sc_time(10, SC_US)); }

    void setup(u32 mode) {
        sent.clear();
        memset(mem.data(), 0, MEM_SIZE);
        for (u32 i = 0; i < NUM_BD; i++)
            write_bd(i, 0, 0);
        write(ETHOC_TX_BD_NUM, NUM_TXBD);
        write(ETHOC_INT_MASK, INT_TXB | INT_TXE | INT_RXB | INT_RXE);
        write(ETHOC_MODER, mode);
        settle();
    }

    void teardown() {
        write(ETHOC_MODER, 0);
        write(ETHOC_INT_MASK, 0);
        write(ETHOC_INT_SOURCE, ~0u);
        settle();
        EXPECT_FALSE(irq);
    }

    eth_frame make_frame(const mac_addr& dest, size_t len, u8 seed) {
        vector<u8> payload(len);
        for (size_t i = 0; i < len; i++)
            payload[i] = (u8)(seed + i);
        return eth_frame(dest, peer, payload);
    }

    void queue_tx(u32 idx, const eth_frame& frame, u32 flags) {
        u32 addr = TX_BUF + idx * BUF_SIZE;
        memcpy(mem.data() + addr, frame.data(), frame.size());
        write_bd(idx, (u32)frame.size() << 16 | TXBD_RD | flags, addr);
    }

    void queue_rx(u32 idx, u32 flags) {
        write_bd(NUM_TXBD + idx, RXBD_E | flags, RX_BUF + idx * BUF_SIZE);
    }

    eth_frame rx_frame(u32 idx) {
        u32 info = read_bd(NUM_TXBD + idx);
        u32 len = info >> 16;
        EXPECT_GE(len, FCS_LEN);
        EXPECT_LE(len, BUF_SIZE);
        if (len < FCS_LEN || len > BUF_SIZE)
            return eth_frame();
        return eth_frame(mem.data() + RX_BUF + idx * BUF_SIZE, len - FCS_LEN);
    }

    void test_strings() {
        EXPECT_STREQ(ethoc.kind(), "vcml::ethernet::ethoc");
        EXPECT_STREQ(ethoc.version(), VCML_VERSION_STRING);
        EXPECT_EQ(ethoc.mac.get(), "12:34:56:78:9a:bc");
    }

    void test_defaults() {
        EXPECT_EQ(read(ETHOC_MODER), 0xa000);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), 0);
        EXPECT_EQ(read(ETHOC_INT_MASK), 0);
        EXPECT_EQ(read(ETHOC_IPGT), 0x12);
        EXPECT_EQ(read(ETHOC_IPGR1), 0xc);
        EXPECT_EQ(read(ETHOC_IPGR2), 0x12);
        EXPECT_EQ(read(ETHOC_PACKETLEN), 0x400600);
        EXPECT_EQ(read(ETHOC_COLLCONF), 0xf003f);
        EXPECT_EQ(read(ETHOC_TX_BD_NUM), 0x40);
        EXPECT_EQ(read(ETHOC_CTRLMODER), 0);
        EXPECT_EQ(read(ETHOC_MIIMODER), 0x64);
        EXPECT_EQ(read(ETHOC_MIISTATUS), 0);
        EXPECT_EQ(read(ETHOC_MAC_ADDR0), 0x56789abc);
        EXPECT_EQ(read(ETHOC_MAC_ADDR1), 0x1234);
        EXPECT_FALSE(irq);
    }

    void test_registers() {
        const u64 regs[] = {
            ETHOC_IPGT,      ETHOC_IPGR1,      ETHOC_IPGR2,
            ETHOC_PACKETLEN, ETHOC_COLLCONF,   ETHOC_CTRLMODER,
            ETHOC_MIIMODER,  ETHOC_MIIADDRESS, ETHOC_MIITX_DATA,
            ETHOC_HASH0,     ETHOC_HASH1,      ETHOC_TXCTRL,
        };

        for (u64 addr : regs) {
            write(addr, 0x12345678 + addr);
            EXPECT_EQ(read(addr), 0x12345678 + addr) << std::hex << addr;
        }

        write(ETHOC_TX_BD_NUM, 0xffffff10);
        EXPECT_EQ(read(ETHOC_TX_BD_NUM), 0x10);

        u32 data = 0;
        EXPECT_CE(out.writew(ETHOC_MIIRX_DATA, data));
        EXPECT_CE(out.writew(ETHOC_MIISTATUS, data));
        EXPECT_AE(out.readw(ETHOC_TXCTRL + 4, data));
        EXPECT_AE(out.readw(ETHOC_BD - 4, data));
        EXPECT_AE(out.readw(ETHOC_END, data));
        EXPECT_AE(out.writew(ETHOC_END, data));
    }

    void test_mac_address() {
        write(ETHOC_MAC_ADDR0, 0x33445566);
        write(ETHOC_MAC_ADDR1, 0x1122);
        EXPECT_EQ(read(ETHOC_MAC_ADDR0), 0x33445566);
        EXPECT_EQ(read(ETHOC_MAC_ADDR1), 0x1122);

        setup(MODER_RXEN);
        queue_rx(0, 0);
        eth_frame frame = make_frame(mac_addr("11:22:33:44:55:66"), 64, 1);
        eth_tx.send(frame);
        settle();
        EXPECT_FALSE(read_bd(NUM_TXBD) & RXBD_E);
        EXPECT_EQ(rx_frame(0), frame);
        teardown();

        write(ETHOC_MAC_ADDR0, 0x56789abc);
        write(ETHOC_MAC_ADDR1, 0x1234);
    }

    u32 mii_read(u32 phy, u32 reg) {
        write(ETHOC_MIIADDRESS, reg << 8 | phy);
        write(ETHOC_MIICOMMAND, MII_RSTAT);
        EXPECT_EQ(read(ETHOC_MIICOMMAND), MII_RSTAT);
        return read(ETHOC_MIIRX_DATA);
    }

    void test_mii() {
        const u32 bmcr_fulldplx = 0x0100;
        const u32 bmsr_lstatus = 0x0004;
        const u32 bmsr_anegcomplete = 0x0020;

        EXPECT_EQ(mii_read(0, 0) & bmcr_fulldplx, bmcr_fulldplx);
        EXPECT_EQ(mii_read(0, 1) & bmsr_lstatus, bmsr_lstatus);
        EXPECT_EQ(mii_read(0, 1) & bmsr_anegcomplete, bmsr_anegcomplete);

        u32 id1 = mii_read(0, 2);
        u32 id2 = mii_read(0, 3);
        EXPECT_NE(id1, 0);
        EXPECT_NE(id1, 0xffff);
        EXPECT_NE(id2, 0);
        EXPECT_NE(id2, 0xffff);

        EXPECT_NE(mii_read(0, 4), 0xffff);
        EXPECT_NE(mii_read(0, 5), 0xffff);
        for (u32 reg :
             { 6, 9, 10, 15, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 }) {
            EXPECT_EQ(mii_read(0, reg), 0) << "register " << reg;
        }

        for (u32 reg : { 7, 8, 11, 12, 13, 14, 16, 17, 29, 30, 31 })
            EXPECT_EQ(mii_read(0, reg), 0xffff) << "register " << reg;

        EXPECT_EQ(mii_read(0, 2), id1);
        EXPECT_EQ(mii_read(1, 3), id1);
        write(ETHOC_MIIADDRESS, 3 << 8);
        write(ETHOC_MIICOMMAND, MII_WCTRLDATA);
        EXPECT_EQ(read(ETHOC_MIIRX_DATA), id1);
    }

    void test_descriptors() {
        for (u32 i = 0; i < NUM_BD; i++)
            write_bd(i, 0x1000 + i, 0x2000 + i);

        for (u32 i = 0; i < NUM_BD; i++) {
            EXPECT_EQ(read(ETHOC_BD + i * 8), 0x1000 + i) << "bd " << i;
            EXPECT_EQ(read(ETHOC_BD + i * 8 + 4), 0x2000 + i) << "bd " << i;
        }

        u8 data = 0;
        EXPECT_OK(out.readw(ETHOC_END - 1, data));
        EXPECT_AE(out.readw(ETHOC_END, data));

        for (u32 i = 0; i < NUM_BD; i++)
            write_bd(i, 0, 0);
    }

    void test_transmit() {
        setup(MODER_TXEN);
        eth_frame frame0 = make_frame(peer, 64, 0x10);
        eth_frame frame1 = make_frame(peer, 1500, 0x20);
        eth_frame frame2 = make_frame(peer, 1, 0x30);

        queue_tx(0, frame0, 0);
        queue_tx(1, frame1, 0);
        settle();
        ASSERT_EQ(sent.size(), 2);
        EXPECT_EQ(sent[0], frame0);
        EXPECT_EQ(sent[1], frame1);
        EXPECT_EQ(read_bd(0) & TXBD_RD, 0);
        EXPECT_EQ(read_bd(1) & TXBD_RD, 0);
        EXPECT_EQ(read_bd(1) >> 16, frame1.size());
        EXPECT_EQ(read(ETHOC_INT_SOURCE), 0);
        EXPECT_FALSE(irq);

        queue_tx(0, frame0, 0);
        settle();
        EXPECT_EQ(sent.size(), 2);
        EXPECT_EQ(read_bd(0) & TXBD_RD, TXBD_RD);

        queue_tx(2, frame2, TXBD_IRQ | TXBD_WR);
        settle();
        ASSERT_EQ(sent.size(), 3);
        EXPECT_EQ(sent[2], frame2);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), INT_TXB);
        EXPECT_TRUE(irq);
        EXPECT_EQ(read_bd(0) & TXBD_RD, TXBD_RD);

        write(ETHOC_INT_SOURCE, INT_TXB);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), 0);
        EXPECT_FALSE(irq);
        settle();
        ASSERT_EQ(sent.size(), 4);
        EXPECT_EQ(sent[3], frame0);
        EXPECT_EQ(read_bd(0) & TXBD_RD, 0);

        write(ETHOC_INT_MASK, 0);
        queue_tx(1, frame1, TXBD_IRQ);
        settle();
        EXPECT_EQ(sent.size(), 5);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), INT_TXB);
        EXPECT_FALSE(irq);
        write(ETHOC_INT_MASK, INT_TXB);
        EXPECT_TRUE(irq);

        teardown();
        queue_tx(2, frame2, 0);
        settle();
        EXPECT_EQ(sent.size(), 5);
        EXPECT_EQ(read_bd(2) & TXBD_RD, TXBD_RD);
    }

    void test_transmit_errors() {
        setup(MODER_TXEN);

        write_bd(0, (MAX_LEN + 1) << 16 | TXBD_RD, TX_BUF);
        settle();
        EXPECT_TRUE(sent.empty());
        EXPECT_EQ(read_bd(0) & TXBD_RD, 0);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), INT_TXE);
        EXPECT_TRUE(irq);
        write(ETHOC_INT_SOURCE, INT_TXE);
        EXPECT_FALSE(irq);

        write_bd(1, 64 << 16 | TXBD_RD, MEM_SIZE);
        settle();
        EXPECT_TRUE(sent.empty());
        EXPECT_EQ(read_bd(1) & TXBD_RD, 0);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), INT_TXE);
        EXPECT_TRUE(irq);

        teardown();
    }

    void test_receive() {
        setup(MODER_RXEN);
        eth_frame frame0 = make_frame(mac, 64, 0x40);
        eth_frame frame1 = make_frame(mac, 1500, 0x50);
        eth_frame frame2 = make_frame(mac, 1, 0x60);

        eth_tx.send(frame0);
        settle();
        EXPECT_EQ(read(ETHOC_INT_SOURCE), 0);

        queue_rx(0, 0);
        queue_rx(1, RXBD_IRQ);
        settle();
        EXPECT_EQ(read_bd(NUM_TXBD + 0) & RXBD_E, 0);
        EXPECT_EQ(read_bd(NUM_TXBD + 1) & RXBD_E, RXBD_E);
        EXPECT_EQ(rx_frame(0), frame0);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), 0);
        EXPECT_FALSE(irq);

        eth_tx.send(frame1);
        settle();
        EXPECT_EQ(read_bd(NUM_TXBD + 1) & RXBD_E, 0);
        EXPECT_EQ(read_bd(NUM_TXBD + 1) & RXBD_IRQ, RXBD_IRQ);
        EXPECT_EQ(rx_frame(1), frame1);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), INT_RXB);
        EXPECT_TRUE(irq);
        write(ETHOC_INT_SOURCE, INT_RXB);
        EXPECT_FALSE(irq);

        write_bd(NUM_BD - 1, RXBD_E, RX_BUF + 2 * BUF_SIZE);
        queue_rx(2, RXBD_WRAP);
        queue_rx(0, 0);
        eth_tx.send(frame2);
        eth_tx.send(frame1);
        settle();
        EXPECT_EQ(rx_frame(2), frame2);
        EXPECT_EQ(rx_frame(0), frame1);
        EXPECT_EQ(read_bd(NUM_BD - 1) & RXBD_E, RXBD_E);
        EXPECT_FALSE(irq);

        teardown();
    }

    void test_receive_filter() {
        setup(MODER_RXEN);
        eth_frame other = make_frame(peer, 64, 0x70);
        eth_frame broadcast = make_frame(mac_addr("ff:ff:ff:ff:ff:ff"), 64,
                                         0x80);

        queue_rx(0, 0);
        queue_rx(1, 0);
        eth_tx.send(other);
        settle();
        EXPECT_EQ(read_bd(NUM_TXBD) & RXBD_E, RXBD_E);

        eth_tx.send(broadcast);
        settle();
        EXPECT_EQ(read_bd(NUM_TXBD) & RXBD_E, 0);
        EXPECT_EQ(rx_frame(0), broadcast);

        write(ETHOC_MODER, MODER_RXEN | MODER_PRO);
        eth_tx.send(other);
        settle();
        EXPECT_EQ(read_bd(NUM_TXBD + 1) & RXBD_E, 0);
        EXPECT_EQ(rx_frame(1), other);

        teardown();
    }

    void test_receive_errors() {
        setup(MODER_RXEN);
        eth_frame frame = make_frame(mac, 64, 0x90);

        write_bd(NUM_TXBD, RXBD_E, MEM_SIZE);
        eth_tx.send(frame);
        settle();
        EXPECT_EQ(read_bd(NUM_TXBD) & RXBD_E, 0);
        EXPECT_EQ(read(ETHOC_INT_SOURCE), INT_RXE);
        EXPECT_TRUE(irq);

        teardown();
    }

    void test_soft_reset() {
        write(ETHOC_IPGT, 0x55);
        write(ETHOC_INT_MASK, INT_TXB);
        write(ETHOC_TX_BD_NUM, NUM_TXBD);

        write(ETHOC_MODER, MODER_RST | MODER_PRO);
        EXPECT_EQ(read(ETHOC_MODER), MODER_PRO);
        EXPECT_EQ(read(ETHOC_IPGT), 0x12);
        EXPECT_EQ(read(ETHOC_INT_MASK), 0);
        EXPECT_EQ(read(ETHOC_TX_BD_NUM), 0x40);
        EXPECT_EQ(read(ETHOC_MAC_ADDR0), 0x56789abc);
        EXPECT_EQ(read(ETHOC_MAC_ADDR1), 0x1234);
        EXPECT_FALSE(irq);

        write(ETHOC_MODER, 0xa000);
    }
};

TEST(ethernet, ethoc) {
    ethoc_test test("test");
    sc_core::sc_start();
}
