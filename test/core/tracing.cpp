/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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


MATCHER_P(match_trace, msg, "matches if trace message contains string") {
    if (arg.level != LOG_TRACE)
        return false;
    if (arg.lines.size() != 1)
        return false;
    return arg.lines[0].find(msg) != std::string::npos;
}

class mock_logger: public vcml::logger
{
public:
    mock_logger(): vcml::logger(vcml::LOG_ERROR, vcml::LOG_INFO) {}
    MOCK_METHOD(void, write_log, (const vcml::logmsg&), (override));
};

class test_harness: public test_base
{
public:
    vcml::log_term term;
    mock_logger mock;

    u64 addr;
    u32 data;

    tlm_initiator_socket OUT;
    tlm_target_socket IN;

    test_harness(const sc_module_name& nm):
        test_base(nm),
        term(),
        mock(),
        addr(),
        data(),
        OUT("OUT"),
        IN("IN") {
        OUT.bind(IN);
        term.set_level(LOG_TRACE);
        mock.set_level(LOG_TRACE);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
        const tlm_sbi& info, address_space as) override {
        EXPECT_EQ(tx.get_address(), addr)
            << "received wrong address";
        EXPECT_EQ(tx.get_data_length(), sizeof(data))
            << "received wrong size";
        EXPECT_EQ(*reinterpret_cast<u32*>(tx.get_data_ptr()), data)
            << "received wrong data";
        EXPECT_FALSE(info.is_debug)
            << "received debug request";

        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    virtual void run_test() override {
        loglvl = LOG_TRACE;
        addr = 0x420;
        data = 0x1234;

        vector<const char*> expected = {
            ">> WR 0x00000420 [34 12 00 00] (TLM_INCOMPLETE_RESPONSE)",
            "<< WR 0x00000420 [34 12 00 00] (TLM_OK_RESPONSE)",
        };

        for (auto trace : expected)
            EXPECT_CALL(mock, write_log(match_trace(trace))).Times(2);
        EXPECT_OK(OUT.writew(addr, data)) << "failed to send transaction";
    }
};

TEST(tracing, basic) {
    test_harness test("harness");
    sc_core::sc_start();
}


