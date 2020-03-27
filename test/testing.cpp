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

test_base::test_base(const sc_module_name& nm):
    component(nm) {
    SC_HAS_PROCESS(test_base);
    SC_THREAD(run);
}

test_base::~test_base() {
    finalize_test();
}

void test_base::run() {
    wait(SC_ZERO_TIME);
    run_test();
    sc_stop();
}

void test_base::finalize_test() {
    ASSERT_EQ(sc_core::sc_get_status(), sc_core::SC_STOPPED)
        << "simulation incomplete";
}

void test_base::before_end_of_elaboration() {
    if (!CLOCK.is_bound())
        CLOCK.stub(100 * MHz);
    if (!RESET.is_bound())
        RESET.stub();
}

vector<string> args;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    for (int i = 0; i < argc; i++)
        args.push_back(argv[i]);
    return RUN_ALL_TESTS();
}

int sc_main(int argc, char** argv) {
    EXPECT_TRUE(false) << "sc_main called";
    return EXIT_FAILURE;
}
