/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_META_LOADER_H
#define VCML_META_LOADER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/debugging/loader.h"
#include "vcml/protocols/tlm.h"

namespace vcml {
namespace meta {

class loader : public component, public debugging::loader
{
public:
    property<vector<string>> images;

    tlm_initiator_socket insn;
    tlm_initiator_socket data;

    loader(const sc_module_name& nm);
    loader(const sc_module_name& nm, const vector<string>& images);
    virtual ~loader();
    VCML_KIND(loader);

    virtual void reset() override;

protected:
    virtual u8* allocate_image(u64 size, u64 offset) override;
    virtual u8* allocate_image(const elf_segment& seg, u64 offset) override;

    virtual void copy_image(const u8* img, u64 size, u64 offset) override;
    virtual void copy_image(const u8* img, const elf_segment& s,
                            u64 offset) override;

    virtual void before_end_of_elaboration() override;
};

} // namespace meta
} // namespace vcml

#endif
