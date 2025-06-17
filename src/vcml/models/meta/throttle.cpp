/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/meta/throttle.h"

namespace vcml {
namespace meta {

static u64 do_usleep(u64 delta) {
#ifdef INSCIGHT_KTHREAD_THROTTLED
    INSCIGHT_KTHREAD_THROTTLED();
#endif
    u64 start = mwr::timestamp_us();
    mwr::usleep(delta);
    u64 end = mwr::timestamp_us();
    if (start >= end)
        return 0;
    u64 d = end - start;
#ifdef INSCIGHT_KTHREAD_UNTHROTTLED
    INSCIGHT_KTHREAD_UNTHROTTLED();
#endif
    return d > delta ? d - delta : 0;
}

static u64 do_wait(u64 delta) {
#ifdef INSCIGHT_KTHREAD_THROTTLED
    INSCIGHT_KTHREAD_THROTTLED();
#endif
    u64 start = mwr::timestamp_us();
    u64 end = mwr::timestamp_us() + delta;
    u64 t = start;
    while (t < end) {
        wait(SC_ZERO_TIME);
        t = mwr::timestamp_us();
    }

    u64 d = t - start;
#ifdef INSCIGHT_KTHREAD_UNTHROTTLED
    INSCIGHT_KTHREAD_UNTHROTTLED();
#endif
    return d > delta ? d - delta : 0;
}

void throttle::update() {
    while (true) {
        sc_time quantum = tlm::tlm_global_quantum::instance().get();
        sc_time interval = max<sc_time>(quantum, update_interval);
        wait(interval);

        if (rtf > 0.0) {
            u64 actual = mwr::timestamp_us() - m_start + m_extra;
            u64 target = time_to_us(interval) / rtf;

            if (actual < target) {
                m_extra = m_use_wait ? do_wait(target - actual)
                                     : do_usleep(target - actual);
                if (!m_throttling)
                    log_debug("throttling started");
                m_throttling = true;
            } else {
                m_extra = actual - target;
                if (m_throttling)
                    log_debug("throttling stopped");
                m_throttling = false;
            }
        }

        m_start = mwr::timestamp_us();
    }
}

throttle::throttle(const sc_module_name& nm):
    module(nm),
    m_throttling(false),
    m_use_wait(false),
    m_start(mwr::timestamp_us()),
    m_extra(0),
    method("method", "sleep"),
    update_interval("update_interval", sc_time(10.0, SC_MS)),
    rtf("rtf", 0.0) {
    if (rtf > 0.0) {
        SC_HAS_PROCESS(throttle);
        SC_THREAD(update);
    }

    if (method == "wait") {
        log_debug("using wait method");
        m_use_wait = true;
    } else if (method == "sleep") {
        log_debug("using sleep method");
        m_use_wait = false;
    } else {
        log_warn("unknown throttle method: \"%s\"", method.c_str());
        m_use_wait = false;
    }
}

void throttle::session_suspend() {
    m_start -= mwr::timestamp_us();
}

void throttle::session_resume() {
    m_start += mwr::timestamp_us();
    m_extra = 0;
}

VCML_EXPORT_MODEL(vcml::meta::throttle, name, args) {
    return new throttle(name);
}

} // namespace meta
} // namespace vcml
