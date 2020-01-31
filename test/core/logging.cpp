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

TEST(logging, operators) {
    std::stringstream ss("error warning info debug trace");
    vcml::log_level lvl;

    ss >> lvl; EXPECT_EQ(lvl, vcml::LOG_ERROR);
    ss >> lvl; EXPECT_EQ(lvl, vcml::LOG_WARN);
    ss >> lvl; EXPECT_EQ(lvl, vcml::LOG_INFO);
    ss >> lvl; EXPECT_EQ(lvl, vcml::LOG_DEBUG);
    ss >> lvl; EXPECT_EQ(lvl, vcml::LOG_TRACE);
}

class mock_logger: public vcml::logger
{
public:
    mock_logger(): vcml::logger(vcml::LOG_ERROR, vcml::LOG_INFO) {}
    MOCK_METHOD2(log_line, void(vcml::log_level, const char*));
};

TEST(logging, levels) {
    vcml::log_term cons;
    mock_logger logger;
    EXPECT_CALL(logger, log_line(vcml::LOG_INFO,_)).Times(1);
    vcml::log_info("this is an informational message");

    logger.set_level(vcml::LOG_ERROR, vcml::LOG_WARN);
    cons.set_level(vcml::LOG_ERROR, vcml::LOG_WARN);
    EXPECT_TRUE(vcml::logger::would_log(vcml::LOG_ERROR));
    EXPECT_TRUE(vcml::logger::would_log(vcml::LOG_WARN));
    EXPECT_FALSE(vcml::logger::would_log(vcml::LOG_INFO));
    EXPECT_FALSE(vcml::logger::would_log(vcml::LOG_DEBUG));
    EXPECT_CALL(logger, log_line(_,_)).Times(0);
    vcml::log_info("this is an informational message");

    EXPECT_CALL(logger, log_line(_,_)).Times(2);
    vcml::log_error("this is an error message");
    vcml::log_warning("this is a warning message");

    logger.set_level(vcml::LOG_DEBUG, vcml::LOG_DEBUG);
    cons.set_level(vcml::LOG_DEBUG, vcml::LOG_DEBUG);
    EXPECT_FALSE(vcml::logger::would_log(vcml::LOG_ERROR));
    EXPECT_FALSE(vcml::logger::would_log(vcml::LOG_WARN));
    EXPECT_FALSE(vcml::logger::would_log(vcml::LOG_INFO));
    EXPECT_TRUE(vcml::logger::would_log(vcml::LOG_DEBUG));
    EXPECT_CALL(logger, log_line(vcml::LOG_DEBUG,_)).Times(1);
    vcml::log_debug("this is a debug message");
    vcml::log_info("this is an informational message");
    vcml::log_error("this is an error message");
    vcml::log_warning("this is a warning message");

    EXPECT_CALL(logger, log_line(vcml::LOG_DEBUG,_)).Times(3);
    vcml::log_debug("multi\nline\nmessage");
}

TEST(logging, component) {
    vcml::log_term cons;
    mock_logger logger;

    cons.set_level(vcml::LOG_DEBUG);
    logger.set_level(vcml::LOG_DEBUG);

    vcml::component comp("mock");
    comp.loglvl = vcml::LOG_WARN;

    EXPECT_CALL(logger, log_line(vcml::LOG_WARN,_)).Times(1);
    comp.log_warn("this is a warning message");
    comp.log_debug("this debug message should be filtered out");

    EXPECT_CALL(logger, log_line(_,_)).Times(4);
    comp.loglvl = vcml::LOG_DEBUG;
    comp.log_debug("debug message");
    comp.log_info("info message");
    comp.log_warn("warning message");
    comp.log_error("error message");
}

class mock_component: public vcml::component
{
public:
    vcml::component subcomp;

    mock_component(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        subcomp("subcomp") {
    }

    virtual ~mock_component() {}
};

TEST(logging, hierarchy) {
    vcml::log_term cons;
    mock_logger logger;

    cons.set_level(vcml::LOG_DEBUG);
    logger.set_level(vcml::LOG_DEBUG);

    vcml::property_provider prov;
    prov.add("mock.loglvl", "debug");

    mock_component comp("mock");
    EXPECT_EQ(comp.loglvl.get(), vcml::LOG_DEBUG);
    EXPECT_EQ(comp.subcomp.loglvl.get(), vcml::LOG_DEBUG);
    EXPECT_CALL(logger, log_line(vcml::LOG_DEBUG,_)).Times(2);
    comp.log_debug("top level debug message");
    comp.subcomp.log_debug("sub level debug message");

    mock_component comp2("mock2");
    EXPECT_EQ(comp2.loglvl.get(), vcml::LOG_INFO);
    EXPECT_EQ(comp2.subcomp.loglvl.get(), vcml::LOG_INFO);
}

TEST(logging, reporting) {
    vcml::log_term cons;
    mock_logger logger;

    EXPECT_CALL(logger, log_line(vcml::LOG_ERROR,_)).Times(AtLeast(4));
    vcml::report rep("This is an error message - things are really bad", __FILE__, __LINE__);
    vcml::logger::log(rep);
}
