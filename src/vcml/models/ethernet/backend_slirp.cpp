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

static int slirp_add_poll_fd(int fd, int events, void* opaque) {
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

static void slirp_register_poll_fd(int fd, void* opaque) {
    // nothing to do
}

static void slirp_unregister_poll_fd(int fd, void* opaque) {
    // nothing to do
}

static void slirp_notify(void* opaque) {
    // nothing to do
}

static const SlirpCb SLIRP_CBS = {
    /* send_packet        = */ slirp_send,
    /* guest_error        = */ slirp_error,
    /* clock_get_ns       = */ slirp_clock_ns,
    /* timer_new          = */ slirp_timer_new,
    /* timer_free         = */ slirp_timer_free,
    /* timer_mod          = */ slirp_timer_mod,
    /* register_poll_fd   = */ slirp_register_poll_fd,
    /* unregister_poll_fd = */ slirp_unregister_poll_fd,
    /* notify             = */ slirp_notify,
};

void slirp_network::slirp_thread() {
    mwr::set_thread_name(mkstr("slirp_%u", m_id));

    while (m_running) {
        unsigned int timeout = 10; // ms
        vector<pollfd> fds;

        m_mtx.lock();
        slirp_pollfds_fill(m_slirp, &timeout, &slirp_add_poll_fd, &fds);
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

slirp_network::slirp_network(unsigned int id):
    m_id(id),
    m_config(),
    m_slirp(),
    m_clients(),
    m_mtx(),
    m_running(true),
    m_thread() {
    m_config.version = 1;

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

    m_slirp = slirp_new(&m_config, &SLIRP_CBS, this);
    VCML_REPORT_ON(!m_slirp, "failed to initialize SLIRP");

    if (m_config.in_enabled)
        log_debug("created slirp ipv4 network 10.0.%u.0/24", id);
    if (m_config.in6_enabled)
        log_debug("created slirp ipv6 network %04x::", 0xfec0 + id);

    m_thread = thread(&slirp_network::slirp_thread, this);
}

slirp_network::~slirp_network() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();

    for (auto client : m_clients)
        client->disconnect();

    if (m_slirp)
        slirp_cleanup(m_slirp);
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

backend* backend_slirp::create(bridge* br, const string& type) {
    unsigned int netid = 0;
    if (sscanf(type.c_str(), "slirp:%u", &netid) != 1)
        netid = 0;

    static unordered_map<unsigned int, shared_ptr<slirp_network>> networks;
    auto& network = networks[netid];
    if (network == nullptr)
        network = std::make_shared<slirp_network>(netid);
    return new backend_slirp(br, network);
}

} // namespace ethernet
} // namespace vcml
