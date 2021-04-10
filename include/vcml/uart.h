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

#ifndef VCML_UART_H
#define VCML_UART_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/serial/port.h"
#include "vcml/serial/backend.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/peripheral.h"

namespace vcml {

    class uart: public peripheral,
                public serial::port
    {
    private:
        vector<serial::backend*> m_backends;

        bool cmd_history(const vector<string>& args, ostream& os);

    public:
        property<string> backends;

        uart(const sc_module_name& nm, endianess e = host_endian(),
             unsigned int read_latency = 0, unsigned int write_latency = 0);
        virtual ~uart();

        uart() = delete;
        uart(const uart&) = delete;

        VCML_KIND(uart);
    };

}

#endif
