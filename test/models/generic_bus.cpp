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

class mock_initiator: public vcml::component
{
public:
    vcml::master_socket OUT;

    mock_initiator(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        OUT("OUT") {
        /* nothing to do */
    }
};

TEST(generic_bus, transfer) {
    mock_initiator initiator("INITIATOR");
    vcml::generic::memory mem1("MEM1", 0x2000);
    vcml::generic::memory mem2("MEM2", 0x2000);
    vcml::generic::bus bus("BUS");

    bus.bind(initiator.OUT);
    bus.bind(mem1.IN, 0x0000, 0x1fff, 0);
    bus.bind(mem2.IN, 0x2000, 0x3fff, 0);

    vcml::clock_t clk = 100 * vcml::MHz;
    initiator.CLOCK.stub(clk);
    initiator.RESET.stub();
    mem1.CLOCK.stub(clk);
    mem1.RESET.stub();
    mem2.CLOCK.stub(clk);
    mem2.RESET.stub();
    bus.CLOCK.stub(clk);
    bus.RESET.stub();

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    EXPECT_EQ(initiator.OUT.writew(0x0000, 0x11111111ul), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.OUT.writew(0x0004, 0xfffffffful), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.OUT.writew(0x2000, 0x55555555ul), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.OUT.writew(0x2004, 0xbbbbbbbbul), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(initiator.OUT.writew(0x4000, 0x1234ul), tlm::TLM_ADDRESS_ERROR_RESPONSE);

    vcml::u32 data;
    EXPECT_EQ(initiator.OUT.readw(0x0000, data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(data, 0x11111111ul);
    EXPECT_EQ(initiator.OUT.readw(0x0004, data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(data, 0xfffffffful);
    EXPECT_EQ(initiator.OUT.readw(0x2000, data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(data, 0x55555555ul);
    EXPECT_EQ(initiator.OUT.readw(0x2004, data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(data, 0xbbbbbbbbul);
    EXPECT_EQ(initiator.OUT.readw(0x4000, data), tlm::TLM_ADDRESS_ERROR_RESPONSE);

    ASSERT_EQ(initiator.OUT.dmi().get_entries().size(), 2);
    EXPECT_NE(initiator.OUT.dmi().get_entries()[0].get_start_address(), initiator.OUT.dmi().get_entries()[1].get_start_address());
    EXPECT_NE(initiator.OUT.dmi().get_entries()[0].get_dmi_ptr(), initiator.OUT.dmi().get_entries()[1].get_dmi_ptr());

    mem1.unmap_dmi(0, 0x1fff);
    ASSERT_EQ(initiator.OUT.dmi().get_entries().size(), 1);
    EXPECT_EQ(initiator.OUT.dmi().get_entries()[0].get_start_address(), 0x2000);
}
