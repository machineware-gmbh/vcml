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

#include "vcml/models/infra/loader.h"

namespace vcml { namespace infra {

    bool loader::cmd_load_elf(const vector<string>& args, ostream& os) {
        if (!file_exists(args[0])) {
            os << "File not found: " << args[0];
            return false;
        }

        u64 n = load_elf(args[0]);
        if (n == 0) {
            os << "Failed to read " << args[0];
            return false;
        }

        os << "OK, loaded " << n << " bytes total";
        return true;
    }

    size_t loader::load_elf(const string& filepath) {
        debugging::elf_reader reader(filepath);

        size_t count = 0;
        for (const auto& segment : reader.segments())
            count += load_elf_segment(reader, segment);

        return count;
    }

    size_t loader::load_elf_segment(debugging::elf_reader& reader,
                                    const debugging::elf_segment& seg) {
        master_socket& out = seg.x ? INSN : DATA;
        const range addr = { seg.phys, seg.phys + seg.size - 1 };
        log_debug("loading %s segment at 0x%016lx .. 0x%016lx (%lu bytes)",
                  seg.x ? "code" : "data", addr.start, addr.end, seg.size);

        // try DMI first, avoids one extra copy
        u8* dest = out.lookup_dmi_ptr(addr, VCML_ACCESS_WRITE);
        if (dest != nullptr && reader.read_segment(seg, dest) == seg.size)
            return seg.size;

        // otherwise, we read the entire seg into a local buffer
        // and copy it over using transport_dbg.
        log_debug("slow path loading segment at 0x%016lx", addr.start);

        vector<u8> b(seg.size);
        if (reader.read_segment(seg, b.data()) != seg.size) {
            log_warn("error reading segment at at 0x%016lx", addr.start);
            return 0;
        }

        if (failed(out.write(seg.phys, b.data(), b.size(), SBI_DEBUG))) {
            log_warn("error reading segment at at 0x%016lx", addr.start);
            return 0;
        }

        return seg.size;
    }

    loader::loader(const sc_module_name& nm, const string& imginit):
        component(nm),
        images("images", imginit),
        INSN("INSN"),
        DATA("DATA") {
        RESET.stub();
        CLOCK.stub();

        register_command("load_elf", 1, this, &loader::cmd_load_elf,
            "loads all sections from <FILE> to connected memories");
    }

    loader::~loader() {
        // nothing to do
    }

    void loader::reset() {
        component::reset();

        size_t count = 0;
        vector<string> files = split(images, ';');
        for (auto file : files) {
            file = trim(file);
            if (file.empty())
                continue;

            if (!file_exists(file)) {
                log_warn("file not found: %s", file.c_str());
                continue;
            }

            count += load_elf(file);
        }

        log_debug("loaded %zu segments from %zu files", count, files.size());
    }

}}
