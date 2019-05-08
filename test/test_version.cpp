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

#include <iostream>
#include <cstring>
#include <gtest/gtest.h>
#include "vcml.h"

TEST(version, vstr) {
    std::cout << VCML_RELEASE_STRING << std::endl;
    std::cout << VCML_VERSION_STRING << std::endl;

    EXPECT_GT(std::strlen(VCML_RELEASE_STRING), 0);
    EXPECT_GT(std::strlen(VCML_VERSION_STRING), 0);

}
