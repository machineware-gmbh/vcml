/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

class cmd_test_module : public vcml::module
{
public:
    bool cmd_test(const std::vector<std::string>& args, std::ostream& os) {
        for (const std::string& arg : args)
            os << arg;
        return true;
    }

    cmd_test_module(const sc_core::sc_module_name& nm = "cmd_test_module"):
        vcml::module(nm) {
        register_command("test", 3, &cmd_test_module::cmd_test, "test");
    }
};

TEST(module, commands) {
    cmd_test_module mod;

    vcml::command_base* cmd = mod.get_command("test");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->name()), std::string("test"));
    EXPECT_EQ(cmd->argc(), 3);

    std::stringstream ss;
    EXPECT_TRUE(mod.execute("test", { "a", "b", "c" }, ss));
    EXPECT_EQ(ss.str(), "abc");

    ss.str("");
    EXPECT_FALSE(mod.execute("test", ss));
    EXPECT_FALSE(ss.str().empty());
}

class proc_test_module : public vcml::module
{
public:
    size_t thread_calls;
    size_t method_calls;

    void thread() {
        EXPECT_TRUE(is_local_process());
        thread_calls++;
    }

    void method() {
        EXPECT_TRUE(is_local_process());
        method_calls++;
    }

    proc_test_module(const sc_core::sc_module_name& nm = "proc_test_module"):
        vcml::module(nm), thread_calls(0), method_calls(0) {
        SC_HAS_PROCESS(proc_test_module);
        SC_THREAD(thread);
        SC_METHOD(method);
    }
};

TEST(module, local_processes) {
    proc_test_module mod;
    sc_start(1.0, sc_core::SC_SEC);
    ASSERT_EQ(mod.thread_calls, 1);
    ASSERT_EQ(mod.method_calls, 1);
}
