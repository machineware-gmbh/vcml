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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "vcml/net/backend_tap.h"

#define ETH_MAX_FRAME_SIZE 1522

namespace vcml {
namespace net {

static void tap_read(int fd, shared_ptr<vector<u8>>& packet) {
    size_t len;
    packet->resize(ETH_MAX_FRAME_SIZE);

    do {
        len = read(fd, packet->data(), packet->size());
    } while (len < 0 && errno == EINTR);

    packet->resize(len);
}

void backend_tap::close_tap() {
    if (m_fd >= 0) {
        aio_cancel(m_fd);
        close(m_fd);
        m_fd = -1;
    }
}

backend_tap::backend_tap(const string& adapter, int devno): backend(adapter) {
    m_fd = open("/dev/net/tun", O_RDWR);
    VCML_REPORT_ON(m_fd < 0, "error opening tundev: %s", strerror(errno));

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    snprintf(ifr.ifr_name, IFNAMSIZ, "tap%d", devno);

    int err = ioctl(m_fd, TUNSETIFF, (void*)&ifr);
    VCML_REPORT_ON(err < 0, "error creating tapdev: %s", strerror(errno));
    log_info("using tap device %s", ifr.ifr_name);

    m_type = mkstr("tap:%d", devno);

    aio_notify(m_fd, [&](int fd) -> void {
        auto packet = std::make_shared<vector<u8>>();
        tap_read(fd, packet);
        if (packet->empty()) {
            log_error("error reading tap device: %s", strerror(errno));
            close_tap();
            return;
        }

        queue_packet(packet);
    });
}

backend_tap::~backend_tap() {
    close_tap();
}

void backend_tap::send_packet(const vector<u8>& packet) {
    if (m_fd >= 0)
        fd_write(m_fd, packet.data(), packet.size());
}

backend* backend_tap::create(const string& adapter, const string& type) {
    unsigned int devno = 0;
    if (sscanf(type.c_str(), "tap:%u", &devno) != 1)
        devno = 0;
    return new backend_tap(adapter, devno);
}

} // namespace net
} // namespace vcml
