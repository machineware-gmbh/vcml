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

#include "vcml/common/thctl.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    static pthread_t thctl_init();

    static pthread_t g_thctl_sysc_thread = pthread_self();
    static pthread_t g_thctl_mutex_owner = thctl_init();
    static pthread_mutex_t g_thctl_mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t g_thctl_notify = PTHREAD_COND_INITIALIZER;

    static void thctl_cycle() {
        thctl_exit_critical();
        thctl_enter_critical();
    }

    pthread_t thctl_sysc_thread() {
        return g_thctl_sysc_thread;
    }

    bool thctl_is_sysc_thread() {
        return g_thctl_sysc_thread == pthread_self();
    }

    void thctl_enter_critical() {
        VCML_ERROR_ON(thctl_in_critical(),
                      "thread already in critical section");
        pthread_mutex_lock(&g_thctl_mutex);
        g_thctl_mutex_owner = pthread_self();
    }

    void thctl_exit_critical() {
        VCML_ERROR_ON(!thctl_in_critical(), "thread not in critical section");
        g_thctl_mutex_owner = 0;
        pthread_mutex_unlock(&g_thctl_mutex);
    }

    bool thctl_in_critical() {
        return g_thctl_mutex_owner == pthread_self();
    }

    void thctl_suspend() {
        VCML_ERROR_ON(!thctl_is_sysc_thread(),
                      "thctl_suspend must be called from SystemC thread");
        VCML_ERROR_ON(!thctl_in_critical(), "thread not in critical section");

        g_thctl_mutex_owner = 0;
        int res = pthread_cond_wait(&g_thctl_notify, &g_thctl_mutex);
        VCML_ERROR_ON(res != 0, "pthread_cond_wait: %s", strerror(res));
        g_thctl_mutex_owner = pthread_self();
    }

    void thctl_resume() {
        VCML_ERROR_ON(thctl_is_sysc_thread(),
                      "SystemC thread cannot resume itself");
        int res = pthread_cond_signal(&g_thctl_notify);
        VCML_ERROR_ON(res != 0, "pthread_cond_signal: %s", strerror(res));
    }

#ifdef SNPS_VP_SC_VERSION
    static void snps_enter_critical(void* unused) {
        (void)unused;
        thctl_enter_critical();
    }

    static void snps_exit_critical(void* unused) {
        (void)unused;
        thctl_exit_critical();
    }
#endif

    static pthread_t thctl_init() {
#ifdef SNPS_VP_SC_VERSION
        sc_simcontext* simc = sc_get_curr_simcontext();
        simc->add_phase_callback(snps::sc::PCB_BEGIN_OF_EVALUATE_PHASE,
                                 &snps_enter_critical);
        simc->add_phase_callback(snps::sc::PCB_END_OF_EVALUATE_PHASE,
                                 &snps_exit_critical);
        return 0;
#else

        pthread_mutex_init(&g_thctl_mutex, nullptr);
        pthread_cond_init(&g_thctl_notify, nullptr);
        pthread_mutex_lock(&g_thctl_mutex);

        on_each_delta_cycle(&thctl_cycle);

        return pthread_self();
#endif
    }

}
