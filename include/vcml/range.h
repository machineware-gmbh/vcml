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

#ifndef VCML_RANGE_H
#define VCML_RANGE_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    struct range {
        u64 start;
        u64 end;

        inline u64 length() const {
            return end - start + 1;
        }

        inline bool includes(const u64 addr) const {
            return addr >= start && addr <= end;
        }

        inline bool includes(const range& other) const {
            return includes(other.start) && includes(other.end);
        }

        inline bool inside(const range& other) const {
            return other.includes(*this);
        }

        inline bool overlaps(const range& other) const {
            return other.end >= start && other.start <= end;
        }

        inline bool connects(const range& other) const {
            return other.start == end + 1 || start == other.end + 1;
        }

        inline range intersect(const range& other) const {
            if (!overlaps(other))
                return range(0, 0);
            return range(max(start, other.start), min(end, other.end));
        }

        inline range(u64 _start, u64 _end): start(_start), end(_end) {
            VCML_ERROR_ON(_start > _end, "invalid range specified");
        }

        inline range(const tlm_generic_payload& tx):
            start(tx.get_address()),
            end(tx.get_address() + tx.get_streaming_width() - 1) {
            if (tx.get_streaming_width() == 0)
                end = tx.get_address() + tx.get_data_length() - 1;
        }

        inline range(const tlm_dmi& dmi):
            start(dmi.get_start_address()),
            end(dmi.get_end_address()) {
        }

        inline bool operator == (const range& other) const {
            return start == other.start && end == other.end;
        }

        inline bool operator != (const range& other) const {
            return start != other.start || end != other.end;
        }
    };

}

#endif
