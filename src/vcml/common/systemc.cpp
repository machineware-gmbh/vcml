/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/common/systemc.h"
#include "vcml/common/thctl.h"

namespace vcml {

    sc_object* find_object(const string& name) {
        return sc_core::sc_find_object(name.c_str());
    }

    sc_attr_base* find_attribute(const string& name) {
        size_t pos = name.rfind(SC_HIERARCHY_CHAR);
        if (pos == string::npos)
            return NULL;

        sc_object* parent = find_object(name.substr(0, pos));
        if (parent == nullptr)
            return nullptr;

        return parent->get_attribute(name);
    }

    bool is_thread(sc_process_b* proc) {
        if (proc == nullptr)
            proc = sc_get_current_process_b();
        if (proc == nullptr)
            return false;
        return proc->proc_kind() == sc_core::SC_THREAD_PROC_;
    }

    bool is_method(sc_process_b* proc) {
        if (proc == nullptr)
            proc = sc_get_current_process_b();
        if (proc == nullptr)
            return false;
        return proc->proc_kind() == sc_core::SC_METHOD_PROC_;
    }

    sc_process_b* current_thread() {
        sc_process_b* proc = sc_get_current_process_b();
        if (proc == nullptr || proc->proc_kind() != sc_core::SC_THREAD_PROC_)
            return nullptr;
        return proc;
    }

    sc_process_b* current_method() {
        sc_process_b* proc = sc_get_current_process_b();
        if (proc == nullptr || proc->proc_kind() != sc_core::SC_METHOD_PROC_)
            return nullptr;
        return proc;
    }

    const char* tlm_response_to_str(tlm_response_status status) {
        switch (status) {
        case TLM_OK_RESPONSE:
            return "TLM_OK_RESPONSE";

        case TLM_INCOMPLETE_RESPONSE:
            return "TLM_INCOMPLETE_RESPONSE";

        case TLM_GENERIC_ERROR_RESPONSE:
            return "TLM_GENERIC_ERROR_RESPONSE";

        case TLM_ADDRESS_ERROR_RESPONSE:
            return "TLM_ADDRESS_ERROR_RESPONSE";

        case TLM_COMMAND_ERROR_RESPONSE:
            return "TLM_COMMAND_ERROR_RESPONSE";

        case TLM_BURST_ERROR_RESPONSE:
            return "TLM_BURST_ERROR_RESPONSE";

        case TLM_BYTE_ENABLE_ERROR_RESPONSE:
            return "TLM_BYTE_ENABLE_ERROR_RESPONSE";

        default:
            return "TLM_UNKNOWN_RESPONSE";
        }
    }

    string tlm_transaction_to_str(const tlm_generic_payload& tx) {
        stringstream ss;

        // command
        switch (tx.get_command()) {
        case TLM_READ_COMMAND  : ss << "RD "; break;
        case TLM_WRITE_COMMAND : ss << "WR "; break;
        default: ss << "IG "; break;
        }

        // address
        ss << "0x";
        ss << std::hex;
        ss.width(16);
        ss.fill('0');
        ss << tx.get_address();

        // data array
        unsigned int size = tx.get_data_length();
        unsigned char* c = tx.get_data_ptr();

        ss << " [";
        if (size == 0)
            ss << "<no data>";
        for (unsigned int i = 0; i < size; i++) {
            ss.width(2);
            ss.fill('0');
            ss << static_cast<unsigned int>(*c++);
            if (i != (size - 1))
                ss << " ";
        }
        ss << "]";

        // response status
        ss << " (" << tx.get_response_string() << ")";

        // ToDo: byte enable, streaming, etc.
        return ss.str();
    }

}

std::istream& operator >> (std::istream& is, sc_core::sc_time& t) {
    std::string str; is >> str;
    str = vcml::to_lower(str);

    char* endptr = NULL;
    sc_dt::uint64 value = strtoul(str.c_str(), &endptr, 0);
    double fval = value;

    if (strcmp(endptr, "ps") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_PS);
    else if (strcmp(endptr, "ns") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_NS);
    else if (strcmp(endptr, "us") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_US);
    else if (strcmp(endptr, "ms") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_MS);
    else if (strcmp(endptr, "s") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_SEC);
    else // raw value, not part of ieee1666!
        t = sc_core::sc_time(value, true);

    return is;
}
