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

enum lan9118_addr : u64 {
    CSR_ID_REV = 0x50,
    CSR_IRQ_CFG = 0x54,
    CSR_IRQ_STS = 0x58,
    CSR_IRQ_EN = 0x5c,
    CSR_BYTE_TEST = 0x64,
    CSR_FIFO_INT = 0x68,
    CSR_RX_CFG = 0x6c,
    CSR_TX_CFG = 0x70,
    CSR_HW_CFG = 0x74,
    CSR_RX_DP_CTRL = 0x78,
    CSR_RX_FIFO_INF = 0x7c,
    CSR_TX_FIFO_INF = 0x80,
    CSR_PMT_CTRL = 0x84,
    CSR_GPIO_CFG = 0x88,
    CSR_GPT_CFG = 0x8c,
    CSR_GPT_CNT = 0x90,
    CSR_WORD_SWAP = 0x98,
    CSR_FREE_RUN = 0x9c,
    CSR_RX_DROP = 0xa0,
    CSR_MAC_CMD = 0xa4,
    CSR_MAC_DATA = 0xa8,
    CSR_AFC_CFG = 0xac,
    CSR_E2P_CMD = 0xb0,
    CSR_E2P_DATA = 0xb4,
};

enum lan9118_mac_csr : u32 {
    MAC_CR = 1,
    MAC_ADDRH = 2,
    MAC_ADDRL = 3,
    MAC_HASHH = 4,
    MAC_HASHL = 5,
    MAC_MII_ACC = 6,
    MAC_MII_DATA = 7,
    MAC_FLOW = 8,
};

enum lan9117_phy_csr : u32 {
    PHY_CR = 0,
    PHY_STATUS = 1,
    PHY_IDENT1 = 2,
    PHY_IDENT2 = 3,
};

class lan9118_bench : public test_base
{
public:
    ethernet::lan9118 lan;
    tlm_initiator_socket out;
    gpio_target_socket irq;

    lan9118_bench(const sc_module_name& nm):
        test_base(nm), lan("lan9118"), out("out"), irq("irq") {
        out.bind(lan.in);
        lan.irq.bind(irq);
        lan.eth_tx.stub();
        lan.eth_rx.stub();
        rst.bind(lan.rst);
        clk.bind(lan.clk);
    }

    void check_irq(unsigned int irq) {
        u32 data = 0;
        EXPECT_OK(out.readw(CSR_IRQ_STS, data)) << "cannot read IRQ STS";
        EXPECT_TRUE(data & (1u << irq)) << "IRQ " << irq << " inactive";
        EXPECT_OK(out.writew(CSR_IRQ_STS, ~0u)) << "cannot clear IRQ status";
        EXPECT_OK(out.readw(CSR_IRQ_STS, data)) << "cannot check IRQ STS";
        EXPECT_EQ(data, 0u) << "interrupts did not clear on write";
    }

    u32 mac_read(u32 mac_csr) {
        u32 data = 1u << 31 | 1u << 30 | (mac_csr & 0xff);
        EXPECT_OK(out.writew(CSR_MAC_CMD, data)) << "cannot write MAC_CMD";
        EXPECT_OK(out.readw(CSR_MAC_DATA, data)) << "cannot read MAC_DATA";
        return data;
    }

    void mac_write(u32 mac_csr, u32 val) {
        u32 data = 1u << 31 | (mac_csr & 0xff);
        EXPECT_OK(out.writew(CSR_MAC_DATA, val)) << "cannot write MAC_DATA";
        EXPECT_OK(out.writew(CSR_MAC_CMD, data)) << "cannot write MAC_CMD";
    }

    u32 phy_read(u32 phy_csr) {
        u32 cmd = 1u << 11 | phy_csr << 6 | 1;
        mac_write(MAC_MII_ACC, cmd);
        return mac_read(MAC_MII_DATA);
    }

    void phy_write(u32 phy_csr, u32 val) {
        u32 cmd = 1u << 11 | phy_csr << 6 | 1u << 1 | 1;
        mac_write(MAC_MII_DATA, val & 0xffff);
        mac_write(MAC_MII_ACC, cmd);
    }

    virtual void run_test() override {
        // test that interrupts are reset
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irq.read()) << "irq not reset";

        // check ID and BYTE_TEST
        u32 data = 0;
        EXPECT_OK(out.readw(CSR_ID_REV, data)) << "cannot read ID_REV";
        EXPECT_EQ(data, 0x01180001) << "wrong id"; // ID for lan9118
        EXPECT_CE(out.writew(CSR_ID_REV, data)) << "ID_REV must be read only";

        EXPECT_OK(out.readw(CSR_BYTE_TEST, data)) << "cannot read BYTE_TEST";
        EXPECT_EQ(data, 0x87654321) << "wrong byte test value";
        EXPECT_CE(out.writew(CSR_BYTE_TEST, data)) << "must be read only";

        // check EEPROM access
        u32 cmd = 0; // read EEPROM cell 0
        EXPECT_OK(out.writew(CSR_E2P_CMD, cmd)) << "cannot write EEPROM cmd";
        EXPECT_OK(out.readw(CSR_E2P_DATA, data)) << "cannot read EEPROM data";
        EXPECT_EQ(data, 0xa5) << "EEPROM magic value mismatch";

        // check MAC was loaded from EEPROM
        mac_addr addr = lan.mac_address();
        EXPECT_EQ(to_string(addr), "12:34:56:78:9a:bc") << "address broken";
        EXPECT_EQ(lan.mac.addrh, 0xbc9au) << "address not loaded into MAC";
        EXPECT_EQ(lan.mac.addrl, 0x78563412u) << "address not loaded into MAC";

        // check free running timer
        EXPECT_OK(out.readw(CSR_FREE_RUN, data)) << "cannot read timer";
        EXPECT_EQ(data, 0) << "timer not reset";
        wait(1, SC_SEC);
        EXPECT_OK(out.readw(CSR_FREE_RUN, data)) << "cannot read timer";
        EXPECT_EQ(data, 25000000) << "timer did not count";

        // check general purpose timer
        data = 1u << 29 | 0x1000;
        EXPECT_OK(out.writew(CSR_GPT_CFG, data)) << "cannot configure GPT";
        EXPECT_OK(out.readw(CSR_GPT_CNT, data)) << "cannot read GPT";
        EXPECT_EQ(data, 0x1000);
        wait(10 * 100, SC_US);
        EXPECT_OK(out.readw(CSR_GPT_CNT, data)) << "cannot read GPT";
        EXPECT_EQ(data, 0x1000 - 10) << "GPT reports wrong counter value";
        wait(0x1000 * 100, SC_US);
        EXPECT_OK(out.readw(CSR_GPT_CNT, data)) << "cannot read GPT";
        EXPECT_EQ(data, 0x1000 - 10) << "GPT reports wrong wrap value";
        check_irq(19);

        // check MAC register access
        EXPECT_EQ(mac_read(MAC_ADDRH), 0x0000bc9a) << "cannot read MAC_ADDRH";
        EXPECT_EQ(mac_read(MAC_ADDRL), 0x78563412) << "cannot read MAC_ADDRL";

        // check PHY register access
        EXPECT_EQ(phy_read(PHY_IDENT1), 0x0007) << "cannot read PHY_IDENT1";
        EXPECT_EQ(phy_read(PHY_IDENT2), 0xc0d1) << "cannot read PHY_IDENT2";

        // check software interrupt
        EXPECT_OK(out.writew(CSR_IRQ_CFG, 1u << 8)) << "cannot enable IRQs";
        EXPECT_OK(out.writew(CSR_IRQ_EN, 1u << 31)) << "cannot enable SW_IRQ";
        EXPECT_TRUE(irq.read()) << "interrupt did not get raised";
        check_irq(31);
        EXPECT_FALSE(irq.read()) << "interrupts did not get cleared";
    }
};

TEST(lan9118, simulate) {
    lan9118_bench test("bench");
    sc_core::sc_start();
}
