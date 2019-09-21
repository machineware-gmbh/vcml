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

#ifndef VCML_GENERIC_SIMDEV_H
#define VCML_GENERIC_SIMDEV_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"

#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace generic {

    class simdev: public peripheral
    {
    private:
        u32 write_STOP(u32 val);
        u32 write_EXIT(u32 val);
        u32 write_ABRT(u32 val);

        u64 read_SCLK();
        u64 read_HCLK();

        u32 write_SOUT(u32 val);
        u32 write_SERR(u32 val);

        u32 read_PRNG();

        // disabled
        simdev();
        simdev(const simdev&);

    public:
        // simulation control
        reg<simdev, u32> STOP;
        reg<simdev, u32> EXIT;
        reg<simdev, u32> ABRT;

        // timing
        reg<simdev, u64> SCLK;
        reg<simdev, u64> HCLK;

        // output
        reg<simdev, u32> SOUT;
        reg<simdev, u32> SERR;

        // random
        reg<simdev, u32> PRNG;

        slave_socket IN;

        simdev(const sc_module_name& name);
        virtual ~simdev();
        VCML_KIND(simdev);

        virtual void reset() override;
    };

}}

#endif
