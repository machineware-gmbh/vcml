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

#include "vcml/common/aio.h"

namespace vcml {

    struct handler_info {
        aio_policy policy;
        aio_handler handler;
    };

    static std::map<int, handler_info> handlers;
    static struct sigaction prev_sa;

    static void aio_enable(int fd) {
        int flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETOWN, getpid());
        fcntl(fd, F_SETFL, flags | O_ASYNC);
        fcntl(fd, F_SETSIG, SIGIO);
    }

    static void aio_disable(int fd) {
        int flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETOWN, getpid());
        fcntl(fd, F_SETFL, flags & ~O_ASYNC);
    }

    static void aio_call(aio_handler handler, int fd, int event) {
        try {
            handler(fd, event);
        } catch (std::exception& ex) {
            if (fd != STDERR_FILENO)
                std::cerr << "aio exception: " << ex.what() << std::endl;
            std::abort();
        } catch (...) {
            if (fd != STDERR_FILENO)
                std::cerr << "unknown exception during aio" << std::endl;
            std::abort();
        }
    }

    static void aio_sigaction(int sig, siginfo_t* siginfo, void* context) {
        if (sig != SIGIO)
            return; // should not happen

        int fd = siginfo->si_fd;
        if (stl_contains(handlers, fd)) {
            handler_info info = handlers.at(fd);
            if (handlers[fd].policy == AIO_ONCE)
                aio_cancel(fd);
            aio_call(info.handler, fd, siginfo->si_band);
            return;
        }

        if (prev_sa.sa_flags & SA_SIGINFO) {
            (*prev_sa.sa_sigaction)(sig, siginfo, context);
            return;
        }

        void (*sa)(int) = prev_sa.sa_handler;
        if (sa != NULL && sa != SIG_DFL && sa != SIG_IGN && sa != SIG_ERR)
            (*sa)(sig);
    }

    static void aio_setup() {
        struct sigaction vcml_sa;

        memset(&prev_sa, 0, sizeof(prev_sa));
        memset(&vcml_sa, 0, sizeof(vcml_sa));

        vcml_sa.sa_sigaction = &aio_sigaction;
        vcml_sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&vcml_sa.sa_mask);

        if (sigaction(SIGIO, &vcml_sa, &prev_sa) < 0)
            VCML_ERROR("failed to install SIGIO signal handler");
    }

    void aio_notify(int fd, aio_handler handler, aio_policy policy) {
        static bool aio_setup_done = false;
        if (!aio_setup_done) {
            aio_setup();
            aio_setup_done = true;
        }

        if (fd < 0)
            VCML_ERROR("invalid aio fd %d", fd);

        if (stl_contains(handlers, fd))
            VCML_ERROR("aio handler for fd %d already installed", fd);

        aio_enable(fd);
        handlers[fd] = {
            .policy = policy,
            .handler = handler,
        };
    }

    void aio_cancel(int fd) {
        aio_disable(fd);
        handlers.erase(fd);
    }

}
