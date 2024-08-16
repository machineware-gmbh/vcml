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

#include "vcml/protocols/serial.h"

TEST(serial, to_string) {
    serial_payload tx;
    tx.data = 'A';
    tx.mask = serial_mask(SERIAL_8_BITS);
    tx.baud = SERIAL_9600BD;
    tx.parity = SERIAL_PARITY_EVEN;
    tx.width = SERIAL_8_BITS;

    if (serial_calc_parity(tx.data & tx.mask, tx.parity))
        tx.data |= 1u << tx.width;

    std::stringstream ss;
    ss << tx;
    EXPECT_EQ(ss.str(), "SERIAL TX [41] (9600e8)");
}

TEST(serial, success) {
    serial_payload tx;
    tx.mask = serial_mask(SERIAL_8_BITS);
    tx.baud = SERIAL_9600BD;
    tx.parity = SERIAL_PARITY_EVEN;
    tx.width = SERIAL_8_BITS;
    tx.data = 'A' | 1u << tx.width;
    EXPECT_TRUE(success(tx));
    EXPECT_FALSE(failed(tx));

    tx.data = 'A';
    EXPECT_FALSE(success(tx));
    EXPECT_TRUE(failed(tx));

    tx.parity = SERIAL_PARITY_ODD;
    EXPECT_TRUE(success(tx));
    EXPECT_FALSE(failed(tx));
}

MATCHER_P(serial_match_socket, socket, "Matches a serial socket") {
    return &arg == socket;
}

MATCHER_P2(serial_match_tx, data, baud, "Matches a serial payload") {
    return (arg.data & arg.mask) == (u32)data && arg.baud == baud;
}

class serial_bench : public test_base, public serial_host
{
public:
    serial_initiator_socket serial_tx;
    serial_base_initiator_socket serial_tx_h;
    serial_base_target_socket serial_rx_h;
    serial_target_socket serial_rx;

    serial_initiator_array serial_array_tx;
    serial_target_array serial_array_rx;

    serial_bench(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
        serial_tx("serial_tx"),
        serial_tx_h("serial_tx_h"),
        serial_rx_h("serial_rx_h"),
        serial_rx("serial_rx"),
        serial_array_tx("serial_array_tx"),
        serial_array_rx("serial_array_rx") {
        serial_bind(*this, "serial_tx", *this, "serial_tx_h");
        serial_bind(*this, "serial_rx_h", *this, "serial_rx");
        serial_bind(*this, "serial_tx_h", *this, "serial_rx_h");

        serial_bind(*this, "serial_array_tx", 4, *this, "serial_array_rx", 4);
        serial_stub(*this, "serial_array_tx", 5);
        serial_stub(*this, "serial_array_rx", 6);

        // did the ports get created?
        EXPECT_TRUE(find_object("serial.serial_array_tx[4]"));
        EXPECT_TRUE(find_object("serial.serial_array_rx[4]"));
        EXPECT_TRUE(find_object("serial.serial_array_tx[5]"));
        EXPECT_TRUE(find_object("serial.serial_array_rx[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("serial.serial_array_tx[5]_stub"));
        EXPECT_TRUE(find_object("serial.serial_array_rx[6]_stub"));
    }

    MOCK_METHOD(void, serial_receive,
                (const serial_target_socket&, serial_payload&), (override));

    virtual void run_test() override {
        wait(SC_ZERO_TIME);

        EXPECT_CALL(*this,
                    serial_receive(serial_match_socket(&serial_rx),
                                   serial_match_tx('B', SERIAL_9600BD)));

        serial_tx.set_baud(SERIAL_9600BD);
        serial_tx.send('B');

        EXPECT_CALL(*this,
                    serial_receive(serial_match_socket(&serial_array_rx[4]),
                                   serial_match_tx('X', SERIAL_115200BD)));
        serial_array_tx[4].set_baud(SERIAL_115200BD);
        serial_array_tx[4].send('X');
    }
};

TEST(serial, simulate) {
    serial_bench bench("serial");
    sc_core::sc_start();
}
