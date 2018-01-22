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

class mock_logger: public vcml::logger
{
public:
    mock_logger(): vcml::logger(vcml::SEVERITY_ERROR, vcml::SEVERITY_INFO) {}
    MOCK_METHOD1(write_log, void(const vcml::report&));
};

TEST(logging, levels) {
    vcml::log_term cons;
    mock_logger logger;
    EXPECT_CALL(logger, write_log(_)).Times(1);
    vcml::log_info("this is an informational message");

    logger.set_level(vcml::SEVERITY_ERROR, vcml::SEVERITY_WARNING);
    cons.set_level(vcml::SEVERITY_ERROR, vcml::SEVERITY_WARNING);
    EXPECT_TRUE(vcml::logger::would_log(vcml::SEVERITY_ERROR));
    EXPECT_TRUE(vcml::logger::would_log(vcml::SEVERITY_WARNING));
    EXPECT_FALSE(vcml::logger::would_log(vcml::SEVERITY_INFO));
    EXPECT_FALSE(vcml::logger::would_log(vcml::SEVERITY_DEBUG));
    EXPECT_CALL(logger, write_log(_)).Times(0);
    vcml::log_info("this is an informational message");

    EXPECT_CALL(logger, write_log(_)).Times(2);
    vcml::log_error("this is an error message");
    vcml::log_warning("this is a warning message");

    logger.set_level(vcml::SEVERITY_DEBUG);
    cons.set_level(vcml::SEVERITY_DEBUG);
    EXPECT_FALSE(vcml::logger::would_log(vcml::SEVERITY_ERROR));
    EXPECT_FALSE(vcml::logger::would_log(vcml::SEVERITY_WARNING));
    EXPECT_FALSE(vcml::logger::would_log(vcml::SEVERITY_INFO));
    EXPECT_TRUE(vcml::logger::would_log(vcml::SEVERITY_DEBUG));
    EXPECT_CALL(logger, write_log(_)).Times(1);
    vcml::log_debug("this is a debug message");
    vcml::log_info("this is an informational message");
    vcml::log_error("this is an error message");
    vcml::log_warning("this is a warning message");
}

TEST(logging, reporting) {
    vcml::log_term cons;
    mock_logger logger;
    vcml::initialize_reporting();
    EXPECT_CALL(logger, write_log(_)).Times(2);
    VCML_INFO("this is an informational message");
    VCML_WARNING("this is a warning message");
    EXPECT_THROW(VCML_ERROR("error %s", "message!"), ::vcml::report);

    EXPECT_CALL(logger, write_log(_)).Times(1);
    for (int i = 0; i < 4; i++)
        VCML_WARNING_ONCE("this should only be shown once");
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
