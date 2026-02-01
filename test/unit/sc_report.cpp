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

MATCHER_P2(match, lvl, msg, "matches a log message") {
    return !arg.lines.empty() && !strcmp(msg, arg.lines[0].c_str());
}

class mock_publisher : public mwr::publisher
{
public:
    mock_publisher(): mwr::publisher(vcml::LOG_ERROR, vcml::LOG_INFO) {}
    MOCK_METHOD(void, publish, (const mwr::logmsg&), (override));
};

TEST(report, kernel_logger) {
    mock_publisher p;

    EXPECT_CALL(p, publish(match(vcml::LOG_INFO, "(example) info message")));
    SC_REPORT_INFO("example", "info message");

    EXPECT_CALL(p, publish(match(vcml::LOG_WARN, "(example) warn message")));
    SC_REPORT_WARNING("example", "warn message");

    sc_core::sc_report_handler::set_actions(
        "throw", sc_core::SC_LOG | sc_core::SC_THROW);

    EXPECT_CALL(p, publish(match(vcml::LOG_ERROR, "(throw) error message")));
    EXPECT_THROW(
        { SC_REPORT_ERROR("throw", "error message"); }, sc_core::sc_report);
}

int sc_main(int argc, char** argv) {
    ADD_FAILURE() << "sc_main should not be called";
    return EXIT_FAILURE;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    vcml::setup s(argc, argv);
    return RUN_ALL_TESTS();
}
