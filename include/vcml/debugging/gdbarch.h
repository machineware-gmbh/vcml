/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_DEBUGGING_GDBARCH_H
#define VCML_DEBUGGING_GDBARCH_H

#include "vcml/core/types.h"
#include "vcml/debugging/target.h"

namespace vcml {
namespace debugging {

struct gdbfeature {
    const char* name;
    vector<string> registers;

    bool collect_regs(const target& t, vector<const cpureg*>& regs) const;
    bool collect_regs(const target& t, vector<const cpureg*>& regs,
                      vector<string>& missing) const;

    bool supported(const target& t) const;
    bool supported(const target& t, vector<string>& missing) const;

    void write_xml(const target& t, ostream& os) const;
};

inline bool gdbfeature::collect_regs(const target& t,
                                     vector<const cpureg*>& regs) const {
    vector<string> missing;
    return collect_regs(t, regs, missing);
}

inline bool gdbfeature::supported(const target& t) const {
    vector<string> missing;
    return supported(t, missing);
}

inline bool gdbfeature::supported(const target& t,
                                  vector<string>& miss) const {
    vector<const cpureg*> regs;
    return collect_regs(t, regs, miss);
}

struct gdbarch {
    const char* name;
    const char* gdb_name;
    const char* abi_name;
    vector<gdbfeature> features;

    gdbarch(const char* name, const char* gdb, const char* abi,
            const vector<gdbfeature>& features);
    ~gdbarch();

    void write_xml(target& t, ostream& os) const;

    bool collect_core_regs(const target& t, vector<const cpureg*>& r) const;

    bool supported(const target& t) const;

    static const gdbarch* lookup(const string& name);

    static const gdbarch AARCH64;
    static const gdbarch ARM;
    static const gdbarch ARM_M;
    static const gdbarch OR1K;
    static const gdbarch RISCV;
};

inline bool gdbarch::supported(const target& t) const {
    vector<const cpureg*> regs;
    return collect_core_regs(t, regs);
}

} // namespace debugging
} // namespace vcml

#endif
