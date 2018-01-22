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
#include "vcml.h"

TEST(utils, mkstr) {
    EXPECT_EQ(vcml::mkstr("%d %s", 42, "fortytwo"), "42 fortytwo");
    EXPECT_EQ(vcml::mkstr("%.9f", 1.987654321), "1.987654321");
}

TEST(utils, dirname) {
    EXPECT_EQ(vcml::dirname("/a/b/c.txt"), "/a/b");
    EXPECT_EQ(vcml::dirname("a/b/c.txt"), "a/b");
    EXPECT_EQ(vcml::dirname("/a/b/c/"), "/a/b/c");
    EXPECT_EQ(vcml::dirname("nothing"), "");
}

namespace N {

    template<typename T>
    struct A {
        struct  B {
            void func() {
                std::vector<std::string> bt = ::vcml::backtrace(1, 1);
                EXPECT_EQ(bt.size(), 1);
                EXPECT_EQ(bt[0], "N::A<int>::B::func()+0x38");
            }

            void func(T t) {
                std::vector<std::string> bt = ::vcml::backtrace(1, 1);
                EXPECT_EQ(bt.size(), 1);
                EXPECT_EQ(bt[0], "N::A<char const*>::B::func(char const*)+0x3f");
            }

            void func2() {
                std::vector<std::string> bt = ::vcml::backtrace(1, 1);
                EXPECT_EQ(bt.size(), 1);
                EXPECT_EQ(bt[0], "N::A<N::A<std::map<int, double, std::less<int>, std::allocator<std::pair<int const, double> > > > >::B::func2()+0x38");
            }
        };
    };

    struct U {
        template<int N>
        void unroll(double d) {
            unroll<N-1>(d);
        }
    };

    template<>
    void U::unroll<0>(double d) {
        const char* ref[5] = {
            "void N::U::unroll<0>(double)+0x68",
            "void N::U::unroll<1>(double)+0x2a",
            "void N::U::unroll<2>(double)+0x2a",
            "void N::U::unroll<3>(double)+0x2a",
            "void N::U::unroll<4>(double)+0x2a"
        };
        std::vector<std::string> bt = ::vcml::backtrace(5, 1);
        EXPECT_EQ(bt.size(), 5);
        for (int i = 0; i < 5; i++)
            EXPECT_EQ(bt[i], ref[i]);
    }
}

TEST(utils, backtrace) {
    N::A<int>::B().func();
    N::A<const char*>::B().func("42");
    N::A< N::A<std::map<int, double> > >::B().func2();
    N::U().unroll<5>(42.0);
}

TEST(utils, memswap) {
    vcml::u8 x8 = 0x11;
    vcml::memswap(&x8, sizeof(x8));
    EXPECT_EQ(x8, 0x11);

    vcml::u16 x16 = 0x1122;
    vcml::memswap(&x16, sizeof(x16));
    EXPECT_EQ(x16, 0x2211);

    vcml::u32 x32 = 0x11223344;
    vcml::memswap(&x32, sizeof(x32));
    EXPECT_EQ(x32, 0x44332211);

    vcml::u64 x64 = 0x1122334455667788ull;
    vcml::memswap(&x64, sizeof(x64));
    EXPECT_EQ(x64, 0x8877665544332211ull);
}

TEST(utils, split) {
    std::string s = "abc def\nghi\tjkl    :.;";
    std::vector<std::string> v = vcml::split(s, isspace);
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.at(0), "abc");
    EXPECT_EQ(v.at(1), "def");
    EXPECT_EQ(v.at(2), "ghi");
    EXPECT_EQ(v.at(3), "jkl");
    EXPECT_EQ(v.at(4), ":.;");
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
