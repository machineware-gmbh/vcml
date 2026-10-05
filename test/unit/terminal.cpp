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

class terminal_bench : public test_base, public serial_host
{
public:
    serial::terminal term;
    serial_target_socket serial_rx;

    vector<serial_payload> rx;
    vector<sc_time> rx_times;

    terminal_bench(const sc_module_name& nm):
        test_base(nm), serial_host(), term("term"), serial_rx("serial_rx") {
        term.serial_tx.bind(serial_rx);
        term.serial_rx.stub();
    }

    virtual void serial_receive(const serial_target_socket& sock,
                                serial_payload& tx) override {
        rx.push_back(tx);
        rx_times.push_back(sc_time_stamp());
    }

    string received() const {
        string s;
        for (const auto& tx : rx)
            s += (char)(tx.data & tx.mask);
        return s;
    }

    bool send(const vector<string>& args, string& resp) {
        stringstream ss;
        bool ok = term.execute("send", args, ss);
        resp = ss.str();
        return ok;
    }

    virtual void run_test() override {
        string resp;

        // invalid escape sequences are rejected and nothing gets queued
        EXPECT_FALSE(send({ "abc\\q" }, resp));
        EXPECT_FALSE(send({ "abc\\x4" }, resp));
        EXPECT_FALSE(send({ "abc\\" }, resp));
        EXPECT_EQ(term.send_pending(), 0);

        // data is queued, but only transmitted once the simulation resumes
        term.serial_tx.set_baud(SERIAL_115200BD);
        ASSERT_TRUE(send({ "hi", "\\x41\\r\\n\\\\\\0" }, resp)) << resp;
        EXPECT_EQ(resp, "queued 7 bytes");
        EXPECT_EQ(term.send_pending(), 7);
        EXPECT_TRUE(rx.empty());

        sc_time start = sc_time_stamp();
        sc_time cycle = term.serial_tx.cycle();
        wait(7 * cycle);
        EXPECT_EQ(term.send_pending(), 0);
        ASSERT_EQ(rx.size(), 7);
        EXPECT_EQ(received(), string("hiA\r\n\\\0", 7));

        // transmission must be paced according to the baud rate of serial_tx
        for (size_t i = 0; i < rx.size(); i++) {
            EXPECT_EQ(rx[i].baud, SERIAL_115200BD);
            EXPECT_EQ(rx_times[i], start + i * cycle) << "byte " << i;
        }

        // baud rate changes take effect for subsequently sent data
        rx.clear();
        rx_times.clear();
        term.serial_tx.set_baud(SERIAL_9600BD);
        ASSERT_TRUE(send({ "xy" }, resp)) << resp;
        start = sc_time_stamp();
        cycle = term.serial_tx.cycle();
        wait(2 * cycle);
        ASSERT_EQ(rx.size(), 2);
        EXPECT_EQ(received(), "xy");
        EXPECT_EQ(rx[0].baud, SERIAL_9600BD);
        EXPECT_EQ(rx_times[0], start);
        EXPECT_EQ(rx_times[1], start + cycle);
    }
};

TEST(terminal, send) {
    terminal_bench bench("bench");
    sc_core::sc_start();
}
