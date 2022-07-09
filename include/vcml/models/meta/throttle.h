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

#ifndef VCML_META_THROTTLE_H
#define VCML_META_THROTTLE_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/utils.h"
#include "vcml/core/module.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

namespace vcml {
namespace meta {

class throttle : public module
{
private:
    bool m_throttling;

    u64 m_start;
    u64 m_extra;

    void update();

public:
    property<sc_time> update_interval;
    property<double> rtf;

    throttle(const sc_module_name& nm);
    virtual ~throttle() = default;
    VCML_KIND(throttle);

    bool is_throttling() const { return m_throttling; }

protected:
    virtual void session_suspend() override;
    virtual void session_resume() override;
};

} // namespace meta
} // namespace vcml

#endif
