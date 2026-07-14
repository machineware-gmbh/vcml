/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
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

union linux_can_frame {
    ::can_frame cc;
    ::canfd_frame fd;
#ifdef CANXL_MTU
    ::canxl_frame xl;
#endif
};

enum can_constants : size_t {
    CAN_CC_MTU = sizeof(::can_frame),
    CAN_FD_MTU = sizeof(::canfd_frame),
#ifdef CANXL_MTU
    CAN_XL_MTU = sizeof(::canxl_frame),
    CAN_XL_HDR_SIZE = offsetof(struct ::canxl_frame, data),
    CAN_XL_MIN_MTU = CAN_XL_HDR_SIZE + 64,
#endif
};

backend_socket::backend_socket(bridge* br, const string& ifname):
    backend(br),
    m_name(ifname),
    m_socket(-1),
    m_canfd(false),
    m_canxl(false),
    m_mtu(0) {
    m_socket = socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW);
    if (m_socket < 0)
        VCML_REPORT("error creating can socket: %s", strerror(errno));

    size_t mtu = can_iface_mtu(m_socket, m_name.c_str());
    m_mtu = mtu;
    if (mtu >= CAN_FD_MTU) {
        int enable = 1;
        if (setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable,
                       sizeof(enable))) {
            VCML_ERROR("error enabling canfd: %s", strerror(errno));
        }

        m_canfd = true;
        log_debug("using CAN-FD mode");
    }

#ifdef CANXL_MTU
    if (mtu >= CAN_XL_MIN_MTU) {
        int enable = 1;
        if (setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_XL_FRAMES, &enable,
                       sizeof(enable))) {
            VCML_ERROR("error enabling canxl: %s", strerror(errno));
        }

#ifdef CANXL_VCID_OFFSET
        struct can_raw_vcid_options vcid_opts {};

        vcid_opts.flags = CAN_RAW_XL_VCID_TX_PASS;
        if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_XL_VCID_OPTS, &vcid_opts,
                       sizeof(vcid_opts))) {
            VCML_ERROR("error when enabling CAN XL VCID pass through\n");
            return 1;
        }
#endif

        m_canxl = true;
        log_debug("using CAN-XL mode");
    }

#ifdef CANXL_VCID_OFFSET
    using CAN_XL_VCID_MASK = field<8, CANXL_VCID_OFFSET, u32>;
#endif
#endif

    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = can_iface_idx(m_socket, m_name.c_str());
    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        VCML_REPORT("failed to bind %s: %s", m_name.c_str(), strerror(errno));

    mwr::aio_notify(m_socket, [this](int fd) -> void {
        auto rec_frame = std::make_unique<linux_can_frame>();
        ssize_t n = read(fd, rec_frame.get(), sizeof(linux_can_frame));

        if (n < 0) {
            log_error("error reading %s: %s", m_name.c_str(), strerror(errno));
            mwr::aio_cancel(fd);
            return;
        }

        can_frame frame{};

#ifdef CANXL_MTU
        if ((rec_frame->xl.flags & CANXL_XLF)) {
            if (static_cast<size_t>(n) <= CAN_XL_HDR_SIZE)
                log_error("received a too small CAN XL frame");

            auto& xl_frame = rec_frame->xl;
            frame.msgid = xl_frame.prio & mwr::bitmask(11);
            frame.sdt = xl_frame.sdt;
            frame.af = xl_frame.af;
            frame.flags = CAN_FD_FDF | CAN_XL_XLF;
            frame.data.resize(xl_frame.len);

            if (xl_frame.flags & CANXL_SEC)
                frame.flags |= CAN_XL_SEC;

#ifdef CANXL_RRS
            if (xl_frame.flags & CANXL_RRS)
                frame->flags |= CAN_XL_RRS;
#endif

#ifdef CANXL_VCID_OFFSET
            frame->set_vcid(get_field<CAN_XL_VCID_MASK>(xl_frame.prio));
#endif

            if (static_cast<size_t>(n) != (CAN_XL_HDR_SIZE + xl_frame.len)) {
                log_error("mtu size does not match data length");
                return;
            }

            memcpy(frame.data.data(), xl_frame.data, xl_frame.len);
        } else
#endif
            if (static_cast<size_t>(n) == CAN_CC_MTU ||
                static_cast<size_t>(n) == CAN_FD_MTU) {
            frame.msgid = rec_frame->fd.can_id;
            frame.flags = 0;
            frame.data.resize(rec_frame->fd.len);

#ifdef CANFD_FDF
            if (rec_frame->fd.flags & CANFD_FDF)
                frame.flags |= CANFD_FDF;
#else
            if (n == CAN_FD_MTU)
                frame.flags |= CAN_FD_FDF;
#endif

            if (frame.is_canfd()) {
                if (rec_frame->fd.flags & CANFD_BRS)
                    frame.flags |= CANFD_BRS;

                if (rec_frame->fd.flags & CANFD_ESI)
                    frame.flags |= CANFD_ESI;
            }

            memcpy(frame.data.data(), rec_frame->fd.data, rec_frame->fd.len);
        } else {
            log_warn("received invalid frame of %zu bytes on %s", n,
                     m_name.c_str());
            return;
        }

        send_to_guest(std::move(frame));
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

void backend_socket::send_to_host_cc(const can_frame& frame) {
    if (!frame.is_cancc())
        return;

    ::can_frame canraw{};
    canraw.can_id = frame.msgid;
    canraw.can_dlc = min(frame.length(), sizeof(canraw.data));
    memcpy(canraw.data, frame.data.data(), canraw.can_dlc);

    size_t ret = mwr::fd_write(m_socket, &canraw, sizeof(canraw));
    if (ret != sizeof(canraw))
        log_error("failed to send CAN FD frame: %s", strerror(errno));
}

void backend_socket::send_to_host_fd(const can_frame& frame) {
    if (!frame.is_canfd())
        return;

    if (!m_canfd) {
        log_warn("socket does not support CAN FD, dropping frame");
        return;
    }

    ::canfd_frame canfdraw{};
    canfdraw.can_id = frame.msgid;
    canfdraw.len = min(frame.length(), sizeof(canfdraw.data));
    canfdraw.flags = 0;

    if (frame.is_brs())
        canfdraw.flags |= CANFD_BRS;

    if (frame.is_esi())
        canfdraw.flags |= CANFD_ESI;

#ifdef CANFD_FDF
    if (frame.is_fdf())
        canfdraw.flags |= CANFD_FDF;
#endif

    memcpy(canfdraw.data, frame.data.data(), canfdraw.len);

    size_t ret = mwr::fd_write(m_socket, &canfdraw, sizeof(canfdraw));
    if (ret != sizeof(canfdraw))
        log_error("failed to send CAN FD frame: %s", strerror(errno));
}

void backend_socket::send_to_host_xl(const can_frame& frame) {
    if (!frame.is_canxl())
        return;

    if (!m_canxl) {
        log_warn("socket does not support CAN XL, dropping frame");
        return;
    }

#ifdef CANXL_MTU
    size_t len = frame.length();
    size_t mtu = CAN_XL_HDR_SIZE + len;
    if (mtu > m_mtu) {
        log_error("MTU size too large, dropping frame");
        return;
    }

    auto xl_frame = std::make_unique<::canxl_frame>();
    xl_frame->prio = frame.id();

#ifdef CANXL_VCID_OFFSET
    xl_frame->prio |= CAN_XL_VCID_MASK::set(frame.vcid());
#endif
    xl_frame->sdt = frame.sdt;
    xl_frame->len = len;
    xl_frame->af = frame.af;
    xl_frame->flags = 0;

    if (frame.flags & CAN_XL_XLF)
        xl_frame->flags |= CANXL_XLF;

    if (frame.flags & CAN_XL_SEC)
        xl_frame->flags |= CANXL_SEC;

#ifdef CANXL_RRS
    if (frame.flags & CAN_XL_RRS)
        xl_frame->flags |= CANXL_RRS;
#endif

    memcpy(xl_frame->data, frame.data.data(), xl_frame->len);

    if (mwr::fd_write(m_socket, xl_frame.get(), mtu) != mtu)
        log_error("failed to send CAN XL frame: %s", strerror(errno));
#endif
}

void backend_socket::send_to_host(const can_frame& frame) {
    if (m_socket < 0)
        return;

    if (frame.is_canxl())
        send_to_host_xl(frame);
    else if (frame.is_canfd())
        send_to_host_fd(frame);
    else
        send_to_host_cc(frame);
}

backend* backend_socket::create(bridge* br, const vector<string>& args) {
    string tx = mkstr("%s.tx", br->name());
    if (args.size() > 0)
        tx = args[0];

    return new backend_socket(br, tx);
}

} // namespace can
} // namespace vcml
