/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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

#include "vcml/models/ethernet/backend_tap.h"

namespace vcml {
namespace ethernet {

static bool tap_read(int fd, vector<u8>& buf) {
    size_t len;
    buf.resize(eth_frame::FRAME_MAX_SIZE);

    do {
        len = read(fd, buf.data(), buf.size());
    } while (len < 0 && errno == EINTR);

    if (len < 0)
        return false;

    buf.resize(len);
    return true;
}

void backend_tap::close_tap() {
    if (m_fd >= 0) {
        mwr::aio_cancel(m_fd);
        close(m_fd);
        m_fd = -1;
    }
}

backend_tap::backend_tap(bridge* br, int devno): backend(br) {
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

    mwr::aio_notify(m_fd, [&](int fd) -> void {
        eth_frame frame;
        if (!tap_read(fd, frame)) {
            log_error("error reading tap device: %s", strerror(errno));
            mwr::aio_cancel(fd);
            return;
        }

        send_to_guest(std::move(frame));
    });
}

backend_tap::~backend_tap() {
    close_tap();
}

void backend_tap::send_to_host(const eth_frame& frame) {
    if (m_fd >= 0)
        mwr::fd_write(m_fd, frame.data(), frame.size());
}

backend* backend_tap::create(bridge* br, const string& type) {
    unsigned int devno = 0;
    if (sscanf(type.c_str(), "tap:%u", &devno) != 1)
        devno = 0;
    return new backend_tap(br, devno);
}

} // namespace ethernet
} // namespace vcml
