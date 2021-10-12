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

#include "vcml/net/client_slirp.h"

namespace vcml { namespace net {

    static ssize_t slirp_receive(const void* buf, size_t len, void* opaque) {
        client_slirp* client = (client_slirp*)opaque;
        client->insert_packet((const u8*)buf, len);
        return (ssize_t)len;
    }

    static void slirp_error(const char *msg, void *opaque) {
        log_error("%s", msg);
    }

    static int64_t slirp_clock_ns(void* opaque) {
        return time_stamp_ns();
    }

    static void* slirp_timer_new(SlirpTimerCb cb, void* obj, void* opaque) {
        return new timer([cb, obj](timer& t) -> void { (*cb)(obj); });
    }

    static void slirp_timer_free(void* t, void* opaque) {
        if (t) delete (timer*)t;
    }

    static void slirp_timer_mod(void* t, int64_t expire_time, void* opaque) {
        ((timer*)t)->reset(expire_time, SC_NS);
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

    static const SlirpCb slirp_cbs = {
        /* send_packet        = */ slirp_receive,
        /* guest_error        = */ slirp_error,
        /* clock_get_ns       = */ slirp_clock_ns,
        /* timer_new          = */ slirp_timer_new,
        /* timer_free         = */ slirp_timer_free,
        /* timer_mod          = */ slirp_timer_mod,
        /* register_poll_fd   = */ slirp_register_poll_fd,
        /* unregister_poll_fd = */ slirp_unregister_poll_fd,
        /* notify             = */ slirp_notify,
    };

    client_slirp::client_slirp(const string& adapter, SlirpConfig cfg):
        client(adapter),
        m_config(std::move(cfg)),
        m_slirp(slirp_new(&m_config, &slirp_cbs, this)) {
        VCML_REPORT_ON(!m_slirp, "failed to initialize SLIRP");
    }

    client_slirp::~client_slirp() {
        if (m_slirp)
            slirp_cleanup(m_slirp);
    }

    void client_slirp::insert_packet(const u8* ptr, size_t len) {
        vector<u8> packet(ptr, ptr + len);
        m_packets.push(packet);
    }

    bool client_slirp::recv_packet(vector<u8>& packet) {
        if (m_packets.empty())
            return false;

        packet = std::move(m_packets.front());
        m_packets.pop();
        return true;
    }

    void client_slirp::send_packet(const vector<u8>& packet) {
        slirp_input(m_slirp, packet.data(), packet.size());
    }

    static SlirpConfig parse_config(const string& type) {
        SlirpConfig config = { 0 }; // ToDo: parse type, setup defaults
        return config;
    }

    client* client_slirp::create(const string& adapter, const string& type) {
        return new client_slirp(adapter, parse_config(type));
    }

}}
