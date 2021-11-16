/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_HWRNG_H
#define VCML_GENERIC_HWRNG_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/protocols/tlm.h"

#include "vcml/register.h"
#include "vcml/peripheral.h"

namespace vcml { namespace generic {

    class hwrng: public peripheral
    {
    private:
        u32 read_RNG();

    public:
        reg<u32> RNG;
        tlm_target_socket IN;

        property<bool> pseudo;
        property<u32> seed;

        hwrng() = delete;
        hwrng(const sc_module_name& nm);
        virtual ~hwrng();
        VCML_KIND(hwrng);

        virtual void reset() override;
    };

}}

#endif
