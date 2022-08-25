/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "testing.h"

class test_harness : public test_base
{
public:
    vcml::tlm_initiator_socket test1_out64;
    vcml::tlm_target_socket test1_in64;
    tlm::tlm_initiator_socket<32> test1_out32;
    tlm::tlm_target_socket<32> test1_in32;

    vcml::tlm_initiator_socket test2_out64;
    tlm::tlm_initiator_socket<32> test2_out32;
    vcml::tlm_target_socket test2_in64;

    vcml::tlm_initiator_socket test3_out64;
    tlm::tlm_target_socket<32> test3_in32;
    vcml::tlm_target_socket test3_in64;

    test_harness(const sc_module_name& nm):
        test_base(nm),
        test1_out64("test1_out64"),
        test1_in64("test1_in64"),
        test1_out32("test1_out32"),
        test1_in32("test1_in32"),
        test2_out64("test2_out64"),
        test2_out32("test2_out32"),
        test2_in64("test2_in64"),
        test3_out64("test3_out64"),
        test3_in32("test3_in32"),
        test3_in64("test3_in64") {
        // test1: out64 -> out32 -> in32 -> in64
        test1_out64.bind(test1_out32);
        test1_in64.bind(test1_in32);
        test1_out32.bind(test1_in32);

        // test2: out64 -> out32 -> in64
        test2_out64.bind(test2_out32);
        test2_out32.bind(test2_in64.adapt());

        // test3: out64 -> in32 -> in64
        test3_in64.bind(test3_in32);
        test3_out64.bind(test3_in32);
    }

    unsigned int transport(tlm_generic_payload& tx, const tlm_sbi& sbi,
                           address_space as) override {
        EXPECT_TRUE(tx.is_read());
        EXPECT_EQ(tx.get_address(), 0x1234);
        EXPECT_EQ(tx.get_data_length(), 8u);

        memset(tx.get_data_ptr(), 0xff, tx.get_data_length());
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    void run_test() override {
        u64 data = 0;
        EXPECT_OK(test1_out64.readw(0x1234, data));
        EXPECT_EQ(data, ~0ull);

        data = 0;
        EXPECT_OK(test2_out64.readw(0x1234, data));
        EXPECT_EQ(data, ~0ull);

        data = 0;
        EXPECT_OK(test3_out64.readw(0x1234, data));
        EXPECT_EQ(data, ~0ull);
    }
};

TEST(generic_memory, access) {
    test_harness test("harness");
    sc_core::sc_start();
}
