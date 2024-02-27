/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/backend_socket.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

namespace vcml {
namespace can {

static struct ifreq can_request(int socket, const char* ifname, int command) {
    struct ifreq request;
    memset(&request.ifr_name, 0, sizeof(request.ifr_name));
    strncpy(request.ifr_name, ifname, sizeof(request.ifr_name) - 1);
    if (ioctl(socket, command, &request) < 0)
        VCML_REPORT("host interface '%s' not available", ifname);
    return request;
}

static int can_iface_idx(int socket, const char* ifname) {
    struct ifreq req = can_request(socket, ifname, SIOCGIFINDEX);
    return req.ifr_ifindex;
}

static int can_iface_mtu(int socket, const char* ifname) {
    struct ifreq req = can_request(socket, ifname, SIOCGIFMTU);
    return req.ifr_mtu;
}

backend_socket::backend_socket(bridge* br, const string& ifname):
    backend(br), m_name(ifname), m_socket(-1) {
    m_socket = socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW);
    if (m_socket < 0)
        VCML_REPORT("error creating can socket: %s", strerror(errno));

    size_t mtu = can_iface_mtu(m_socket, m_name.c_str());
    if (mtu >= CANFD_MTU) {
        int enable = 1;
        if (setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable,
                       sizeof(enable))) {
            VCML_ERROR("error enabling canfd: %s", strerror(errno));
        }

        log_debug("using CAN-FD mode");
    }

    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = can_iface_idx(m_socket, m_name.c_str());
    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        VCML_REPORT("failed to bind %s: %s", m_name.c_str(), strerror(errno));

    mwr::aio_notify(m_socket, [=](int fd) -> void {
        can_frame frame;
        if (read(fd, &frame, sizeof(frame)) < 0) {
            log_error("error reading %s: %s", m_name.c_str(), strerror(errno));
            mwr::aio_cancel(fd);
            return;
        }

        // Linux VCAN has already translated DLC to length in bytes, undo that
        // before we forward the frame to the devices.
        frame.dlc = len2dlc(frame.dlc);

        send_to_guest(frame);
    });

    m_type = mkstr("socket:%s", ifname.c_str());
}

backend_socket::~backend_socket() {
    if (m_socket > -1) {
        mwr::aio_cancel(m_socket);
        mwr::fd_close(m_socket);
        m_socket = -1;
    }
}

void backend_socket::send_to_host(const can_frame& frame) {
    if (m_socket > -1)
        mwr::fd_write(m_socket, &frame, frame.is_fdf() ? 72 : 16);
}

backend* backend_socket::create(bridge* br, const string& type) {
    string tx = mkstr("%s.tx", br->name());
    vector<string> args = split(type, ':');
    if (args.size() > 1)
        tx = args[1];

    return new backend_socket(br, tx);
}

} // namespace can
} // namespace vcml
