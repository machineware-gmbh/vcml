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

#include "testing.h"

TEST(utils, dirname) {
    EXPECT_EQ(vcml::dirname("/a/b/c.txt"), "/a/b");
    EXPECT_EQ(vcml::dirname("a/b/c.txt"), "a/b");
    EXPECT_EQ(vcml::dirname("/a/b/c/"), "/a/b/c");
    EXPECT_EQ(vcml::dirname("nothing"), ".");
}

namespace N {

    template<typename T>
    struct A {
        struct  B {
            void func() {
                std::vector<std::string> bt = ::vcml::backtrace(1, 1);
                EXPECT_EQ(bt.size(), 1);
                //EXPECT_TRUE(str_begins_with(bt[0], "N::A<int>::B::func()"));
            }

            void func(T t) {
                std::vector<std::string> bt = ::vcml::backtrace(1, 1);
                EXPECT_EQ(bt.size(), 1);
                //EXPECT_TRUE(str_begins_with(bt[0], "N::A<char const*>::B::func(char const*)"));
            }

            void func2() {
                std::vector<std::string> bt = ::vcml::backtrace(1, 1);
                EXPECT_EQ(bt.size(), 1);
                //EXPECT_TRUE(str_begins_with(bt[0], "N::A<N::A<std::map<int, double, std::less<int>, std::allocator<std::pair<int const, double> > > > >::B::func2()"));
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
        std::vector<std::string> bt = ::vcml::backtrace(5, 1);
        EXPECT_EQ(bt.size(), 5);
        for (auto func : bt)
            std::cout << func << std::endl;
    }
}

TEST(utils, backtrace) {
    N::A<int>::B().func();
    N::A<const char*>::B().func("42");
    N::A< N::A<std::map<int, double> > >::B().func2();
    N::U().unroll<5>(42.0);
}
