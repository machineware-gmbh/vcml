/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#ifndef VCML_DEBUGGING_VNCSERVER_H
#define VCML_DEBUGGING_VNCSERVER_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"

namespace vcml { namespace debugging {

    class vncserver {
    private:
        u16 m_port;

        static std::map<u16, shared_ptr<vncserver>> m_servers;

        vncserver();
        vncserver(u16 port);
        vncserver(const vncserver&);

    public:
        u16 get_port() const { return m_port; }

        virtual ~vncserver();

        static shared_ptr<vncserver> lookup(u16 port);
    };

}}

#endif
