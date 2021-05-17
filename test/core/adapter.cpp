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

class test_harness: public test_base
{
public:
    master_socket            TEST1_OUT64;
    slave_socket             TEST1_IN64;
    tlm_initiator_socket<32> TEST1_OUT32;
    tlm_target_socket<32>    TEST1_IN32;

    master_socket            TEST2_OUT64;
    tlm_initiator_socket<32> TEST2_OUT32;
    slave_socket             TEST2_IN64;

    master_socket            TEST3_OUT64;
    tlm_target_socket<32>    TEST3_IN32;
    slave_socket             TEST3_IN64;

    test_harness(const sc_module_name& nm):
        test_base(nm),
        TEST1_OUT64("TEST1_OUT64"),
        TEST1_IN64("TEST1_IN64"),
        TEST1_OUT32("TEST1_OUT32"),
        TEST1_IN32("TEST1_IN32"),
        TEST2_OUT64("TEST2_OUT64"),
        TEST2_OUT32("TEST2_OUT32"),
        TEST2_IN64("TEST2_IN64"),
        TEST3_OUT64("TEST3_OUT64"),
        TEST3_IN32("TEST3_IN32"),
        TEST3_IN64("TEST3_IN64") {

        // test1: out64 -> out32 -> in32 -> in64
        TEST1_OUT64.bind(TEST1_OUT32);
        TEST1_IN64.bind(TEST1_IN32);
        TEST1_OUT32.bind(TEST1_IN32);

        // test2: out64 -> out32 -> in64
        TEST2_OUT64.bind(TEST2_OUT32);
        TEST2_IN64.bind(TEST2_OUT32);

        // test3: out64 -> in32 -> in64
        TEST3_IN64.bind(TEST3_IN32);
        TEST3_OUT64.bind(TEST3_IN32);
    }

    unsigned int transport(tlm_generic_payload& tx,
                           const sideband& sbi) override {
        EXPECT_TRUE(tx.is_read());
        EXPECT_EQ(tx.get_address(), 0x1234);
        EXPECT_EQ(tx.get_data_length(), 8u) << "mop";

        memset(tx.get_data_ptr(), 0xff, tx.get_data_length());
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    void run_test() override {
        u64 data = 0;
        EXPECT_OK(TEST1_OUT64.readw(0x1234, data));
        EXPECT_EQ(data, ~0ull);

        data = 0;
        EXPECT_OK(TEST2_OUT64.readw(0x1234, data));
        EXPECT_EQ(data, ~0ull);

        data = 0;
        EXPECT_OK(TEST3_OUT64.readw(0x1234, data));
        EXPECT_EQ(data, ~0ull);
    }

};

TEST(generic_memory, access) {
    test_harness test("harness");
    sc_core::sc_start();
}

