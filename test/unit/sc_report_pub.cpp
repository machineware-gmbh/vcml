/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

// Global hook to forward SystemC reports to the mock function
static std::function<void(const sc_core::sc_report&,
                          const sc_core::sc_actions&)>
    g_handler;

void test_report_handler(const sc_core::sc_report& rep,
                         const sc_core::sc_actions& actions) {
    if (g_handler)
        g_handler(rep, actions);

    sc_core::sc_report_handler::default_handler(rep, actions);
}

// Matcher to verify the severity and of the report
MATCHER_P2(match_report, severity, msg,
           "Matches a SystemC report with severity and") {
    return arg.get_severity() == severity && std::string(arg.get_msg()) == msg;
}

TEST(report_publisher, publish) {
    vcml::publishers::report p;
    vcml::logger log("test-logger");
    log.set_level(vcml::LOG_DEBUG);

    sc_core::sc_report_handler::set_actions(
        log.name(), sc_core::SC_LOG | sc_core::SC_DISPLAY);

    MockFunction<void(const sc_core::sc_report&, const sc_core::sc_actions&)>
        handler;
    g_handler = handler.AsStdFunction();
    sc_core::sc_report_handler::set_handler(&test_report_handler);

    EXPECT_CALL(handler, Call(match_report(sc_core::SC_INFO, "debug"), _));
    log_debug("debug");

    EXPECT_CALL(handler, Call(match_report(sc_core::SC_INFO, "info"), _));
    log_info("info");

    EXPECT_CALL(handler, Call(match_report(sc_core::SC_WARNING, "warn"), _));
    log.warn("warn");

    EXPECT_CALL(handler, Call(match_report(sc_core::SC_ERROR, "error"), _));
    log_error("error");

    sc_core::sc_report_handler::set_handler(nullptr);
    g_handler = nullptr;
}

TEST(report_publisher, vcml_main) {
    vcml::setup s(0, nullptr);
    EXPECT_DEATH(
        { vcml::publishers::report p; }, "cannot use report publisher");
}

int sc_main(int argc, char** argv) {
    ADD_FAILURE() << "sc_main should not be called";
    return EXIT_FAILURE;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
