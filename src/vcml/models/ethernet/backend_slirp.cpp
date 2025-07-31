/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifdef _WIN32
#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT  _WIN32_WINNT_VISTA
#endif

#include "vcml/models/ethernet/backend_slirp.h"

#ifdef MWR_MSVC
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
typedef ssize_t slirp_ssize_t;
#endif

namespace vcml {
namespace ethernet {

static struct in_addr ipaddr(const string& str) {
    struct in_addr addr;
    if (inet_pton(AF_INET, str.c_str(), &addr) != 1)
        VCML_ERROR("cannot parse ipv4 address: %s", str.c_str());
    return addr;
}

static struct in_addr ipaddr(const string& format, int val) {
    return ipaddr(mkstr(format.c_str(), val));
}

static struct in6_addr ipaddr6(const string& str) {
    struct in6_addr addr6;
    if (inet_pton(AF_INET6, str.c_str(), &addr6) != 1)
        VCML_ERROR("cannot parse ipv6 address: %s", str.c_str());
    return addr6;
}

static struct in6_addr ipaddr6(const string& format, int val) {
    return ipaddr6(mkstr(format.c_str(), val));
}

#if SLIRP_CHECK_VERSION(4, 9, 0)
static int slirp_add_poll_socket(slirp_os_socket fd, int events,
                                 void* opaque) {
#else
static int slirp_add_poll_fd(int fd, int events, void* opaque) {
#endif
    pollfd request;
    request.fd = fd;
    request.events = 0;
    request.revents = 0;

    if (events & SLIRP_POLL_IN)
        request.events |= POLLIN;
    if (events & SLIRP_POLL_OUT)
        request.events |= POLLOUT;

#ifndef MWR_MSVC
    if (events & SLIRP_POLL_PRI)
        request.events |= POLLPRI;
    if (events & SLIRP_POLL_ERR)
        request.events |= POLLERR;
    if (events & SLIRP_POLL_HUP)
        request.events |= POLLHUP;
#endif

    vector<pollfd>* requests = (vector<pollfd>*)opaque;
    requests->push_back(request);
    return requests->size() - 1;
}

static int slirp_get_events(int idx, void* opaque) {
    vector<pollfd>* requests = (vector<pollfd>*)opaque;
    int events = 0, revents = requests->at(idx).revents;
    if (revents & POLLIN)
        events |= SLIRP_POLL_IN;
    if (revents & POLLOUT)
        events |= SLIRP_POLL_OUT;
    if (revents & POLLPRI)
        events |= SLIRP_POLL_PRI;
    if (revents & POLLERR)
        events |= SLIRP_POLL_ERR;
    if (revents & POLLHUP)
        events |= SLIRP_POLL_HUP;
    return events;
}

static slirp_ssize_t slirp_send(const void* buf, size_t len, void* opaque) {
    slirp_network* network = (slirp_network*)opaque;
    network->send_packet((const u8*)buf, len);
    return (slirp_ssize_t)len;
}

static void slirp_error(const char* msg, void* opaque) {
    log_error("%s", msg);
}

static int64_t slirp_clock_ns(void* opaque) {
    return time_stamp_ns();
}

static void* slirp_timer_new(SlirpTimerCb cb, void* obj, void* opaque) {
    return new async_timer([cb, obj](async_timer& t) -> void { (*cb)(obj); });
}

static void slirp_timer_free(void* t, void* opaque) {
    if (t)
        delete (async_timer*)t;
}

static void slirp_timer_mod(void* t, int64_t expire_time, void* opaque) {
    ((async_timer*)t)->reset(expire_time, SC_MS);
}

#if SLIRP_CHECK_VERSION(4, 9, 0)
static void slirp_register_poll_socket(slirp_os_socket fd, void* opaque) {
#else
static void slirp_register_poll_fd(int fd, void* opaque) {
#endif
    // nothing to do
}

#if SLIRP_CHECK_VERSION(4, 9, 0)
static void slirp_unregister_poll_socket(slirp_os_socket fd, void* opaque) {
#else
static void slirp_unregister_poll_fd(int fd, void* opaque) {
#endif
    // nothing to do
}

static void slirp_notify(void* opaque) {
    // nothing to do
}

struct slirp_callbacks : SlirpCb {
    slirp_callbacks(): SlirpCb() {
        send_packet = slirp_send;
        guest_error = slirp_error;
        clock_get_ns = slirp_clock_ns;
        timer_new = slirp_timer_new;
        timer_free = slirp_timer_free;
        timer_mod = slirp_timer_mod;
#if !SLIRP_CHECK_VERSION(4, 9, 0)
        register_poll_fd = slirp_register_poll_fd;
        unregister_poll_fd = slirp_unregister_poll_fd;
#endif
        notify = slirp_notify;
#if SLIRP_CHECK_VERSION(4, 7, 0)
        init_completed = NULL;
        timer_new_opaque = NULL;
#if SLIRP_CHECK_VERSION(4, 9, 0)
        register_poll_socket = slirp_register_poll_socket;
        unregister_poll_socket = slirp_unregister_poll_socket;
#endif
#endif
    }

    static const SlirpCb* instance() {
        static slirp_callbacks singleton;
        return &singleton;
    }
};

void slirp_network::slirp_thread() {
    mwr::set_thread_name(mkstr("slirp_%u", m_id));

    while (m_running) {
        unsigned int timeout = 10; // ms
        vector<pollfd> fds;

        m_mtx.lock();
#if SLIRP_CHECK_VERSION(4, 9, 0)
        slirp_pollfds_fill_socket(m_slirp, &timeout, &slirp_add_poll_socket,
                                  &fds);
#else
        slirp_pollfds_fill(m_slirp, &timeout, &slirp_add_poll_fd, &fds);
#endif
        m_mtx.unlock();

        if (fds.empty()) {
            mwr::usleep(timeout * 1000);
            continue;
        }

#ifdef MWR_MSVC
        int ret = WSAPoll(fds.data(), fds.size(), timeout);
#else
        int ret = poll(fds.data(), fds.size(), timeout);
#endif

        if (ret != 0) {
            lock_guard<mutex> guard(m_mtx);
            slirp_pollfds_poll(m_slirp, ret < 0, &slirp_get_events, &fds);
        }
    }
}

static bool try_create_socket(int domain, int type, int proto) {
#ifdef MWR_MSVC
    SOCKET sock = socket(domain, type, proto);
    if (sock == INVALID_SOCKET)
        return false;
    closesocket(sock);
    return true;
#else
    int sock = socket(domain, type, proto);
    if (sock < 0)
        return false;
    close(sock);
    return true;
#endif
}

static void icmp_permissions_check_once(logger& log) {
    static bool once = false;
    if (once)
        return;

    once = true;

    if (!try_create_socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP) &&
        !try_create_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) {
        log.warn("cannot create ICMP sockets, pings will not work");
#ifdef MWR_LINUX
        log.warn("try checking /proc/sys/net/ipv4/ping_group_range");
#endif
    }
}

slirp_network::slirp_network(unsigned int id, logger& l):
    m_id(id),
    m_config(),
    m_slirp(),
    m_clients(),
    m_mtx(),
    m_running(true),
    m_thread(),
    log(l) {
#if SLIRP_CHECK_VERSION(4, 9, 0)
    m_config.version = 6;
#elif SLIRP_CHECK_VERSION(4, 7, 0)
    m_config.version = 4;
#else
    m_config.version = 1;
#endif

    m_config.in_enabled = true;
    m_config.vnetwork = ipaddr("10.0.%u.0", id);
    m_config.vnetmask = ipaddr("255.255.255.0");
    m_config.vhost = ipaddr("10.0.%u.2", id);
    m_config.vdhcp_start = ipaddr("10.0.%u.15", id);
    m_config.vnameserver = ipaddr("10.0.%u.3", id);

    m_config.in6_enabled = true;
    m_config.vprefix_addr6 = ipaddr6("%x::", 0xfec0 + id);
    m_config.vhost6 = ipaddr6("%x::2", 0xfec0 + id);
    m_config.vnameserver6 = ipaddr6("%x::3", 0xfec0 + id);
    m_config.vprefix_len = 64;

    m_config.vhostname = nullptr;
    m_config.tftp_server_name = nullptr;
    m_config.tftp_path = nullptr;
    m_config.bootfile = nullptr;
    m_config.vdnssearch = nullptr;
    m_config.vdomainname = nullptr;

    m_config.if_mtu = 0; // IF_MTU_DEFAULT
    m_config.if_mru = 0; // IF_MRU_DEFAULT
    m_config.disable_host_loopback = false;
    m_config.enable_emu = false;
    m_config.restricted = false;

    m_slirp = slirp_new(&m_config, slirp_callbacks::instance(), this);
    VCML_REPORT_ON(!m_slirp, "failed to initialize SLIRP");

    if (m_config.in_enabled)
        log_debug("created slirp ipv4 network 10.0.%u.0/24", id);
    if (m_config.in6_enabled)
        log_debug("created slirp ipv6 network %04x::", 0xfec0 + id);

    // some OSes disallow the creation of ICMP sockets from userspace, so
    // we run a quick test here and output a warning. We do this after
    // slirp_new so the whole WSAStartup inititialization on windows has
    // already been done for us by slirp.
    icmp_permissions_check_once(log);

    m_thread = thread(&slirp_network::slirp_thread, this);
}

slirp_network::~slirp_network() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();

    for (auto client : m_clients)
        client->disconnect();

    if (m_slirp) {
        for (auto& fwd : m_forwardings) {
#if SLIRP_CHECK_VERSION(4, 5, 0)
            slirp_remove_hostxfwd(m_slirp, (sockaddr*)&fwd.host,
                                  sizeof(fwd.host), fwd.flags);
#else
            slirp_remove_hostfwd(m_slirp, fwd.flags, fwd.host.sin_addr,
                                 ntohs(fwd.host.sin_port));
#endif
        }

        slirp_cleanup(m_slirp);
    }
}

void slirp_network::send_packet(const u8* ptr, size_t len) {
    eth_frame frame(ptr, len);
    for (auto client : m_clients)
        client->send_to_guest(frame);
}

void slirp_network::recv_packet(const u8* ptr, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    slirp_input(m_slirp, ptr, len);
}

void slirp_network::register_client(backend_slirp* client) {
    m_clients.insert(client);
}

void slirp_network::unregister_client(backend_slirp* client) {
    m_clients.erase(client);
}

void slirp_network::host_port_forwarding(const string& desc) {
    vector<string> args = split(desc, ':');
    if (args.size() != 4)
        VCML_ERROR("invalid port forwarding: '%s'", desc.c_str());

    const string& protocol = args[0];
    const string& guest_addr = args[1];
    u16 guest_port = from_string<u16>(args[2]);
    u16 host_port = from_string<u16>(args[3]);

    int flags = -1;
    if (protocol == "forward" || protocol == "forward-ipv4")
        flags = 0;
    else if (protocol == "forward-ipv6")
#ifdef SLIRP_HOSTFWD_V6ONLY
        flags = SLIRP_HOSTFWD_V6ONLY;
#else
        flags = 0;
#endif
    else if (protocol == "forward-udp")
#ifdef SLIRP_HOSTFWD_UDP
        flags = SLIRP_HOSTFWD_UDP;
#else
        flags = 1;
#endif
    else
        VCML_ERROR("invalid slirp protocol: %s", protocol.c_str());

    sockaddr_in guest{};
    guest.sin_family = AF_INET;
    guest.sin_port = htons(guest_port);
    if (guest_addr.empty() || guest_addr == "*")
        guest.sin_addr.s_addr = INADDR_ANY;
    else
        guest.sin_addr = ipaddr(guest_addr);

    sockaddr_in host{};
    host.sin_family = AF_INET;
    host.sin_port = htons(host_port);
    host.sin_addr.s_addr = INADDR_ANY;

#if SLIRP_CHECK_VERSION(4, 5, 0)
    int err = slirp_add_hostxfwd(m_slirp, (sockaddr*)&host, sizeof(host),
                                 (sockaddr*)&guest, sizeof(guest), flags);
#else
    int err = slirp_add_hostfwd(m_slirp, flags, host.sin_addr, host_port,
                                guest.sin_addr, guest_port);
#endif
    if (err) {
        log_warn("failed to setup slirp host port forwarding: %s (%d)",
                 strerror(errno), errno);
        return;
    }

    m_forwardings.push_back({ host, flags });
}

backend_slirp::backend_slirp(bridge* br, const shared_ptr<slirp_network>& n):
    backend(br), m_network(n) {
    VCML_ERROR_ON(!m_network, "no network");
    m_network->register_client(this);
}

backend_slirp::~backend_slirp() {
    if (m_network)
        m_network->unregister_client(this);
}

void backend_slirp::send_to_host(const eth_frame& frame) {
    if (m_network)
        m_network->recv_packet(frame.data(), frame.size());
}

void backend_slirp::handle_option(const string& option) {
    if (starts_with(option, "forward")) {
        m_network->host_port_forwarding(option);
        return;
    }

    log_warn("unknown slirp option: %s", option.c_str());
}

backend* backend_slirp::create(bridge* br, const string& type) {
    unsigned int netid = 0;
    if (sscanf(type.c_str(), "slirp:%u", &netid) != 1)
        netid = 0;

    static unordered_map<unsigned int, shared_ptr<slirp_network> > networks;
    auto& network = networks[netid];
    if (network == nullptr)
        network = std::make_shared<slirp_network>(netid, br->log);

    backend_slirp* slirp = new backend_slirp(br, network);

    auto options = split(type, ',');
    for (size_t i = 1; i < options.size(); i++)
        slirp->handle_option(options[i]);

    return slirp;
}

} // namespace ethernet
} // namespace vcml
