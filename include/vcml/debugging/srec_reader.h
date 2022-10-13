/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_DEBUGGING_SREC_READER_H
#define VCML_DEBUGGING_SREC_READER_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"

namespace vcml {
namespace debugging {

class srec_reader
{
public:
    struct record {
        u64 addr;
        vector<u8> data;
    };

    u64 entry() const { return m_entry; }
    string header() const { return m_header; }
    const vector<record>& records() const { return m_records; }

    srec_reader(const string& filename);
    ~srec_reader() = default;

    srec_reader(const srec_reader&) = delete;

private:
    u64 m_entry;
    string m_header;
    vector<record> m_records;
};

} // namespace debugging
} // namespace vcml

#endif
