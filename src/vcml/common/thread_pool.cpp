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

#include "vcml/common/thread_pool.h"

namespace vcml {

    void thread_pool::spawn() {
        if (m_workers.size() == m_limit)
            return;

        size_t id = m_workers.size();
        thread th(std::bind(&thread_pool::work, this, id));
        set_thread_name(th, mkstr("vcml_worker_%zu", id));
        m_workers.push_back(std::move(th));
    }

    void thread_pool::work(size_t id) {
        while (true) {
            m_mutex.lock();

            while (m_jobs.empty() && !m_exit)
                m_notify.wait(m_mutex);

            if (m_exit) {
                m_mutex.unlock();
                return;
            }

            auto job = m_jobs.front();
            m_jobs.pop();
            m_mutex.unlock();

            m_active++;
            job();
            m_active--;
        }
    }

    thread_pool::thread_pool(size_t nthreads):
        m_limit(nthreads),
        m_exit(false),
        m_active(0),
        m_workers(),
        m_jobs(),
        m_mutex(),
        m_notify() {
        VCML_ERROR_ON(nthreads == 0, "thread count cannot be zero");
    }

    thread_pool::~thread_pool() {
        m_exit = true;
        m_notify.notify_all();

        for (auto& worker : m_workers)
            worker.detach();
    }

    void thread_pool::run(function<void(void)> job) {
        m_mutex.lock();
        m_jobs.push(job);

        if (m_workers.size() - m_active < m_jobs.size())
            spawn();

        m_mutex.unlock();
        m_notify.notify_one();
    }

    thread_pool& thread_pool::instance() {
        size_t nthreads = thread::hardware_concurrency();
        static thread_pool the_pool(nthreads);
        return the_pool;
    }

}
