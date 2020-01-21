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

#include "vcml/backends/backend_tap.h"

namespace vcml {

    static int g_devno = 0;

    backend_tap::backend_tap(const sc_module_name& nm, int no):
        backend(nm),
        m_fd(-1),
        devno("devno", no ? no : g_devno++) {
        m_fd = open("/dev/net/tun", O_RDWR);
        VCML_REPORT_ON(m_fd < 0, "error opening tundev: %s", strerror(errno));

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
        snprintf(ifr.ifr_name, IFNAMSIZ, "tap%d", devno.get());

        int err = ioctl(m_fd, TUNSETIFF, (void*)&ifr);
        VCML_REPORT_ON(err < 0, "error creating tapdev: %s", strerror(errno));
    }

    backend_tap::~backend_tap() {
        if (m_fd < 0)
            return;

        close(m_fd);
    }

    size_t backend_tap::peek() {
        return m_fd < 0 ? 0 : backend::peek(m_fd);
    }

    size_t backend_tap::read(void* buf, size_t len) {
        if (m_fd < 0)
            return 0;
        return ::read(m_fd, buf, len);
    }

    size_t backend_tap::write(const void* buf, size_t len) {
        if (m_fd < 0)
            return 0;
        return backend::full_write(m_fd, buf, len);
    }

    backend* backend_tap::create(const string& nm) {
        return new backend_tap(nm.c_str());
    }

}
