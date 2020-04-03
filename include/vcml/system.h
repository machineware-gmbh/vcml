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

#ifndef VCML_SYSTEM_H
#define VCML_SYSTEM_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/debugging/vspserver.h"
#include "vcml/module.h"

namespace vcml {

    class system: public module
    {
    public:
        property<string>  name;
        property<string>  desc;

        property<bool>    backtrace;

        property<u16>     session;
        property<bool>    session_debug;

        property<sc_time> quantum;
        property<sc_time> duration;

        system() = delete;
        system(const system&) = delete;
        explicit system(const sc_module_name& name);
        virtual ~system();
        VCML_KIND(system);

        virtual int run();
    };

}


#endif
