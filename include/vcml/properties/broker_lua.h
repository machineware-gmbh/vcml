/******************************************************************************
 *                                                                            *
 * Copyright 2023 MachineWare GmbH                                            *
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

#ifndef VCML_BROKER_LUA_H
#define VCML_BROKER_LUA_H

#include "vcml/properties/broker.h"

namespace vcml {

class broker_lua : public broker
{
public:
    broker_lua() = delete;
    broker_lua(const string& filename);
    virtual ~broker_lua();
    VCML_KIND(broker_lua);
};

} // namespace vcml

#endif
