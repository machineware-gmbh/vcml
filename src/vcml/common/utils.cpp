/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/common/utils.h"

namespace vcml {

    string mkstr(const char* format, ...) {
        va_list args;
        va_start(args, format);
        string str = vmkstr(format, args);
        va_end(args);
        return str;
    }

    string vmkstr(const char* format, va_list args) {
        va_list args2;
        va_copy(args2, args);

        int size = vsnprintf(NULL, 0, format, args) + 1;
        if (size <= 0)
            return "";

        char* buffer = new char [size];
        vsnprintf(buffer, size, format, args2);
        va_end(args2);

        string s(buffer);
        delete [] buffer;
        return s;
    }

    string concat(const string& a, const string& b) {
        return a + b;
    }

    string dirname(const string& filename) {
#ifdef _WIN32
        const char separator = '\\';
#else
        const char separator = '/';
#endif

        size_t i = filename.rfind(separator, filename.length());
        return (i == string::npos) ? "" : filename.substr(0, i);
    }

    string tlm_response_to_str(tlm_response_status status) {
        switch (status) {
        case TLM_INCOMPLETE_RESPONSE:
            return "TLM_INCOMPLETE_RESPONSE";
            break;

        case TLM_GENERIC_ERROR_RESPONSE:
            return "TLM_GENERIC_ERROR_RESPONSE";
            break;

        case TLM_ADDRESS_ERROR_RESPONSE:
            return "TLM_ADDRESS_ERROR_RESPONSE";
            break;

        case TLM_COMMAND_ERROR_RESPONSE:
            return "TLM_COMMAND_ERROR_RESPONSE";
            break;

        case TLM_BURST_ERROR_RESPONSE:
            return "TLM_BURST_ERROR_RESPONSE";
            break;

        case TLM_BYTE_ENABLE_ERROR_RESPONSE:
            return "TLM_BYTE_ENABLE_ERROR_RESPONSE";
            break;

        default: {
                stringstream ss;
                ss << "TLM_UNKNOWN_RESPONSE (" << status << ")";
                return ss.str();
            }
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
        ss.width(8);
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

    int replace(string& str, const string& search, const string& repl) {
        int count = 0;
        while (true) {
             size_t index = str.find(search, index);
             if (index == std::string::npos)
                 break;

             str.replace(index, repl.length(), repl);
             index += repl.length();
             count++;
        }

        return count;
    }

    vector<string> backtrace(unsigned int frames, unsigned int skip) {
        vector<string> sv;

        void* symbols[frames + skip];
        unsigned int size = (unsigned int)::backtrace(symbols, frames + skip);
        if (size <= skip)
            return sv;

        sv.resize(size - skip);

        size_t dmbufsz = 256;
        char* dmbuf = (char*)malloc(dmbufsz);
        char** names = ::backtrace_symbols(symbols, size);
        for (unsigned int i = skip; i < size; i++) {
            char* func = NULL, *offset = NULL, *end = NULL;
            for (char* ptr = names[i]; *ptr != '\0'; ptr++) {
                if (*ptr == '(')
                    func = ptr++;
                else if (*ptr == '+')
                    offset = ptr++;
                else if (*ptr == ')') {
                    end = ptr++;
                    break;
                }
            }

            if (!func || !offset || !end) {
                sv[i-1] = mkstr("<unknown> [%p]", symbols[i]);
                continue;
            }

            *func++ = '\0';
            *offset++ = '\0';
            *end = '\0';

            sv[i-skip] = string(func) + "+" + string(offset);

            int status = 0;
            char* res = abi::__cxa_demangle(func, dmbuf, &dmbufsz, &status);
            if (status == 0) {
                sv[i-skip] = string(dmbuf) + "+" + string(offset);
                dmbuf = res; // dmbuf might get reallocated
            }
        }

        free(names);
        free(dmbuf);

        return sv;
    }

}

std::istream& operator >> (std::istream& is, vcml::vcml_endian& endian) {
    std::string str;
    is >> str;

    if ((str == "big") || (str == "BIG"))
        endian = vcml::VCML_ENDIAN_BIG;
    else if ((str == "little") || (str == "LITTLE"))
        endian = vcml::VCML_ENDIAN_LITTLE;
    else
        endian = vcml::VCML_ENDIAN_UNKNOWN;
    return is;
}

std::istream& operator >> (std::istream& is, sc_core::sc_time& t) {
    std::string str;
    is >> str;

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
