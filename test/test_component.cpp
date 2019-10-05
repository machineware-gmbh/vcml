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

class mock_component: public vcml::component
{
public:
    vcml::master_socket OUT;
    vcml::slave_socket  IN;

    mock_component(const sc_core::sc_module_name& nm = "mock_component"):
        vcml::component(nm),
        OUT("OUT"),
        IN("IN") {
    }

    MOCK_METHOD3(transport, unsigned int(tlm::tlm_generic_payload&, sc_core::sc_time&, const vcml::sideband&));
};

TEST(component, sockets) {
    vcml::u32 data = 0xf3f3f3f3;
    tlm::tlm_dmi dmi;
    unsigned char* dmi_ptr = (unsigned char*)&data;

    mock_component mock1("mock1");
    mock_component mock2("mock2");

    mock1.OUT.bind(mock2.IN);
    mock2.OUT.bind(mock1.IN);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    mock2.map_dmi(dmi_ptr, 0, 3, vcml::VCML_ACCESS_READ);

    EXPECT_CALL(mock2, transport(_,_,_)).WillOnce(Return(sizeof(data)));
    mock1.OUT.readw(0, data);
    EXPECT_TRUE(mock1.OUT.dmi().lookup(0, 4, tlm::TLM_READ_COMMAND, dmi));
    EXPECT_TRUE(dmi.is_read_allowed());
    EXPECT_FALSE(dmi.is_write_allowed());
    EXPECT_FALSE(dmi.is_read_write_allowed());
    EXPECT_EQ(dmi.get_dmi_ptr(), dmi_ptr);

    EXPECT_CALL(mock1, transport(_,_,_)).WillOnce(Return(sizeof(data)));
    mock2.OUT.writew(0, data);
}
