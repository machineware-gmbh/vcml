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

#include <systemc>
#include "vcml.h"

#define ASSERT_OK(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_OK_RESPONSE)
#define ASSERT_AE(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_ADDRESS_ERROR_RESPONSE)
#define ASSERT_CE(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_COMMAND_ERROR_RESPONSE)

using namespace ::testing;
using namespace sc_core;
using namespace vcml;

class test_component: public component
{
public:
    slave_socket IN;
    master_socket OUT;

    test_component(const sc_module_name& nm):
        component(nm),
        IN("IN"),
        OUT("OUT") {

        OUT.bind(IN);

        CLOCK.stub(100 * MHz);
        RESET.stub();

        SC_HAS_PROCESS(test_component);
        SC_THREAD(run_test);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const sideband& sbi) override {
        EXPECT_EQ(tx.get_address(), 0x0);
        EXPECT_EQ(tx.get_data_length(), 4);
        EXPECT_NE(tx.get_data_ptr(), nullptr);
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    void run_test() {
        wait(SC_ZERO_TIME);

        u32 data = 0xf3f3f3f3;
        unsigned char* dmi_ptr = (unsigned char*)&data;
        map_dmi(dmi_ptr, 0, 3, VCML_ACCESS_READ);

        ASSERT_OK(OUT.readw<u32>(0, data))
            << "component did respond to read command";

        tlm_dmi dmi; // previous read should have provided DMI access
        ASSERT_TRUE(OUT.dmi().lookup(0, 4, TLM_READ_COMMAND, dmi))
            << "component did not provide DMI mapping";
        EXPECT_TRUE(dmi.is_read_allowed())
            << "component denied previously granted DMI read access";
        EXPECT_FALSE(dmi.is_write_allowed())
            << "component granted previously denied DMI write access";
        EXPECT_FALSE(dmi.is_read_write_allowed())
            << "component grants both read- and write access";
        EXPECT_EQ(dmi.get_dmi_ptr(), dmi_ptr)
            << "component returned invalid DMI pointer";

        ASSERT_OK(OUT.writew<u32>(0, data))
            << "component did not respond to write command";

        sc_stop();
        return;
    }
};

TEST(component, sockets) {
    test_component test("component");

    sc_start();

    ASSERT_EQ(sc_get_status(), SC_STOPPED);
}
