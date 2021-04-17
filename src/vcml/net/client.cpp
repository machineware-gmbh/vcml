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

#include "vcml/net/client.h"
#include "vcml/net/client_file.h"
#include "vcml/net/client_tap.h"
#include "vcml/net/adapter.h"

namespace vcml { namespace net {

    client::client(const string& adapter):
        m_adapter(adapter),
        m_type("unknown") {
        auto a = adapter::find(adapter);
        VCML_ERROR_ON(!a, "network adapter not found: %s", adapter.c_str());
        a->attach(this);
    }

    client::~client() {
        adapter* a = adapter::find(m_adapter);
        if (a != nullptr)
            a->detach(this);
    }

    client* client::create(const string& adapter, const string& type) {
        string kind = type.substr(0, type.find(":"));
        typedef function<client*(const string&, const string&)> construct;
        static const unordered_map<string, construct> clients = {
            { "file", client_file::create },
            { "tap", client_tap::create },
        };

        auto it = clients.find(kind);
        if (it == clients.end()) {
            stringstream ss;
            log_warn("unknown network client '%s'", type.c_str());
            log_warn("the following client types are known:");
            for (auto avail : clients)
                log_warn("  %s", avail.first.c_str());
            return nullptr;
        }
        try
        {
            return it->second(adapter, type);
        } catch (std::exception& ex) {
            VCML_REPORT("%s: %s", type.c_str(), ex.what());
        } catch (...) {
            VCML_REPORT("%s: unknown error", type.c_str());
        }
    }

}}
