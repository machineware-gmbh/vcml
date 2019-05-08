/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

class mock_peripheral: public vcml::peripheral
{
public:
    mock_peripheral(const sc_core::sc_module_name& nm = "mock_peripheral"):
        vcml::peripheral(nm) {
    }

    MOCK_METHOD3(read, vcml::tlm_response_status(const vcml::range&, void*, int));
    MOCK_METHOD3(write, vcml::tlm_response_status(const vcml::range&, const void*, int));
};

TEST(peripheral, transporting) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time t;
    unsigned char buffer[10];

    mock.read_latency = sc_core::sc_time(1, sc_core::SC_US);
    mock.write_latency = sc_core::sc_time(10, sc_core::SC_US);

    t = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, 4);

    EXPECT_CALL(mock, read(vcml::range(tx), buffer, vcml::VCML_FLAG_NONE)).WillOnce(Return(tlm::TLM_INCOMPLETE_RESPONSE));
    EXPECT_CALL(mock, write(_,_,_)).Times(0);
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_NONE), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(t, mock.read_latency);

    t = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 4);

    EXPECT_CALL(mock, read(_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::VCML_FLAG_NONE)).WillOnce(Return(tlm::TLM_INCOMPLETE_RESPONSE));
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_NONE), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(t, mock.write_latency);
}

TEST(peripheral, transporting_debug) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time t;
    unsigned char buffer[12];

    mock.read_latency = sc_core::sc_time(1, sc_core::SC_US);
    mock.write_latency = sc_core::sc_time(10, sc_core::SC_US);

    t = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_READ_COMMAND, 4, buffer, 16);
    EXPECT_CALL(mock, read(vcml::range(tx), buffer, vcml::VCML_FLAG_DEBUG)).Times(1);

    EXPECT_CALL(mock, write(_,_,_)).Times(0);
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_DEBUG), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(t, sc_core::SC_ZERO_TIME);


    t = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 16);

    EXPECT_CALL(mock, read(_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::VCML_FLAG_DEBUG));
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_DEBUG), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(t, sc_core::SC_ZERO_TIME);
}

TEST(peripheral, transport_streaming) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time t;
    unsigned char buffer[10];
    int npulses;

    mock.read_latency = sc_core::sc_time(1, sc_core::SC_US);
    mock.write_latency = sc_core::sc_time(10, sc_core::SC_US);

    t = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_streaming_width(1);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), _, vcml::VCML_FLAG_NONE)).Times(npulses);
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_NONE), 0);
    EXPECT_EQ(t, mock.write_latency * npulses);

    t = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_streaming_width(2);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(vcml::range(tx), _, vcml::VCML_FLAG_NONE)).Times(npulses);
    EXPECT_CALL(mock, write(_,_,_)).Times(0);
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_NONE), 0);
    EXPECT_EQ(t, mock.read_latency * npulses);
}

TEST(peripheral, transporting_byte_enable) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time t;
    unsigned char buffer[100];

    mock.read_latency = sc_core::sc_time(1, sc_core::SC_US);
    mock.write_latency = sc_core::sc_time(10, sc_core::SC_US);

    t = sc_core::SC_ZERO_TIME;
    vcml::u8 byte_enable[4] = { 0xff, 0x00, 0xff, 0x00 };
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 8);
    tx.set_byte_enable_length(4);
    tx.set_byte_enable_ptr(byte_enable);

    EXPECT_CALL(mock, read(_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(4, 4), buffer + 0, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(5, 5), buffer + 1, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(6, 6), buffer + 2, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(7, 7), buffer + 3, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(8, 8), buffer + 4, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(9, 9), buffer + 5, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(10, 10), buffer + 6, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(11, 11), buffer + 7, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_NONE), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(t, mock.write_latency);
}

TEST(peripheral, transporting_byte_enable_with_streaming) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time t;
    unsigned char buffer[100];
    int npulses;

    t = sc_core::SC_ZERO_TIME;
    vcml::u8 byte_enable[4] = { 0xff, 0x00, 0xff, 0x00 };
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 8);
    tx.set_byte_enable_length(4);
    tx.set_byte_enable_ptr(byte_enable);
    tx.set_streaming_width(4);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(4, 4), buffer + 0, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(4, 4), buffer + 4, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(6, 6), buffer + 2, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(6, 6), buffer + 6, vcml::VCML_FLAG_NONE));
    EXPECT_CALL(mock, write(vcml::range(5, 5), buffer + 1, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(7, 7), buffer + 3, vcml::VCML_FLAG_NONE)).Times(0);
    EXPECT_EQ(mock.transport(tx, t, vcml::VCML_FLAG_NONE), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(t, mock.write_latency * npulses);
}
