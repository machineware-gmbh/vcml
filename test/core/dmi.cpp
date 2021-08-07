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
using namespace ::testing;

#include "vcml.h"

TEST(dmi, insert) {
    unsigned char dummy;
    vcml::tlm_dmi_cache cache;
    tlm::tlm_dmi dmi;

    dmi.allow_read_write();
    dmi.set_start_address(0);
    dmi.set_end_address(1000);
    dmi.set_dmi_ptr(&dummy + dmi.get_start_address());
    cache.insert(dmi);
    EXPECT_EQ(cache.get_entries().size(), 1);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 0);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 1000);

    dmi.set_start_address(900);
    dmi.set_end_address(1100);
    dmi.set_dmi_ptr(&dummy + dmi.get_start_address());
    cache.insert(dmi);
    EXPECT_EQ(cache.get_entries().size(), 1);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 0);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 1100);

    dmi.set_start_address(1200);
    dmi.set_end_address(1500);
    dmi.set_dmi_ptr(&dummy + dmi.get_start_address());
    cache.insert(dmi);
    EXPECT_EQ(cache.get_entries().size(), 2);
    EXPECT_EQ(cache.get_entries()[1].get_start_address(), 0);
    EXPECT_EQ(cache.get_entries()[1].get_end_address(), 1100);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 1200);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 1500);

    dmi.set_start_address(1000);
    dmi.set_end_address(1200);
    dmi.set_dmi_ptr(&dummy + dmi.get_start_address());
    cache.insert(dmi);
    EXPECT_EQ(cache.get_entries().size(), 1);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 0);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 1500);

    dmi.allow_read();
    cache.insert(dmi);
    EXPECT_EQ(cache.get_entries().size(), 2);
}

TEST(dmi, invalidate) {
    unsigned char dummy;
    vcml::tlm_dmi_cache cache;
    tlm::tlm_dmi dmi;

    dmi.allow_read_write();
    dmi.set_start_address(0);
    dmi.set_end_address(1000);
    dmi.set_dmi_ptr(&dummy + dmi.get_start_address());
    cache.insert(dmi);
    EXPECT_EQ(cache.get_entries().size(), 1);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 0);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 1000);

    cache.invalidate(0, 99);
    EXPECT_EQ(cache.get_entries().size(), 1);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 100);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 1000);

    cache.invalidate(900, 1000);
    EXPECT_EQ(cache.get_entries().size(), 1);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 100);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 899);

    cache.invalidate(400, 500);
    EXPECT_EQ(cache.get_entries().size(), 2);
    EXPECT_EQ(cache.get_entries()[1].get_start_address(), 100);
    EXPECT_EQ(cache.get_entries()[1].get_end_address(), 399);
    EXPECT_EQ(cache.get_entries()[0].get_start_address(), 501);
    EXPECT_EQ(cache.get_entries()[0].get_end_address(), 899);
}

TEST(dmi, lookup) {
    unsigned char dummy;
    vcml::tlm_dmi_cache cache;
    tlm::tlm_dmi dmi, dmi2;

    dmi.allow_read();
    dmi.set_start_address(100);
    dmi.set_end_address(1000);
    dmi.set_dmi_ptr(&dummy + dmi.get_start_address());
    cache.insert(dmi);

    EXPECT_TRUE(cache.lookup(200, 4, tlm::TLM_READ_COMMAND, dmi2));
    EXPECT_EQ(vcml::dmi_get_ptr(dmi2, 200), &dummy + 200);
    EXPECT_FALSE(cache.lookup(200, 4, tlm::TLM_WRITE_COMMAND, dmi2));
    EXPECT_TRUE(cache.lookup(997, 4, tlm::TLM_READ_COMMAND, dmi2));
    EXPECT_EQ(vcml::dmi_get_ptr(dmi2, 997), &dummy + 997);
    EXPECT_FALSE(cache.lookup(998, 4, tlm::TLM_READ_COMMAND, dmi2));
}
