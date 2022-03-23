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

namespace vcml {
namespace generic {

bool memory::cmd_show(const vector<string>& args, ostream& os) {
    u64 start = strtoull(args[0].c_str(), NULL, 0);
    u64 end   = strtoull(args[1].c_str(), NULL, 0);

    if ((end <= start) || (end >= size))
        return false;

#define HEX(x, w) \
    std::setfill('0') << std::setw(w) << std::hex << x << std::dec
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

u8* memory::allocate_image(u64 sz, u64 off) {
    if (off >= size)
        VCML_REPORT("offset 0x%lx exceeds memory size", off);

    if (sz + off > size)
        VCML_REPORT("image too big for memory");

    return m_memory.data() + off;
}

void memory::copy_image(const u8* image, u64 sz, u64 off) {
    if (off >= size)
        VCML_REPORT("offset 0x%lx exceeds memory size", off);

    if (sz + off > size)
        VCML_REPORT("image too big for memory");

    memcpy(m_memory.data() + off, image, sz);
}

memory::memory(const sc_module_name& nm, u64 sz, bool read_only, alignment al,
               unsigned int rl, unsigned int wl):
    peripheral(nm, host_endian(), rl, wl),
    debugging::loader(name()),
    m_memory(),
    size("size", sz),
    align("align", al),
    discard_writes("discard_writes", false),
    readonly("readonly", read_only),
    images("images", ""),
    poison("poison", 0x00),
    in("in") {
    VCML_ERROR_ON(size == 0u, "memory size cannot be 0");
    VCML_ERROR_ON(al > VCML_ALIGN_1G, "requested alignment too big");

    m_memory.init(size, align);
    m_memory.set_read_latency(read_cycles());
    m_memory.set_write_latency(write_cycles());

    if (readonly)
        m_memory.allow_read_only();
    if (discard_writes)
        m_memory.discard_writes();

    map_dmi(m_memory);

    register_command(
        "show", 2, this, &memory::cmd_show,
        "show memory contents between addresses [start] and [end]. "
        "usage: show [start] [end]");
}

memory::~memory() {
    // nothing to do
}

void memory::reset() {
    if (poison > 0)
        m_memory.fill(poison);

    load_images(images);
}

tlm_response_status memory::read(const range& addr, void* data,
                                 const tlm_sbi& info) {
    return m_memory.read(addr, data, info.is_debug);
}

tlm_response_status memory::write(const range& addr, const void* data,
                                  const tlm_sbi& info) {
    return m_memory.write(addr, data, info.is_debug);
}

} // namespace generic
} // namespace vcml
