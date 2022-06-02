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

class mock_module : public vcml::module
{
public:
    bool cmd_test(const std::vector<std::string>& args, std::ostream& os) {
        for (const std::string& arg : args)
            os << arg;
        return true;
    }

    mock_module(const sc_core::sc_module_name& nm = "mock_component"):
        vcml::module(nm) {
        register_command("test", 3, &mock_module::cmd_test, "test");
    }
};

TEST(module, commands) {
    mock_module mod("mock_module");

    vcml::command_base* cmd = mod.get_command("test");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->name()), std::string("test"));
    EXPECT_EQ(cmd->argc(), 3);

    std::stringstream ss;
    EXPECT_TRUE(
        mod.execute("test", std::vector<std::string>({ "a", "b", "c" }), ss));
    EXPECT_EQ(ss.str(), "abc");

    ss.str("");
    EXPECT_FALSE(mod.execute("test", std::vector<std::string>(), ss));
    EXPECT_FALSE(ss.str().empty());
}
