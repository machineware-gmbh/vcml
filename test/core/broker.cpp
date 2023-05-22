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
    log_term logger;
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
