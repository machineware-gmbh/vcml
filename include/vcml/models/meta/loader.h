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

#ifndef VCML_META_LOADER_H
#define VCML_META_LOADER_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"

#include "vcml/debugging/loader.h"
#include "vcml/debugging/elf_reader.h"
#include "vcml/protocols/tlm.h"

namespace vcml {
namespace meta {

class loader : public component, public debugging::loader
{
public:
    property<string> images;

    tlm_initiator_socket insn;
    tlm_initiator_socket data;

    loader(const sc_module_name& nm, const string& images = "");
    virtual ~loader();
    VCML_KIND(loader);

    virtual void reset() override;

protected:
    virtual u8* allocate_image(u64 size, u64 offset) override;
    virtual u8* allocate_image(const debugging::elf_segment& seg,
                               u64 offset) override;

    virtual void copy_image(const u8* img, u64 size, u64 offset) override;
    virtual void copy_image(const u8* img, const debugging::elf_segment& s,
                            u64 offset) override;

    virtual void before_end_of_elaboration() override;
};

} // namespace meta
} // namespace vcml

#endif
