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

MATCHER_P(TxEvFIFOElemEq, n, "Equality matcher for TX event FIFO elements") {
    return (arg[0] == n[0]) && ((arg[1] | bit(22)) == n[1]);
}

MATCHER_P(CanFrameEq, n, "Equality matcher for CAN frames") {
    return (arg.msgid == n.msgid) && (arg.dlc == n.dlc) &&
           std::equal(std::begin(arg.data), std::end(arg.data),
                      std::begin(n.data));
}

MATCHER_P(CanFrameFDEq, n, "Equality matcher for CAN FD frames") {
    return (arg.msgid == n.msgid) && (arg.dlc == n.dlc) &&
           (arg.flags == n.flags) &&
           std::equal(std::begin(arg.data), std::end(arg.data),
                      std::begin(n.data));
}

enum m_can_addr : u64 {
    REG_CREL = 0x00,
    REG_ENDN = 0x04,
    REG_DBTP = 0x0c,
    REG_TEST = 0x10,
    REG_RWD = 0x14,
    REG_CCCR = 0x18,
    REG_NBTP = 0x1c,
    REG_TSCC = 0x20,
    REG_TSCV = 0x24,
    REG_TOCC = 0x28,
    REG_TOCV = 0x2c,
    REG_ECR = 0x40,
    REG_PSR = 0x44,
    REG_TDCR = 0x48,
    REG_IR = 0x50,
    REG_IE = 0x54,
    REG_ILS = 0x58,
    REG_ILE = 0x5c,
    REG_GFC = 0x80,
    REG_SIDFC = 0x84,
    REG_XIDFC = 0x88,
    REG_XIDAM = 0x90,
    REG_HPMS = 0x94,
    REG_NDAT1 = 0x98,
    REG_NDAT2 = 0x9c,
    REG_RXF0C = 0xa0,
    REG_RXF0S = 0xa4,
    REG_RXF0A = 0xa8,
    REG_RXBC = 0xac,
    REG_RXF1C = 0xb0,
    REG_RXF1S = 0xb4,
    REG_RXF1A = 0xb8,
    REG_RXESC = 0xbc,
    REG_TXBC = 0xc0,
    REG_TXFQS = 0xc4,
    REG_TXESC = 0xc8,
    REG_TXBRP = 0xcc,
    REG_TXBAR = 0xd0,
    REG_TXBCR = 0xd4,
    REG_TXBTO = 0xd8,
    REG_TXBCF = 0xdc,
    REG_TXBTIE = 0xe0,
    REG_TXBCIE = 0xe4,
    REG_TXEFC = 0xf0,
    REG_TXEFS = 0xf4,
    REG_TXEFA = 0xf8
};

enum m_can_bits : u32 {
    CCCR_CCE = bit(1),
    ILE_EINT0 = bit(0),
    ILE_EINT1 = bit(1),
    IR_RF0N = bit(0),
    IR_TC = bit(9),
    IR_TEFN = bit(12),
    BUF_HDR0_RTR = bit(29),
    BUF_HDR0_XTD = bit(30),
    BUF_HDR0_ESI = bit(31),
    BUF_HDR1_BRS = bit(20),
    BUF_HDR1_FDF = bit(21),
    TXBUF_T1_EFC = bit(23),
};

typedef field<0, 29, u32> BUF_HDR0_ID_XTD;
typedef field<18, 11, u32> BUF_HDR0_ID_STD;
typedef field<8, 8, u32> BUF_HDR1_MM_HI;
typedef field<16, 4, u32> BUF_HDR1_DLC;
typedef field<23, 8, u32> BUF_HDR1_MM_LO;
typedef field<8, 5, u32> TXEFS_EFGI;
typedef field<0, 6, u32> TXEFS_EFFL;
typedef field<16, 6, u32> TXEFC_EFS;
typedef field<24, 6, u32> TXBC_TFQS;
typedef field<0, 7, u32> RXFS_FFL;
typedef field<8, 6, u32> RXFS_FGI;
typedef field<16, 7, u32> RXFC_FS;

constexpr size_t TX_BUF_ELEM_HDR_SZ = 8;
constexpr size_t TX_EFIFO_ELEM_SZ = 8;
constexpr size_t RX_BUF_ELEM_HDR_SZ = 8;
constexpr size_t DATA_FIELD_SZ = 8;

constexpr size_t RX_FIFO0_ELEMS = 2;
constexpr size_t TX_EVFIFO_ELEMS = 2;
constexpr size_t TX_FIFO_ELEMS = 2;

constexpr u64 RX_FIFO0_START_ADDR = 0;
constexpr u64 TX_EVFIFO_START_ADDR = (RX_FIFO0_START_ADDR +
                                      RX_FIFO0_ELEMS * (RX_BUF_ELEM_HDR_SZ +
                                                        DATA_FIELD_SZ * 8));
constexpr u64 TX_FIFO_START_ADDR = (TX_EVFIFO_START_ADDR +
                                    TX_EVFIFO_ELEMS * TX_EFIFO_ELEM_SZ);

static can_frame generate_can_frame(bool xtd_id, bool rtr_frame) {
    can_frame frame{};
    frame.flags = 0;
    frame.dlc = rtr_frame ? 0 : 4;
    frame.msgid = xtd_id ? 0x9f334455 : 0x000005a1;

    if (rtr_frame)
        frame.msgid |= CAN_RTR;

    if (!rtr_frame) {
        frame.data[0] = 0xde;
        frame.data[1] = 0xad;
        frame.data[2] = 0xbe;
        frame.data[3] = 0xef;
    }

    return frame;
}

static can_frame generate_canfd_frame(bool xtd_id) {
    can_frame frame{};
    frame.msgid = xtd_id ? 0x9f334455 : 0x000005a1;
    frame.dlc = len2dlc(16);
    frame.flags = CANFD_FDF;

    static const u8 data[64] = {
        0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe,
        0xef, 0xde, 0xad, 0xbe, 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    memcpy(frame.data, data, min(sizeof(frame.data), sizeof(data)));
    return frame;
}

class mock_can : public peripheral, public can_host
{
public:
    can_target_socket can_in;
    can_initiator_socket can_out;

    mock_can(const sc_module_name& nm):
        peripheral(nm), can_in("can_in"), can_out("can_out") {}
};

class m_can_bench : public test_base
{
public:
    vcml::range addr_m_can;
    vcml::range addr_msgram;

    can::m_can m_can;
    generic::memory msgram;

    generic::bus bus;

    tlm_initiator_socket out;

    mock_can can;

    gpio_target_socket irq0;

    m_can_bench(const sc_module_name& nm):
        test_base(nm),
        addr_m_can(0x0, 0x3fff),
        addr_msgram(0x4000, 0x7fff),
        m_can("m_can", addr_msgram),
        msgram("msgram", addr_msgram.length() - 2),
        bus("bus"),
        out("out"),
        can("mock_can"),
        irq0("irq0") {
        rst.bind(m_can.rst);
        rst.bind(msgram.rst);
        rst.bind(bus.rst);
        rst.bind(can.rst);

        clk.bind(m_can.clk);
        clk.bind(msgram.clk);
        clk.bind(bus.clk);
        clk.bind(can.clk);

        bus.bind(m_can.in, addr_m_can);
        bus.bind(m_can.dma);
        bus.bind(msgram.in, addr_msgram);
        bus.bind(out);

        m_can.can_tx.bind(can.can_in);
        can.can_out.bind(m_can.can_rx);

        m_can.irq0.bind(irq0);
        m_can.irq1.stub();

        EXPECT_STREQ(m_can.kind(), "vcml::can::m_can");
    };

    void check_irq(u32 irq) {
        u32 data = 0;
        EXPECT_OK(out.readw(REG_IR, data)) << "cannot read register IR";
        EXPECT_TRUE(data & irq) << "IRQ(s) " << irq << " inactive";
        EXPECT_OK(out.writew(REG_IR, ~0u)) << "cannot clear IRQ status";
        EXPECT_OK(out.readw(REG_IR, data)) << "cannot check register IR";
        EXPECT_EQ(data, 0u) << "irqs did not clear on write";
    }

    void setup_m_can() {
        u32 data;
        // configure m_can
        EXPECT_OK(out.writew(REG_CCCR, CCCR_CCE))
            << "cannot enable m_can config mode";

        EXPECT_OK(out.writew(REG_ILE, ILE_EINT0))
            << "cannot enable irq line 0";

        EXPECT_OK(out.writew(REG_IE, IR_RF0N | IR_TC | IR_TEFN))
            << "cannot enable irq 0,9,12";

        EXPECT_OK(out.writew(REG_TXBTIE, 1u << 0))
            << "cannot enable tx occured irq 0";

        data = TX_FIFO_START_ADDR | TX_FIFO_ELEMS << TXBC_TFQS::OFFSET;
        EXPECT_OK(out.writew(REG_TXBC, data)) << "cannot set tx buffer config";

        data = RX_FIFO0_START_ADDR | RX_FIFO0_ELEMS << RXFC_FS::OFFSET;
        EXPECT_OK(out.writew(REG_RXF0C, data)) << "cannot set rx fifo0 config";

        data = TX_EVFIFO_START_ADDR | TX_EVFIFO_ELEMS << TXEFC_EFS::OFFSET;
        EXPECT_OK(out.writew(REG_TXEFC, data)) << "cannot set tx fifo cofnig";

        EXPECT_OK(out.writew(REG_TXESC, 7u))
            << "cannot set tx evfifo data size";

        EXPECT_OK(out.writew(REG_RXESC, 7u))
            << "cannot set rx fifo0 data size";

        EXPECT_OK(out.writew(REG_CCCR, 0u))
            << "cannot enable m_can operations";
    }

    void check_tx_evfifo(const u32 test[2]) {
        // check fifo fill level
        u32 txefs = 0;
        EXPECT_OK(out.readw(REG_TXEFS, txefs))
            << "cannot read tx evfifo status";
        EXPECT_EQ(get_field<TXEFS_EFFL>(txefs), 1)
            << "wrong tx evfifo fill level";

        // check fifo content
        u32 evfifo[2] = {};
        u64 addr = addr_msgram.start + TX_EVFIFO_START_ADDR +
                   get_field<TXEFS_EFGI>(txefs) * TX_EFIFO_ELEM_SZ;
        EXPECT_OK(out.readw(addr, evfifo)) << "cannot read from tx evfifo";
        EXPECT_THAT(test, TxEvFIFOElemEq(evfifo))
            << "tx evfifo data is not matching";

        // acknowledge read & check new fifo fill level
        EXPECT_OK(out.writew(REG_TXEFA, get_field<TXEFS_EFGI>(txefs)))
            << "cannot ack tx ev fifo elem";
        EXPECT_OK(out.readw(REG_TXEFS, txefs))
            << "cannot read new tx evfifo status";
        EXPECT_EQ(get_field<TXEFS_EFFL>(txefs), 0)
            << "wrong tx evfifo fill level";
    }

    void test_tx_frame(const can_frame test) {
        u64 addr = addr_msgram.start + TX_FIFO_START_ADDR;

        // construct tx buffer element
        u32 hdr[2] = {};
        if (test.is_eff()) {
            set_field<BUF_HDR0_ID_XTD>(hdr[0], test.msgid);
            hdr[0] |= BUF_HDR0_XTD;
        } else {
            set_field<BUF_HDR0_ID_STD>(hdr[0], test.msgid);
        }

        if (test.is_rtr())
            hdr[0] |= BUF_HDR0_RTR;

        set_field<BUF_HDR1_DLC>(hdr[1], test.dlc);

        hdr[1] |= TXBUF_T1_EFC;

        set_field<BUF_HDR1_MM_LO>(hdr[1], 123);
        EXPECT_OK(out.writew(addr, hdr))
            << "cannot write tx buf element header";
        EXPECT_OK(out.writew(addr + TX_BUF_ELEM_HDR_SZ, test.data))
            << "cannot write tx buf element data";

        // check sent frame & irqs
        EXPECT_OK(out.writew(REG_TXBAR, 1u)) << "cannot simluate new tx frame";

        wait(1, SC_NS);

        can_frame chk = {};
        ASSERT_TRUE(can.can_rx_pop(chk));
        EXPECT_THAT(test, CanFrameEq(chk)) << "tx can frames to not match";
        EXPECT_TRUE(irq0.read()) << "irq did not get raised";
        check_irq(IR_TEFN | IR_TC);
        EXPECT_FALSE(irq0.read()) << "irq did not get cleared";

        check_tx_evfifo(hdr);
    }

    void test_tx_fd_frame(const can_frame test) {
        const u64 addr = addr_msgram.start + TX_FIFO_START_ADDR;

        // construct tx buffer element
        u32 hdr[2] = {};
        if (test.is_eff()) {
            set_field<BUF_HDR0_ID_XTD>(hdr[0], test.msgid);
            hdr[0] |= BUF_HDR0_XTD;
        } else {
            set_field<BUF_HDR0_ID_STD>(hdr[0], test.msgid);
        }
        set_field<BUF_HDR1_DLC>(hdr[1], test.dlc);
        hdr[1] |= BUF_HDR1_FDF;
        hdr[1] |= TXBUF_T1_EFC;
        EXPECT_OK(out.writew(addr, hdr)) << "cannot write hdr";
        EXPECT_OK(out.writew(addr + TX_BUF_ELEM_HDR_SZ, test.data))
            << "cannot write data";

        // check sent frame & irqs
        EXPECT_OK(out.writew(REG_TXBAR, 1u)) << "cannot simluate new tx frame";

        wait(1, SC_NS);

        can_frame chk = {};
        ASSERT_TRUE(can.can_rx_pop(chk));
        EXPECT_THAT(test, CanFrameFDEq(chk)) << "tx can frames to not match";
        EXPECT_TRUE(irq0.read()) << "irq did not get raised";
        check_irq(IR_TEFN | IR_TC);
        EXPECT_FALSE(irq0.read()) << "irq did not get cleared";

        check_tx_evfifo(hdr);
    }

    void test_rx_frame(can_frame test) {
        // receive frame and check irqs
        can.can_out.send(test);
        wait(1, SC_NS);

        EXPECT_TRUE(irq0.read()) << "irq did not get raised";
        check_irq(IR_RF0N);
        EXPECT_FALSE(irq0.read()) << "irq did not get cleared";

        // check rx fifo0 element
        u32 rxf0s = 0;
        EXPECT_OK(out.readw(REG_RXF0S, rxf0s))
            << "cannot read rx fifo0 status";

        u64 addr = addr_msgram.start + RX_FIFO0_START_ADDR +
                   (get_field<RXFS_FGI>(rxf0s) *
                    (RX_BUF_ELEM_HDR_SZ + (DATA_FIELD_SZ * 8)));

        u32 rx_fifo0_hdr[2] = {};
        u8 rx_fifo0_data[64] = {};

        EXPECT_OK(out.readw(addr, rx_fifo0_hdr))
            << "cannot read rx fifo0 elem hdr";
        EXPECT_OK(out.readw(addr + RX_BUF_ELEM_HDR_SZ, rx_fifo0_data))
            << "cannot read rx fifo0 elem data";

        u32 buf0 = 0;
        set_bit<BUF_HDR0_RTR>(buf0, test.is_rtr());
        if (test.is_eff()) {
            set_field<BUF_HDR0_ID_XTD>(buf0, test.msgid);
            buf0 |= BUF_HDR0_XTD;
        } else {
            set_field<BUF_HDR0_ID_STD>(buf0, test.msgid);
        }

        EXPECT_EQ(buf0, rx_fifo0_hdr[0])
            << "rx fifo0 elem header not matching";
        EXPECT_EQ(test.dlc << BUF_HDR1_DLC::OFFSET, rx_fifo0_hdr[1])
            << "rx fifo0 elem header not matching";
        EXPECT_THAT(test.data, ContainerEq(rx_fifo0_data))
            << "rx fifo0 elem data not matching";

        // check rx fifo0 fill level
        EXPECT_EQ(get_field<RXFS_FFL>(rxf0s), 1)
            << "wrong rx fifo0 fill level";
        EXPECT_OK(out.writew(REG_RXF0A, get_field<RXFS_FGI>(rxf0s)))
            << "cannot ack rx fifo0 elem";
        EXPECT_OK(out.readw(REG_RXF0S, rxf0s))
            << "cannot check new rx fifo0 status";
        EXPECT_EQ(get_field<RXFS_FFL>(rxf0s), 0)
            << "wrong rx fifo0 fill level";
    }

    void test_rx_fd_frame(can_frame test) {
        // receive frame and check irqs
        can.can_out.send(test);
        wait(1, SC_NS);

        EXPECT_TRUE(irq0.read()) << "irq did not get raised";
        check_irq(IR_RF0N);
        EXPECT_FALSE(irq0.read()) << "irq did not get cleared";

        // check rx fifo0 element
        u32 rxf0s = 0;
        EXPECT_OK(out.readw(REG_RXF0S, rxf0s))
            << "cannot read rx fifo0 status";

        u64 addr = addr_msgram.start + RX_FIFO0_START_ADDR +
                   (get_field<RXFS_FGI>(rxf0s) *
                    (RX_BUF_ELEM_HDR_SZ + (DATA_FIELD_SZ * 8)));

        u32 rx_fifo0_hdr[2] = {};
        u8 rx_fifo0_data[64] = {};

        EXPECT_OK(out.readw(addr, rx_fifo0_hdr))
            << "cannot read rx fifo0 elem hdr";
        EXPECT_OK(out.readw(addr + RX_BUF_ELEM_HDR_SZ, rx_fifo0_data))
            << "cannot read rx fifo0 elem data";

        u32 buf0 = 0;
        set_bit<BUF_HDR0_RTR>(buf0, test.is_rtr());
        if (test.is_eff()) {
            set_field<BUF_HDR0_ID_XTD>(buf0, test.msgid);
            buf0 |= BUF_HDR0_XTD;
        } else {
            set_field<BUF_HDR0_ID_STD>(buf0, test.msgid);
        }

        EXPECT_EQ(buf0, rx_fifo0_hdr[0])
            << "rx fifo0 elem header not matching";
        EXPECT_EQ(test.dlc << BUF_HDR1_DLC::OFFSET | 1u << 21, rx_fifo0_hdr[1])
            << "rx fifo0 elem header not matching";
        EXPECT_THAT(test.data, ContainerEq(rx_fifo0_data))
            << "rx fifo0 elem data not matching";

        // check rx fifo0 fill level
        EXPECT_EQ(get_field<RXFS_FFL>(rxf0s), 1)
            << "wrong rx fifo0 fill level";
        EXPECT_OK(out.writew(REG_RXF0A, get_field<RXFS_FGI>(rxf0s)))
            << "cannot ack rx fifo0 elem";
        EXPECT_OK(out.readw(REG_RXF0S, rxf0s))
            << "cannot check new rx fifo0 status";
        EXPECT_EQ(get_field<RXFS_FFL>(rxf0s), 0) << "wrong rx fif0 fill level";
    }

    virtual void run_test() override {
        setup_m_can();

        // transmission tests
        test_tx_frame(generate_can_frame(false, false)); // can
        test_tx_frame(generate_can_frame(true, false));  // can + eff
        test_tx_frame(generate_can_frame(false, true));  // can + rtr
        test_tx_fd_frame(generate_canfd_frame(false));   // canfd
        test_tx_fd_frame(generate_canfd_frame(true));    // canfd + eff

        // reception tests
        test_rx_frame(generate_can_frame(false, false)); // can
        test_rx_frame(generate_can_frame(true, false));  // can + eff
        test_rx_frame(generate_can_frame(false, true));  // can + rtr
        test_rx_fd_frame(generate_canfd_frame(false));   // canfd
        test_rx_fd_frame(generate_canfd_frame(true));    // canfd + eff
    }
};

TEST(m_can, simulate) {
    m_can_bench test("m_can_bench");
    sc_core::sc_start();
}
