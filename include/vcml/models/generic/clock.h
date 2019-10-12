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

#ifndef VCML_GENERIC_CLOCK_H
#define VCML_GENERIC_CLOCK_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/ports.h"
#include "vcml/module.h"

namespace vcml { namespace generic {

    class clock: public module
    {
    public:
        property<clock_t> freq;
        out_port<clock_t> CLOCK;

        clock() = delete;
        clock(const clock&) = delete;

        clock(const sc_module_name& nm, clock_t init_freq);
        virtual ~clock();
        VCML_KIND(clock);

    protected:
        virtual void end_of_elaboration() override;
    };

}}

#endif
