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

#include "vcml/core/aio.h"

#include <poll.h>

namespace vcml {

#ifdef __linux__
class aio
{
private:
    mutable mutex m_mtx;
    unordered_map<int, aio_handler> m_handlers;
    atomic<u64> m_gen;

    atomic<bool> m_running;
    thread m_thread;

    static const int TIMEOUT_MS = 10;

    void aio_thread() {
        vector<struct pollfd> polls;
        u64 curgen = 0;

        while (m_running) {
            if (curgen != m_gen) {
                polls.clear();

                lock_guard<mutex> guard(m_mtx);
                for (const auto& it : m_handlers)
                    polls.push_back({ it.first, POLLIN | POLLPRI, 0 });

                curgen = m_gen;
            }

            if (polls.empty()) {
                auto timeout = std::chrono::milliseconds(TIMEOUT_MS);
                std::this_thread::sleep_for(timeout);
                continue;
            }

            int ret = poll(polls.data(), polls.size(), TIMEOUT_MS);
            VCML_ERROR_ON(ret < 0, "aio error: %s", strerror(errno));
            vector<pair<int, aio_handler>> scheduled;

            if (ret > 0 && m_running) {
                lock_guard<mutex> guard(m_mtx);
                for (const auto& pfd : polls) {
                    if (m_handlers.count(pfd.fd) == 0)
                        continue; // fd has been removed

                    if (pfd.revents & POLLNVAL)
                        VCML_ERROR("invalid file descriptor: %d", pfd.fd);

                    if (pfd.revents & (POLLIN | POLLPRI))
                        scheduled.emplace_back(pfd.fd, m_handlers[pfd.fd]);
                }
            }

            for (const auto& handler : scheduled)
                handler.second(handler.first);
        }
    }

public:
    aio(): m_mtx(), m_handlers(), m_gen(), m_running(true), m_thread() {
        m_thread = thread(std::bind(&aio::aio_thread, this));
        set_thread_name(m_thread, "aio_thread");
    }

    virtual ~aio() {
        m_running = false;
        if (m_thread.joinable())
            m_thread.join();
    }

    void notify(int fd, aio_handler handler) {
        lock_guard<mutex> guard(m_mtx);
        m_handlers[fd] = std::move(handler);
        m_gen++;
    }

    void cancel(int fd) {
        lock_guard<mutex> guard(m_mtx);
        m_handlers.erase(fd);
        m_gen++;
    }

    static aio& instance() {
        static aio singleton;
        return singleton;
    }
};
#endif // __linux__

void aio_notify(int fd, aio_handler handler) {
    aio::instance().notify(fd, std::move(handler));
}

void aio_cancel(int fd) {
    aio::instance().cancel(fd);
}

} // namespace vcml
