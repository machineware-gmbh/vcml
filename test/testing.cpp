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
    component(nm),
    m_tracer(),
    m_logger(),
    m_reset("reset"),
    m_clock("clock", 100 * MHz) {
    m_reset.rst.bind(rst);
    m_clock.clk.bind(clk);
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

static void systemc_report_handler(const sc_report& r, const sc_actions& a) {
    // To disable a report manually during testing, use:
    //     sc_report_handler::set_actions(SC_ID_<name>, SC_DO_NOTHING);
    if (a == SC_DO_NOTHING)
        return;

    switch (r.get_severity()) {
    case SC_INFO:
        break;

    case SC_FATAL:
    case SC_ERROR:
    case SC_WARNING:
    default:
        // pass on any report we got from SystemC (warning, error, fatal)
        // to googletest; this catches SystemC things we do not want, like
        // sc_stop being called twice, duplicate module names, etc.
        ADD_FAILURE() << r.what();
        break;
    }

    ::sc_core::sc_report_handler::default_handler(r, a);
}

vector<string> args;

string get_resource_path(const string& name) {
    if (args.size() < 2) {
        ADD_FAILURE() << "test resource path information not provided";
        std::abort();
    }

    string res_dir = args[1];
    if (!file_exists(res_dir)) {
        ADD_FAILURE() << "test resource path does not exist: " << res_dir;
        std::abort();
    }

    string resource = args[1] + "/" + name;
    EXPECT_TRUE(file_exists(resource)) << "resource " << name << " not found";
    return resource;
}

extern "C" int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::vcml::report::report_segfaults();
    ::vcml::broker_arg broker(argc, argv);
    ::sc_core::sc_report_handler::set_handler(systemc_report_handler);
    for (int i = 0; i < argc; i++)
        args.push_back(argv[i]);

    return sc_core::sc_elab_and_sim(argc, argv);
}

extern "C" int sc_main(int argc, char** argv) {
    return RUN_ALL_TESTS();
}
