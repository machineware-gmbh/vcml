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

#include "vcml/models/generic/memory.h"

namespace vcml { namespace generic {

    bool memory::cmd_reset(const vector<string>& args, ostream& os)
    {
        memset(m_memory, 0, size);
        return true;
    }

    bool memory::cmd_load(const vector<string>& args, ostream& os)
    {
        string binary = args[0];
        u64 offset = 0ull;

        if (args.size() > 1)
            offset = strtoull(args[1].c_str(), NULL, 0);

        load(binary, offset);
        return true;
    }

    bool memory::cmd_show(const vector<string>& args, ostream& os)
    {
        u64 start = strtoull(args[0].c_str(), NULL, 0);
        u64 end = strtoull(args[1].c_str(), NULL, 0);

        if ((end <= start) || (end >= size))
            return false;

        #define HEX(x, w) std::setfill('0') << std::setw(w) << \
                          std::hex << x << std::dec
        os << "showing range 0x" << HEX(start, 8) << " .. 0x" << HEX(end, 8);

        u64 addr = start & ~0xf;
        while (addr < end) {
            if ((addr % 16) == 0)
                os << "\n" << HEX(addr, 8) << ":";
            if ((addr % 4) == 0)
                os << " ";
            if (addr >= start)
                os << HEX((unsigned int)m_memory[addr], 2) << " ";
            else
                os << "   ";
            addr++;
        }

        #undef HEX
        return true;
    }

    memory::memory(const sc_module_name& nm, u64 sz, bool read_only,
                   const sc_core::sc_time& rlat, const sc_core::sc_time& wlat):
        peripheral(nm, host_endian(), rlat, wlat),
        m_memory(NULL),
        size("size", sz),
        readonly("readonly", false),
        images("images", ""),
        IN("IN") {
        if (size > 0u) {
            m_memory = new unsigned char [size]();
            map_dmi(m_memory, 0, size - 1, readonly ? VCML_ACCESS_READ
                                                    : VCML_ACCESS_READ_WRITE);
        }

        register_command("reset", 0, this, &memory::cmd_reset,
            "Reset memory contents back to zero");
        register_command("load", 1, this, &memory::cmd_load,
            "Load <binary> [off] to load the contents of file <binary> to " \
            "relative offset [off] in memory (offset is zero if unspecified).");
        register_command("show", 2, this, &memory::cmd_show,
            "Show memory contents between addresses [start] and [end]. " \
            "Usage: show [start] [end]");

        if (!images.get().empty()) {
            vector<string> strings = split(images, isspace);
            for (auto str : strings) {
                vector<string> params = split(str,
                        [](int ch) -> bool { return (char)ch == '@'; });
                string image = params[0];
                u64 offset = 0;
                if (params.size() > 1)
                    offset = strtoull(params[1].c_str(), NULL, 0);
                log_debug("loading '%s' to 0x%08llx", image.c_str(), offset);
                load(image, offset);
            }
        }
    }

    memory::~memory() {
        if (m_memory)
            delete [] m_memory;
    }

    void memory::load(const string& binary, u64 offset) {
        ifstream file(binary.c_str(), std::ios::binary | std::ios::ate);
        VCML_ERROR_ON(!file.is_open(), "error opening '%s'", binary.c_str());

        u64 nbytes = file.tellg();
        nbytes = min(nbytes, size - offset);
        file.seekg(0, std::ios::beg);
        file.read((char*)(m_memory + offset), nbytes);
    }

    tlm_response_status memory::read(const range& addr, void* data, int flags) {
        if (addr.end >= size)
            return TLM_ADDRESS_ERROR_RESPONSE;
        memcpy(data, m_memory + addr.start, addr.length());
        return TLM_OK_RESPONSE;
    }

    tlm_response_status memory::write(const range& addr, const void* data,
                                      int flags) {
        if (addr.end >= size)
            return TLM_ADDRESS_ERROR_RESPONSE;
        if (readonly && !is_debug(flags))
            return TLM_COMMAND_ERROR_RESPONSE;
        memcpy(m_memory + addr.start, data, addr.length());
        return TLM_OK_RESPONSE;
    }

}}
