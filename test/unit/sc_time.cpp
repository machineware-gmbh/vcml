/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <testing.h>

using sc_core::sc_time;

using sc_core::SC_ZERO_TIME;
using sc_core::SC_SEC;
using sc_core::SC_MS;
using sc_core::SC_US;
using sc_core::SC_NS;
using sc_core::SC_PS;
using sc_core::SC_FS;
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
using sc_core::SC_AS;
using sc_core::SC_ZS;
using sc_core::SC_YS;
#endif

sc_time_unit finest_resolvable_res() {
    if (!time_unit_is_resolvable(SC_MS))
        return SC_SEC;
    if (!time_unit_is_resolvable(SC_US))
        return SC_MS;
    if (!time_unit_is_resolvable(SC_NS))
        return SC_US;
    if (!time_unit_is_resolvable(SC_PS))
        return SC_NS;
    if (!time_unit_is_resolvable(SC_FS))
        return SC_PS;
#if SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
    return SC_FS;
#else
    if (!time_unit_is_resolvable(SC_AS))
        return SC_FS;
    if (!time_unit_is_resolvable(SC_ZS))
        return SC_AS;
    if (!time_unit_is_resolvable(SC_YS))
        return SC_ZS;
    return SC_YS;
#endif
}

sc_time_unit coarsest_non_resolvable_res() {
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
    if (time_unit_is_resolvable(SC_ZS))
        return SC_YS;
    if (time_unit_is_resolvable(SC_AS))
        return SC_ZS;
    if (time_unit_is_resolvable(SC_FS))
        return SC_AS;
#endif
    if (time_unit_is_resolvable(SC_PS))
        return SC_FS;
    if (time_unit_is_resolvable(SC_NS))
        return SC_PS;
    if (time_unit_is_resolvable(SC_US))
        return SC_NS;
    if (time_unit_is_resolvable(SC_MS))
        return SC_US;
    return SC_MS; // assumption: secs are always resolvable
}

TEST(sc_time, to_string) {
    sc_time fourtytwo_sec(42.0, SC_SEC);
    EXPECT_EQ(mwr::to_string(fourtytwo_sec), "42s");

    sc_time_unit finest_resolv = finest_resolvable_res();
    sc_time_unit coarsest_non_resolv = coarsest_non_resolvable_res();

    EXPECT_EQ(mwr::to_string(sc_time(1000.0, coarsest_non_resolv)),
              mwr::to_string(sc_time(1.0, finest_resolv)));
}
