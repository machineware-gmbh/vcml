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

class test_base : public component
{
private:
    tracer_term m_tracer;
    log_term m_logger;

    generic::reset m_reset;
    generic::clock m_clock;

    void run();

public:
    test_base() = delete;
    test_base(const sc_module_name& nm);
    virtual ~test_base();

    virtual void run_test() = 0;
    virtual void finalize_test();
};

extern vector<string> args;

string get_resource_path(const string& name);

#endif
