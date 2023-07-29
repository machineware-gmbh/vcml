/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/meta/loader.h"

namespace vcml {
namespace meta {

loader::loader(const sc_module_name& nm):
    component(nm),
    debugging::loader(*this, true),
    images("images"),
    insn("insn"),
    data("data") {
}

loader::loader(const sc_module_name& nm, const vector<string>& imginit):
    component(nm),
    debugging::loader(*this, true),
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

u8* loader::allocate_image(const elf_segment& seg, u64 off) {
    auto& port = seg.x ? insn : data;
    return port.lookup_dmi_ptr(seg.phys + off, seg.size, VCML_ACCESS_NONE);
}

void loader::copy_image(const u8* img, u64 size, u64 off) {
    if (failed(data.write(off, img, size, SBI_DEBUG)))
        VCML_REPORT("bus error writing [%llx..%llx]", off, off + size - 1);
}

void loader::copy_image(const u8* img, const elf_segment& seg, u64 off) {
    auto& port = seg.x ? insn : data;
    if (failed(port.write(seg.phys + off, img, seg.size, SBI_DEBUG))) {
        VCML_REPORT("bus error writing [%llx..%llx]", seg.phys + off,
                    seg.phys + off + seg.size - 1);
    }
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

VCML_EXPORT_MODEL(vcml::meta::loader, name, args) {
    return new loader(name, args);
}

} // namespace meta
} // namespace vcml
