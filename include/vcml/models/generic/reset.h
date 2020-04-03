/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_RESET_H
#define VCML_GENERIC_RESET_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/ports.h"
#include "vcml/module.h"

namespace vcml { namespace generic {

    class reset: public module
    {
    public:
        property<bool> state;

        out_port<bool> RESET;

        reset() = delete;
        reset(const sc_module_name& nm, bool init_state = false);
        virtual ~reset();
        VCML_KIND(reset);

    protected:
        virtual void end_of_elaboration() override;
    };

}}

#endif
