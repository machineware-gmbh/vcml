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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    struct range {
        u64 start;
        u64 end;

        u64 length() const;

        bool includes(const u64 addr) const;
        bool includes(const range& other) const;
        bool inside(const range& other) const;
        bool overlaps(const range& other) const;
        bool connects(const range& other) const;

        range intersect(const range& other) const;

        range();
        range(u64 start, u64 end);
        range(const tlm_generic_payload& tx);
        range(const tlm_dmi& dmi);

        bool operator == (const range& other) const;
        inline bool operator != (const range& other) const;
    };

    inline u64 range::length() const {
        return end - start + 1;
    }

    inline bool range::includes(const u64 addr) const {
        return addr >= start && addr <= end;
    }

    inline bool range::includes(const range& other) const {
        return includes(other.start) && includes(other.end);
    }

    inline bool range::inside(const range& other) const {
        return other.includes(*this);
    }

    inline bool range::overlaps(const range& other) const {
        return other.end >= start && other.start <= end;
    }

    inline bool range::connects(const range& other) const {
        return other.start == end + 1 || start == other.end + 1;
    }

    inline range range::intersect(const range& other) const {
        if (!overlaps(other))
            return range(0, 0);
        return range(max(start, other.start), min(end, other.end));
    }

    inline range::range(): start(0), end(0) {
        /* nothing to do */
    }

    inline range::range(u64 s, u64 e): start(s), end(e) {
        VCML_ERROR_ON(s > e, "invalid range specified: %016lx..%016lx", s, e);
    }

    inline range::range(const tlm_generic_payload& tx):
        start(tx.get_address()),
        end(tx.get_address() + tx.get_streaming_width() - 1) {
        if (tx.get_streaming_width() == 0) {
            end = tx.get_address() + tx.get_data_length() - 1;
            if (tx.get_data_length() == 0)
                end = start;
        }
    }

    inline range::range(const tlm_dmi& dmi):
        start(dmi.get_start_address()),
        end(dmi.get_end_address()) {
    }

    inline bool range::operator == (const range& other) const {
        return start == other.start && end == other.end;
    }

    inline bool range::operator != (const range& other) const {
        return start != other.start || end != other.end;
    }

}

inline std::istream& operator >> (std::istream& is, vcml::range& r) {
    is >> r.start;
    is >> r.end;
    return is;
}

inline std::ostream& operator << (std::ostream& os, const vcml::range& r) {
    int n = (r.start > std::numeric_limits<vcml::u32>::max() ||
             r.end   > std::numeric_limits<vcml::u32>::max()) ? 16 : 8;
    os <<  "0x" << std::hex << std::setw(n) << std::setfill('0') << r.start
       << " 0x" << std::hex << std::setw(n) << std::setfill('0') << r.end;
    return os;
}

#endif
