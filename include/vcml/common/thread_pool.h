/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_COMMON_THREAD_POOL_H
#define VCML_COMMON_THREAD_POOL_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"
#include "vcml/common/utils.h"

namespace vcml {

    class thread_pool
    {
    private:
        const size_t m_limit;
        atomic<bool> m_exit;
        atomic<size_t> m_active;
        vector<thread> m_workers;
        queue<function<void(void)>> m_jobs;
        mutable mutex m_mutex;
        condition_variable_any m_notify;

        void spawn();
        void work(size_t id);

        thread_pool(size_t nthreads);
        ~thread_pool();

    public:
        size_t workers() const { return m_workers.size(); }
        size_t jobs() const;

        void run(function<void(void)> job);

        static thread_pool& instance();
    };

    inline size_t thread_pool::jobs() const {
        lock_guard<mutex> guard(m_mutex);
        return m_jobs.size();
    }

}

#endif
