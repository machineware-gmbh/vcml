/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RANGE_H
#define VCML_RANGE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

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

    bool operator==(const range& other) const;
    bool operator!=(const range& other) const;
    bool operator<(const range& other) const;
    bool operator>(const range& other) const;

    range& operator+=(u64 offset);
    range& operator-=(u64 offset);

    range operator+(u64 offset) const;
    range operator-(u64 offset) const;
};

VCML_TYPEINFO(range);

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
    // nothing to do
}

inline range::range(u64 s, u64 e): start(s), end(e) {
    VCML_ERROR_ON(e < s && s - e > 1, "invalid range: %016llx..%016llx", s, e);
}

inline range::range(const tlm_generic_payload& tx):
    start(tx.get_address()), end(tx.get_address() + tx_size(tx) - 1) {
}

inline range::range(const tlm_dmi& dmi):
    start(dmi.get_start_address()), end(dmi.get_end_address()) {
}

inline bool range::operator==(const range& other) const {
    return start == other.start && end == other.end;
}

inline bool range::operator!=(const range& other) const {
    return start != other.start || end != other.end;
}

inline bool range::operator<(const range& other) const {
    return end < other.start;
}

inline bool range::operator>(const range& other) const {
    return start > other.end;
}

inline range& range::operator+=(u64 offset) {
    VCML_ERROR_ON(end + offset < end, "range overflow");
    start += offset;
    end += offset;
    return *this;
}

inline range& range::operator-=(u64 offset) {
    VCML_ERROR_ON(offset > start, "range underflow");
    start -= offset;
    end -= offset;
    return *this;
}

inline range range::operator+(u64 offset) const {
    range r(*this);
    r += offset;
    return r;
}

inline range range::operator-(u64 offset) const {
    range r(*this);
    r -= offset;
    return r;
}

inline std::istream& operator>>(std::istream& is, vcml::range& r) {
    string s;
    is >> s;
    u64 start = 0, end = 0;
    if (sscanf(s.c_str(), "0x%llx..0x%llx", &start, &end) == 2) {
        r.start = start;
        r.end = end;
    } else
        is.setstate(std::ios::failbit);
    return is;
}

inline std::ostream& operator<<(std::ostream& os, const vcml::range& r) {
    int n = (r.start > std::numeric_limits<vcml::u32>::max() ||
             r.end > std::numeric_limits<vcml::u32>::max())
                ? 16
                : 8;
    os << "0x" << std::hex << std::setw(n) << std::setfill('0') << r.start
       << "..0x" << std::hex << std::setw(n) << std::setfill('0') << r.end;
    return os;
}

} // namespace vcml

#endif
