/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TESTING_H
#define VCML_TESTING_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <systemc>
#include "vcml.h"

using namespace ::testing;
using namespace ::sc_core;
using namespace ::vcml;

#define ASSERT_OK(tlmcall) ASSERT_EQ(tlmcall, TLM_OK_RESPONSE)
#define ASSERT_AE(tlmcall) ASSERT_EQ(tlmcall, TLM_ADDRESS_ERROR_RESPONSE)
#define ASSERT_CE(tlmcall) ASSERT_EQ(tlmcall, TLM_COMMAND_ERROR_RESPONSE)

#define EXPECT_OK(tlmcall) EXPECT_EQ(tlmcall, TLM_OK_RESPONSE)
#define EXPECT_AE(tlmcall) EXPECT_EQ(tlmcall, TLM_ADDRESS_ERROR_RESPONSE)
#define EXPECT_CE(tlmcall) EXPECT_EQ(tlmcall, TLM_COMMAND_ERROR_RESPONSE)

#define EXPECT_SUCCESS(fn) EXPECT_TRUE(vcml::success(fn))
#define EXPECT_FAILURE(fn) EXPECT_TRUE(vcml::failure(fn))

MATCHER_P2(match_log, lvl, txt, "Matches a log message on level and text") {
    if (arg.level != lvl)
        return false;

    for (const string& line : arg.lines)
        if (line.find(txt) != std::string::npos)
            return true;

    return false;
}

class mock_publisher : public mwr::publisher
{
public:
    mock_publisher(): mwr::publisher(LOG_ERROR, LOG_DEBUG) {}
    mock_publisher(log_level min, log_level max): mwr::publisher(min, max) {}
    MOCK_METHOD(void, publish, (const mwr::logmsg&), (override));

    void expect(log_level lvl, const string& message) {
        EXPECT_CALL(*this, publish(match_log(lvl, message)));
    }
};

class test_base : public component
{
private:
    tracer_term m_tracer;
    mwr::publishers::terminal m_publisher;

    generic::reset m_reset;
    generic::clock m_clock;

    void run();
    void finalize();

public:
    test_base() = delete;
    test_base(const sc_module_name& nm);
    virtual ~test_base();

    virtual void run_test() = 0;
};

extern vector<string> args;

string get_resource_path(const string& name);

#endif
