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

#ifndef VCML_GENERIC_GPIO
#define VCML_GENERIC_GPIO

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/ports.h"
#include "vcml/slave_socket.h"
#include "vcml/peripheral.h"
#include "vcml/register.h"

namespace vcml { namespace generic {

    class gpio: public peripheral
    {
    private:
        bool cmd_status(const vector<string>& args, ostream& os);
        bool cmd_set(const vector<string>& args, ostream& os);
        bool cmd_clear(const vector<string>& args, ostream& os);

        u32 read_DATA();
        u32 write_DATA(u32 val);

        // disabled
        gpio();
        gpio(const gpio&);
    public:
        reg<gpio, u32> DATA;

        out_port_list GPIO;
        slave_socket IN;

        gpio(const sc_module_name& name);
        virtual ~gpio();
        VCML_KIND(gpio);
        virtual void reset();

        virtual void end_of_elaboration() override;
    };

}}

#endif
