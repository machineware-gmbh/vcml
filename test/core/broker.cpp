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

#define EXPECT_DEF(b, p, v)                                                \
    do {                                                                   \
        string s;                                                          \
        ASSERT_TRUE(b.lookup(p, s)) << "property '" << p << "' not found"; \
        EXPECT_EQ(s, v) << "property '" << p << "' has unexpected value";  \
    } while (0)

#define EXPECT_UDF(b, p)            \
    do {                            \
        EXPECT_FALSE(b.defines(p)); \
    } while (0)

TEST(broker, file) {
    string s;
    mwr::publishers::terminal logger;
    broker_file broker(get_resource_path("test.cfg"));

    EXPECT_DEF(broker, "a", "b");
    EXPECT_DEF(broker, "a.b", "c");
    EXPECT_UDF(broker, "test.comment");
    EXPECT_DEF(broker, "abc.def", "123");
    EXPECT_DEF(broker, "xyz", "321");
    EXPECT_DEF(broker, "test.value", "99");
    EXPECT_DEF(broker, "loop.n", "3");
    EXPECT_DEF(broker, "loop.iter0", "0");
    EXPECT_DEF(broker, "loop.iter1", "1");
    EXPECT_DEF(broker, "loop.iter2", "2");
    EXPECT_UDF(broker, "loop.iter3");
}
