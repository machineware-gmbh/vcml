/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/gdbarch.h"

namespace vcml {
namespace debugging {

bool gdbfeature::collect_regs(const target& t, vector<const cpureg*>& regs,
                              vector<string>& missing) const {
    regs.clear();
    missing = registers;

    auto it = missing.begin();
    while (it != missing.end()) {
        const cpureg* r = t.find_cpureg(*it);
        if (r != nullptr) {
            it = missing.erase(it);
            regs.push_back(r);
        } else {
            it++;
        }
    }

    return missing.empty();
}

static void write_vec_init(const cpureg* reg, ostream& os) {
    struct vec_type_size {
        const char* gdb_type;
        const char* id;
        size_t size;
        const char suffix;
    };

    static constexpr vec_type_size vec_lanes[] = {
        { "uint128", "quads", 128, 'q' }, { "uint64", "longs", 64, 'l' },
        { "uint32", "words", 32, 'w' },   { "uint16", "shorts", 16, 's' },
        { "uint8", "bytes", 8, 'b' },
    };

    for (auto& vec : vec_lanes) {
        if (vec.size <= reg->total_width()) {
            os << "<vector id=\"" << vec.id << "\""
               << "  type=\"" << vec.gdb_type << "\""
               << "  count=\"" << reg->total_width() / vec.size << "\" />"
               << std::endl;
        }
    }

    os << "<union id=\"vector_union\">" << std::endl;

    for (auto& vec : vec_lanes) {
        if (vec.size <= reg->total_width()) {
            os << "<field name=\"" << vec.suffix << "\""
               << "  type=\"" << vec.id << "\"/>" << std::endl;
        }
    }

    os << "</union>";
}

void gdbfeature::write_xml(const target& t, ostream& os) const {
    vector<const cpureg*> cpuregs;
    if (!collect_regs(t, cpuregs) || cpuregs.empty())
        return;

    os << "<feature name=\"" << name << "\">" << std::endl;

    size_t vec_size = 0;
    for (size_t i = 0; i < cpuregs.size(); i++) {
        if (cpuregs[i]->count <= 1) {
            os << "<reg name=\"" << registers[i] << "\""
               << "  regnum=\"" << cpuregs[i]->regno << "\""
               << "  bitsize=\"" << cpuregs[i]->width() << "\" />"
               << std::endl;
        } else {
            if (vec_size == 0) {
                write_vec_init(cpuregs[i], os);
                vec_size = cpuregs[i]->total_size();
            } else if (vec_size != cpuregs[i]->total_size()) {
                VCML_ERROR(
                    "vector register %s should be size %zu, but it is %zu",
                    cpuregs[i]->name.c_str(), vec_size,
                    cpuregs[i]->total_size());
            }

            os << "<reg name=\"" << registers[i] << "\""
               << "  regnum=\"" << cpuregs[i]->regno << "\""
               << "  bitsize=\"" << cpuregs[i]->total_width() << "\""
               << "  group=\"vector\""
               << "  type=\"vector_union\" />" << std::endl;
        }
    }

    os << "</feature>" << std::endl;
}

static vector<gdbarch*>& all_gdbarchs() {
    static vector<gdbarch*> archs;
    return archs;
}

const gdbarch* gdbarch::lookup(const string& name) {
    if (name.empty())
        return nullptr;

    const vector<gdbarch*>& all = all_gdbarchs();
    for (auto arch : all)
        if (name.compare(arch->name) == 0)
            return arch;

    return nullptr;
}

gdbarch::gdbarch(const char* nm, const char* gdb, const char* abi,
                 const vector<gdbfeature>& feats):
    name(nm), gdb_name(gdb), abi_name(abi), features(feats) {
    VCML_ERROR_ON(lookup(name), "gdbarch %s already exists", name);
    all_gdbarchs().push_back(this);
}

gdbarch::~gdbarch() {
    stl_remove(all_gdbarchs(), this);
}

void gdbarch::write_xml(target& t, ostream& os) const {
    os << "<?xml version=\"1.0\"?>" << std::endl
       << "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">" << std::endl
       << "<target version=\"1.0\">" << std::endl;

    os << "<architecture>" << gdb_name << "</architecture>" << std::endl;
    if (abi_name && strlen(abi_name))
        os << "<osabi>" << abi_name << "</osabi>" << std::endl;

    for (auto& feat : features)
        feat.write_xml(t, os);

    t.write_gdb_xml_feature(os);

    os << "</target>" << std::endl;
}

bool gdbarch::collect_core_regs(const target& t,
                                vector<const cpureg*>& r) const {
    if (features.empty())
        return false;
    if (!features[0].collect_regs(t, r) || r.empty())
        return false;
    return !r.empty();
}

const gdbarch gdbarch::AARCH64 = gdbarch(
    "aarch64", "aarch64", "none",
    {
        { "org.gnu.gdb.aarch64.core",
          { "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",  "x8",
            "x9",  "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17",
            "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26",
            "x27", "x28", "x29", "x30", "sp",  "pc",  "cpsr" } },
        { "org.gnu.gdb.aarch64.fpu",
          { "v0",  "v1",  "v2",  "v3",  "v4",  "v5",   "v6",  "v7",  "v8",
            "v9",  "v10", "v11", "v12", "v13", "v14",  "v15", "v16", "v17",
            "v18", "v19", "v20", "v21", "v22", "v23",  "v24", "v25", "v26",
            "v27", "v28", "v29", "v30", "v31", "fpsr", "fpcr" } },
        { "org.gnu.gdb.aarch64.sve",
          { "z0",  "z1",  "z2",  "z3",  "z4",  "z5",  "z6",  "z7",  "z8",
            "z9",  "z10", "z11", "z12", "z13", "z14", "z15", "z16", "z17",
            "z18", "z19", "z20", "z21", "z22", "z23", "z24", "z25", "z26",
            "z27", "z28", "z29", "z30", "z31", "p0",  "p1",  "p2",  "p3",
            "p4",  "p5",  "p6",  "p7",  "p8",  "p9",  "p10", "p11", "p12",
            "p13", "p14", "p15", "ffr", "vg" } },
        { "org.gnu.gdb.aarch64.pauth", { "pauth_dmask", "pauth_cmask" } },
    });

const gdbarch gdbarch::ARM = gdbarch(
    "arm", "arm", "none",
    {
        { "org.gnu.gdb.arm.core",
          { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
            "r11", "r12", "sp", "lr", "pc", "cpsr" } },
        { "org.gnu.gdb.arm.m-profile",
          { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
            "r11", "r12", "sp", "lr", "pc", "xpsr" } },
        { "org.gnu.gdb.arm.fpa",
          { "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "fps" } },
        { "org.gnu.gdb.arm.vfp",
          { "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10",
            "d11", "d12", "d13", "d14", "d15", "fpscr" } },
    });

const gdbarch gdbarch::ARM_M = gdbarch(
    "arm-m", "arm", "none",
    {
        { "org.gnu.gdb.arm.m-profile",
          { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
            "r11", "r12", "sp", "lr", "pc", "xpsr" } },
    });

const gdbarch gdbarch::OR1K = gdbarch(
    "or1k", "or1k", "none",
    {
        { "org.gnu.gdb.or1k.group0",
          { "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",  "r8",
            "r9",  "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17",
            "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26",
            "r27", "r28", "r29", "r30", "r31", "ppc", "npc", "sr" } },
    });

const gdbarch gdbarch::RISCV = gdbarch(
    "riscv", "riscv", "none",
    {
        { "org.gnu.gdb.riscv.cpu",
          { "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0",
            "s1",   "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
            "s2",   "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10",
            "s11",  "t3", "t4", "t5", "t6", "pc" } },
        { "org.gnu.gdb.riscv.fpu",
          { "ft0", "ft1", "ft2",  "ft3",  "ft4",    "ft5",  "ft6",
            "ft7", "fs0", "fs1",  "fa0",  "fa1",    "fa2",  "fa3",
            "fa4", "fa5", "fa6",  "fa7",  "fs2",    "fs3",  "fs4",
            "fs5", "fs6", "fs7",  "fs8",  "fs9",    "fs10", "fs11",
            "ft8", "ft9", "ft10", "ft11", "fflags", "frm",  "fcsr" } },
        { "org.gnu.gdb.riscv.vector",
          { "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
            "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
            "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
            "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31" } },
        { "org.gnu.gdb.riscv.virtual", { "priv" } },
    });

} // namespace debugging
} // namespace vcml
