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

TEST(systemc, time) {
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_NS)), 1ull);
    EXPECT_EQ(time_to_ns(sc_time(1.9, SC_NS)), 1ull);
    EXPECT_EQ(time_to_ns(sc_time(2.0, SC_NS)), 2ull);
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_US)), 1000ull);
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_MS)), 1000ull * 1000ull);
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_SEC)), 1000ull * 1000ull * 1000ull);

    EXPECT_EQ(time_to_us(sc_time(1.0, SC_NS)), 0ull);
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_US)), 1ull);
    EXPECT_EQ(time_to_us(sc_time(1.9, SC_US)), 1ull);
    EXPECT_EQ(time_to_us(sc_time(2.0, SC_US)), 2ull);
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_MS)), 1000ull);
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_SEC)), 1000ull * 1000ull);

    EXPECT_EQ(time_to_ms(sc_time(1.0, SC_NS)), 0ull);
    EXPECT_EQ(time_to_ms(sc_time(1.0, SC_US)), 0ull);
    EXPECT_EQ(time_to_ms(sc_time(1.0, SC_MS)), 1ull);
    EXPECT_EQ(time_to_ms(sc_time(1.9, SC_MS)), 1ull);
    EXPECT_EQ(time_to_ms(sc_time(2.0, SC_MS)), 2ull);
    EXPECT_EQ(time_to_ms(sc_time(1.0, SC_SEC)), 1000ull);
}

TEST(systemc, callback) {
    sc_report_handler::set_actions(SC_ID_NO_SC_START_ACTIVITY_, SC_DO_NOTHING);

    unsigned int elab_calls = 0, start_calls = 0;
    on_end_of_elaboration([&elab_calls]() { elab_calls++; });
    on_start_of_simulation([&start_calls]() { start_calls++; });

    unsigned int delta_calls = 0, time_calls = 0;
    on_each_delta_cycle([&delta_calls]() { delta_calls++; });
    on_each_time_step([&time_calls]() { time_calls++; });

    delta_calls = time_calls = 0;
    sc_core::sc_start(SC_ZERO_TIME);
    EXPECT_EQ(delta_calls, 1);
#if SYSTEMC_VERSION <= SYSTEMC_VERSION_2_3_1a
    EXPECT_EQ(time_calls, 1); // SystemC <= 2.3.1a has different behavior
#else
    EXPECT_EQ(time_calls, 0);
#endif

    delta_calls = time_calls = 0;
    sc_core::sc_start(10, SC_SEC);
    EXPECT_EQ(delta_calls, 1);
    EXPECT_EQ(time_calls, 1);

    delta_calls = time_calls = 0;
    sc_core::sc_start(10, SC_SEC);
    sc_core::sc_start(SC_ZERO_TIME);
    sc_core::sc_start(10, SC_SEC);
    EXPECT_EQ(delta_calls, 3);
#if SYSTEMC_VERSION <= SYSTEMC_VERSION_2_3_1a
    EXPECT_EQ(time_calls, 3); // SystemC <= 2.3.1a has different behavior
#else
    EXPECT_EQ(time_calls, 2);
#endif

    EXPECT_EQ(elab_calls, 1);
    EXPECT_EQ(start_calls, 1);

    bool update_called = false;
    on_next_update([&]() -> void {
        EXPECT_TRUE(sc_get_curr_simcontext()->update_phase());
        update_called = true;
    });

    sc_core::sc_start(10, SC_SEC);
    EXPECT_TRUE(update_called);
}
