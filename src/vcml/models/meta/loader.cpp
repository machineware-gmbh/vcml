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

#include "vcml/models/meta/loader.h"

namespace vcml {
namespace meta {

loader::loader(const sc_module_name& nm, const string& imginit):
    component(nm),
    debugging::loader(name()),
    images("images", imginit),
    insn("insn"),
    data("data") {
}

loader::~loader() {
    // nothing to do
}

void loader::reset() {
    component::reset();
    load_images(images);
}

u8* loader::allocate_image(u64 size, u64 offset) {
    u8* ptr = data.lookup_dmi_ptr(offset, size, VCML_ACCESS_NONE);
    if (ptr != nullptr)
        return ptr;
    return insn.lookup_dmi_ptr(offset, size, VCML_ACCESS_NONE);
}

u8* loader::allocate_image(const debugging::elf_segment& seg, u64 off) {
    auto& port = seg.x ? insn : data;
    return port.lookup_dmi_ptr(seg.phys + off, seg.size, VCML_ACCESS_NONE);
}

void loader::copy_image(const u8* img, u64 size, u64 offset) {
    if (failed(data.write(offset, img, size, SBI_DEBUG)))
        VCML_REPORT("bus error");
}

void loader::copy_image(const u8* img, const debugging::elf_segment& seg,
                        u64 offset) {
    auto& port = seg.x ? insn : data;
    if (failed(port.write(seg.phys + offset, img, seg.size, SBI_DEBUG)))
        VCML_REPORT("bus error");
}

void loader::before_end_of_elaboration() {
    component::before_end_of_elaboration();
    if (!clk.is_bound())
        clk.stub(100 * MHz);
    if (!rst.is_bound()) {
        rst.stub();
        reset();
    }
}

} // namespace meta
} // namespace vcml
