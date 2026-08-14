/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(systemc, time) {
    sc_core::sc_set_time_resolution(1.0, SC_FS);

#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
    EXPECT_FALSE(time_unit_is_resolvable(SC_YS));
    EXPECT_EQ(time_to_ys(sc_time(1.0, SC_FS)), 1'000'000'000ull);
    EXPECT_EQ(time_to_ys(sc_time(1.0, SC_PS)), 1'000'000'000'000ull);
    EXPECT_EQ(time_to_ys(sc_time(1.0, SC_NS)), 1'000'000'000'000'000ull);
    EXPECT_EQ(time_to_ys(sc_time(1.0, SC_US)), 1'000'000'000'000'000'000ull);

    EXPECT_FALSE(time_unit_is_resolvable(SC_ZS));
    EXPECT_EQ(time_to_zs(sc_time(1.0, SC_FS)), 1'000'000ull);
    EXPECT_EQ(time_to_zs(sc_time(1.0, SC_PS)), 1'000'000'000ull);
    EXPECT_EQ(time_to_zs(sc_time(1.0, SC_NS)), 1'000'000'000'000ull);
    EXPECT_EQ(time_to_zs(sc_time(1.0, SC_US)), 1'000'000'000'000'000ull);

    EXPECT_FALSE(time_unit_is_resolvable(SC_AS));
    EXPECT_EQ(time_to_as(sc_time(1.0, SC_FS)), 1000ull);
    EXPECT_EQ(time_to_as(sc_time(1.0, SC_PS)), 1'000'000ull);
    EXPECT_EQ(time_to_as(sc_time(1.0, SC_NS)), 1'000'000'000ull);
    EXPECT_EQ(time_to_as(sc_time(1.0, SC_US)), 1'000'000'000'000ull);
    EXPECT_EQ(time_to_as(sc_time(1.0, SC_MS)), 1'000'000'000'000'000ull);
#endif

    EXPECT_TRUE(time_unit_is_resolvable(SC_FS));
    EXPECT_EQ(time_to_fs(sc_time(1.0, SC_FS)), 1ull);
    EXPECT_EQ(time_to_fs(sc_time(1.9, SC_FS)), 2ull);
    EXPECT_EQ(time_to_fs(sc_time(2.0, SC_FS)), 2ull);
    EXPECT_EQ(time_to_fs(sc_time(1.0, SC_PS)), 1000ull);
    EXPECT_EQ(time_to_fs(sc_time(1.0, SC_NS)), 1'000'000ull);
    EXPECT_EQ(time_to_fs(sc_time(1.0, SC_US)), 1'000'000'000ull);
    EXPECT_EQ(time_to_fs(sc_time(1.0, SC_MS)), 1'000'000'000'000ull);
    EXPECT_EQ(time_to_fs(sc_time(1.0, SC_SEC)), 1'000'000'000'000'000ull);

    EXPECT_TRUE(time_unit_is_resolvable(SC_PS));
    EXPECT_EQ(time_to_ps(sc_time(1.0, SC_PS)), 1ull);
    EXPECT_EQ(time_to_ps(sc_time(1.9, SC_PS)), 1ull);
    EXPECT_EQ(time_to_ps(sc_time(2.0, SC_PS)), 2ull);
    EXPECT_EQ(time_to_ps(sc_time(1.0, SC_NS)), 1000ull);
    EXPECT_EQ(time_to_ps(sc_time(1.0, SC_US)), 1'000'000ull);
    EXPECT_EQ(time_to_ps(sc_time(1.0, SC_MS)), 1'000'000'000ull);
    EXPECT_EQ(time_to_ps(sc_time(1.0, SC_SEC)), 1'000'000'000'000ull);

    EXPECT_TRUE(time_unit_is_resolvable(SC_NS));
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_NS)), 1ull);
    EXPECT_EQ(time_to_ns(sc_time(1.9, SC_NS)), 1ull);
    EXPECT_EQ(time_to_ns(sc_time(2.0, SC_NS)), 2ull);
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_US)), 1000ull);
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_MS)), 1'000'000ull);
    EXPECT_EQ(time_to_ns(sc_time(1.0, SC_SEC)), 1'000'000'000ull);

    EXPECT_TRUE(time_unit_is_resolvable(SC_US));
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_NS)), 0ull);
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_US)), 1ull);
    EXPECT_EQ(time_to_us(sc_time(1.9, SC_US)), 1ull);
    EXPECT_EQ(time_to_us(sc_time(2.0, SC_US)), 2ull);
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_MS)), 1000ull);
    EXPECT_EQ(time_to_us(sc_time(1.0, SC_SEC)), 1'000'000ull);

    EXPECT_TRUE(time_unit_is_resolvable(SC_MS));
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
    clear_timed_callbacks();
}

TEST(systemc, time_stamp) {
    sc_report_handler::set_actions(SC_ID_NO_SC_START_ACTIVITY_, SC_DO_NOTHING);

    bool checked = false;
    on_each_time_step([&checked]() {
        sc_time now = sc_time_stamp();
        EXPECT_EQ(time_stamp_ns(), time_to_ns(now));
        EXPECT_EQ(time_stamp_us(), time_to_us(now));
        EXPECT_EQ(time_stamp_ms(), time_to_ms(now));
        EXPECT_EQ(time_stamp_sec(), time_to_sec(now));
        checked = true;
    });

    sc_core::sc_start(1, SC_SEC);
    EXPECT_TRUE(checked);
    clear_timed_callbacks();
}
