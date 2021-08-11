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

#ifndef VCML_BROKER_ENV_H
#define VCML_BROKER_ENV_H

#include "vcml/properties/broker.h"

namespace vcml {

    class broker_env: public broker
    {
    public:
        broker_env();
        virtual ~broker_env();
        VCML_KIND(broker_env);

        virtual bool defines(const string& name) const override;
        virtual bool lookup(const string& name, string& value) override;
    };

}

#endif
