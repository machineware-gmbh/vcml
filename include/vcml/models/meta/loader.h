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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/strings.h"
#include "vcml/common/utils.h"

#include "vcml/debugging/elf_reader.h"

#include "vcml/component.h"
#include "vcml/master_socket.h"

namespace vcml { namespace meta {

    class loader: public component
    {
    private:
        bool cmd_load_elf(const vector<string>& args, ostream& os);

        size_t load_elf(const string& filepath);
        size_t load_elf_segment(debugging::elf_reader& reader,
                                const debugging::elf_segment& segment);

    public:
        property<string> images;

        master_socket INSN;
        master_socket DATA;

        loader(const sc_module_name& nm, const string& images = "");
        virtual ~loader();
        VCML_KIND(loader);

        virtual void reset() override;

    protected:
        virtual void end_of_elaboration() override;
    };

}}

#endif
