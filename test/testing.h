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

class test_base: public component, public irq_target
{
private:
    tracer_term m_tracer;
    log_term m_logger;

    void run();

public:
    test_base() = delete;
    test_base(const sc_module_name& nm);
    virtual ~test_base();

    virtual void run_test() = 0;
    virtual void finalize_test();

protected:
    void before_end_of_elaboration() override;
    virtual void irq_transport(const irq_target_socket&, irq_payload&) override;
};

extern vector<string> args;

string get_resource_path(const string& name);

#endif
