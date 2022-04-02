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

#include "vcml/net/adapter.h"

#include "vcml/net/backend.h"
#include "vcml/net/backend_file.h"
#include "vcml/net/backend_tap.h"

#ifdef HAVE_LIBSLIRP
#include "vcml/net/backend_slirp.h"
#endif

namespace vcml {
namespace net {

backend::backend(const string& adapter):
    m_adapter(adapter), m_type("unknown") {
    auto a = adapter::find(adapter);
    VCML_ERROR_ON(!a, "network adapter not found: %s", adapter.c_str());
    a->attach(this);
}

backend::~backend() {
    adapter* a = adapter::find(m_adapter);
    if (a != nullptr)
        a->detach(this);
}

void backend::queue_packet(const shared_ptr<vector<u8>>& packet) {
    lock_guard<mutex> guard(m_packets_mtx);
    m_packets.push(packet);
}

bool backend::recv_packet(vector<u8>& packet) {
    if (m_packets.empty())
        return false;

    lock_guard<mutex> guard(m_packets_mtx);
    shared_ptr<vector<u8>> top = m_packets.front();
    m_packets.pop();

    packet = *top;
    return true;
}

void backend::send_packet(const vector<u8>& packet) {
    // to be overloaded
}

backend* backend::create(const string& adapter, const string& type) {
    string kind = type.substr(0, type.find(':'));
    typedef function<backend*(const string&, const string&)> construct;
    static const unordered_map<string, construct> backends = {
        { "file", backend_file::create },
        { "tap", backend_tap::create },
#ifdef HAVE_LIBSLIRP
        { "slirp", backend_slirp::create },
#endif
    };

    auto it = backends.find(kind);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown network backend '" << type << "'" << std::endl
           << "the following network backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(adapter, type);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", type.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", type.c_str());
    }
}

} // namespace net
} // namespace vcml
