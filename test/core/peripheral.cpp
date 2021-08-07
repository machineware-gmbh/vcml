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
    mock_peripheral(const sc_core::sc_module_name& nm =
            sc_core::sc_gen_unique_name("mock_peripheral")):
        vcml::peripheral(nm, vcml::ENDIAN_LITTLE, 1, 10) {
        CLOCK.stub(100 * vcml::MHz);
        handle_clock_update(0, CLOCK.read());
    }

    MOCK_METHOD(tlm::tlm_response_status, read, (const vcml::range&, void*, const vcml::tlm_sbi&, vcml::address_space), (override));
    MOCK_METHOD(tlm::tlm_response_status, write, (const vcml::range&, const void*, const vcml::tlm_sbi&, vcml::address_space), (override));
};

TEST(peripheral, transporting) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.CLOCK, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[10];

    local = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, 4);

    EXPECT_CALL(mock, read(vcml::range(tx), buffer, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).WillOnce(Return(tlm::TLM_INCOMPLETE_RESPONSE));
    EXPECT_CALL(mock, write(_,_,_,_)).Times(0);
    EXPECT_EQ(mock.transport(tx, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.read_latency);

    local = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 4);

    EXPECT_CALL(mock, read(_,_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).WillOnce(Return(tlm::TLM_INCOMPLETE_RESPONSE));
    EXPECT_EQ(mock.transport(tx, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);
}

TEST(peripheral, transporting_debug) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.CLOCK, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[12];

    local = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_READ_COMMAND, 4, buffer, 16);
    EXPECT_CALL(mock, read(vcml::range(tx), buffer, vcml::SBI_DEBUG, vcml::VCML_AS_DEFAULT)).Times(1);

    EXPECT_CALL(mock, write(_,_,_,_)).Times(0);
    EXPECT_EQ(mock.transport(tx, vcml::SBI_DEBUG, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, sc_core::SC_ZERO_TIME);

    local = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 16);

    EXPECT_CALL(mock, read(_,_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::SBI_DEBUG, vcml::VCML_AS_DEFAULT));
    EXPECT_EQ(mock.transport(tx, vcml::SBI_DEBUG, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, sc_core::SC_ZERO_TIME);
}

TEST(peripheral, transport_streaming) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.CLOCK, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[10];
    int npulses;

    local = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_streaming_width(1);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(_,_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), _, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(npulses);
    EXPECT_EQ(mock.transport(tx, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(local, cycle *  mock.write_latency * npulses);

    local = sc_core::SC_ZERO_TIME;
    vcml::tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_streaming_width(2);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(vcml::range(tx), _, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(npulses);
    EXPECT_CALL(mock, write(_,_,_,_)).Times(0);
    EXPECT_EQ(mock.transport(tx, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(local, cycle *  mock.read_latency * npulses);
}

TEST(peripheral, transporting_byte_enable) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.CLOCK, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[100];

    local = sc_core::SC_ZERO_TIME;
    vcml::u8 byte_enable[4] = { 0xff, 0x00, 0xff, 0x00 };
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 8);
    tx.set_byte_enable_length(4);
    tx.set_byte_enable_ptr(byte_enable);

    EXPECT_CALL(mock, read(_,_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(4, 4), buffer + 0, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(5, 5), buffer + 1, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(6, 6), buffer + 2, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(7, 7), buffer + 3, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(8, 8), buffer + 4, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(9, 9), buffer + 5, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(10, 10), buffer + 6, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(11, 11), buffer + 7, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_EQ(mock.transport(tx, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);
}

TEST(peripheral, transporting_byte_enable_with_streaming) {
    mock_peripheral mock;
    vcml::tlm_generic_payload tx;
    sc_core::sc_time cycle(1.0 / mock.CLOCK, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    unsigned char buffer[100];
    int npulses;

    local = sc_core::SC_ZERO_TIME;
    vcml::u8 byte_enable[4] = { 0xff, 0x00, 0xff, 0x00 };
    vcml::tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, 8);
    tx.set_byte_enable_length(4);
    tx.set_byte_enable_ptr(byte_enable);
    tx.set_streaming_width(4);
    npulses = tx.get_data_length() / tx.get_streaming_width();

    EXPECT_CALL(mock, read(_,_,_,_)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(tx), buffer, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(4, 4), buffer + 0, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(4, 4), buffer + 4, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(6, 6), buffer + 2, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(6, 6), buffer + 6, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT));
    EXPECT_CALL(mock, write(vcml::range(5, 5), buffer + 1, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_CALL(mock, write(vcml::range(7, 7), buffer + 3, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT)).Times(0);
    EXPECT_EQ(mock.transport(tx, vcml::SBI_NONE, vcml::VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency * npulses);
}
