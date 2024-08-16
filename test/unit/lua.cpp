/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(core, lua) {
    string s;
    mwr::publishers::terminal logger;
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

    for (auto& [prop, val] : properties) {
        ASSERT_TRUE(lua.lookup(prop, s)) << "property undefined: " << prop;
        EXPECT_EQ(s, val) << "property " << prop << " has wrong value";
        std::cout << prop << " = " << val << std::endl;
    }
}
