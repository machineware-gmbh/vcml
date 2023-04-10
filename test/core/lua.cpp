/******************************************************************************
 *                                                                            *
 * Copyright 2023 MachineWare GmbH                                            *
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

TEST(core, lua) {
    string s;
    log_term logger;
    broker_lua lua(get_resource_path("test.lua"));

    const vector<pair<string, string>> properties = {
        { "test.property", VCML_VERSION_STRING },
        { "test.property2", "123" },
        { "data", VCML_VERSION_STRING },
        { "outer.inner.strprop", "hello" },
        { "outer.inner.floatprop", "6.4" },
        { "outer.intprop", "55" },
        { "index.property", "456" },
        { "outer.in", "4096" },
    };

    for (auto [prop, val] : properties) {
        ASSERT_TRUE(lua.lookup(prop, s)) << "property undefined: " << prop;
        EXPECT_EQ(s, val) << "property " << prop << " has wrong value";
        std::cout << prop << " = " << val << std::endl;
    }
}
