/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "vcml.h"

using namespace ::testing;
using namespace ::vcml;

class mock_peripheral : public peripheral
{
public:
    mock_peripheral(const sc_core::sc_module_name& nm =
                        sc_core::sc_gen_unique_name("mock_peripheral")):
        peripheral(nm, ENDIAN_LITTLE, 1, 10) {
        clk.stub(100 * MHz);
        handle_clock_update(0, clk.read());
    }

    MOCK_METHOD(tlm::tlm_response_status, read,
                (const range&, void*, const tlm_sbi&, address_space),
                (override));
    MOCK_METHOD(tlm::tlm_response_status, write,
                (const range&, const void*, const tlm_sbi&, address_space),
                (override));
};

TEST(peripheral, transporting) {
    mock_peripheral mock;
    tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[10];

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, 4);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, read(range(tx), buffer, SBI_NONE, VCML_AS_DEFAULT))
        .WillOnce(Return(tlm::TLM_INCOMPLETE_RESPONSE));
    EXPECT_CALL(mock, write(_, _, _, _)).Times(0); // NOLINT
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.read_latency);

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 4);

    EXPECT_CALL(mock, read(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, write(range(tx), buffer, SBI_NONE, VCML_AS_DEFAULT))
        .WillOnce(Return(tlm::TLM_INCOMPLETE_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);
}

TEST(peripheral, transporting_debug) {
    mock_peripheral mock;
    tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[12];

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 4, buffer, 16);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, read(range(tx), buffer, SBI_DEBUG, VCML_AS_DEFAULT))
        .Times(1);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(_, _, _, _)).Times(0);
    EXPECT_EQ(mock.transport(tx, SBI_DEBUG, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, sc_core::SC_ZERO_TIME);

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 16);

    EXPECT_CALL(mock, read(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, write(range(tx), buffer, SBI_DEBUG, VCML_AS_DEFAULT));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(mock.transport(tx, SBI_DEBUG, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, sc_core::SC_ZERO_TIME);
}

TEST(peripheral, transport_streaming) {
    mock_peripheral mock;
    tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[10];
    int npulses;

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_streaming_width(1);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(_, _, _, _)).Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(tx), _, SBI_NONE, VCML_AS_DEFAULT))
        .Times(npulses)
        .WillRepeatedly(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(buffer));
    EXPECT_EQ(local, cycle * mock.write_latency * npulses);

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_streaming_width(2);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(range(tx), _, SBI_NONE, VCML_AS_DEFAULT))
        .Times(npulses)
        .WillRepeatedly(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(_, _, _, _)).Times(0);
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(buffer));
    EXPECT_EQ(local, cycle * mock.read_latency * npulses);
}

TEST(peripheral, transporting_byte_enable) {
    mock_peripheral mock;
    tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buf[100];
    u8 byte_enable[4] = { 0xff, 0x00, 0xff, 0x00 };

    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buf, 8);
    tx.set_byte_enable_length(4);
    tx.set_byte_enable_ptr(byte_enable);
    local = sc_core::SC_ZERO_TIME;

    EXPECT_CALL(mock, read(_, _, _, _)).Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(tx), buf, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(4, 4), buf + 0, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(5, 5), buf + 1, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(6, 6), buf + 2, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(7, 7), buf + 3, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(8, 8), buf + 4, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(9, 9), buf + 5, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(10, 10), buf + 6, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(11, 11), buf + 7, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(tx.get_response_status(), TLM_OK_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);
}

TEST(peripheral, transporting_byte_enable_with_streaming) {
    mock_peripheral mock;
    tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buf[100];
    int npulses;

    u8 byte_enable[4] = { 0xff, 0x00, 0xff, 0x00 };
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buf, 8);
    tx.set_byte_enable_length(4);
    tx.set_byte_enable_ptr(byte_enable);
    tx.set_streaming_width(4);

    local = sc_core::SC_ZERO_TIME;
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(_, _, _, _)).Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(tx), buf, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(4, 4), buf + 0, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(4, 4), buf + 4, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(6, 6), buf + 2, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(6, 6), buf + 6, SBI_NONE, VCML_AS_DEFAULT))
        .Times(1)
        .WillOnce(Return(TLM_OK_RESPONSE));

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(5, 5), buf + 1, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_CALL(mock, write(range(7, 7), buf + 3, SBI_NONE, VCML_AS_DEFAULT))
        .Times(0);

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency * npulses);
}
