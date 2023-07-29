/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_MEMORY_H
#define VCML_GENERIC_MEMORY_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/model.h"
#include "vcml/core/peripheral.h"

#include "vcml/debugging/loader.h"
#include "vcml/protocols/tlm.h"

namespace vcml {
namespace generic {

class memory : public peripheral, public debugging::loader
{
private:
    tlm_memory m_memory;

    bool cmd_show(const vector<string>& args, ostream& os);

    memory();
    memory(const memory&);

protected:
    virtual u8* allocate_image(u64 size, u64 offset) override;
    virtual void copy_image(const u8* img, u64 size, u64 offset) override;

public:
    property<u64> size;
    property<alignment> align;
    property<bool> discard_writes;
    property<bool> readonly;
    property<string> shared;
    property<vector<string>> images;
    property<u8> poison;

    tlm_target_socket in;

    u8* data() const { return m_memory.data(); }

    u8& operator[](size_t idx) { return m_memory[idx]; }
    u8 operator[](size_t idx) const { return m_memory[idx]; }

    memory(const sc_module_name& name, u64 size, bool read_only = false,
           alignment al = VCML_ALIGN_NONE, unsigned int read_latency = 0,
           unsigned int write_latency = 0);
    virtual ~memory();
    VCML_KIND(memory);
    virtual void reset() override;

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info) override;
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info) override;
};

} // namespace generic
} // namespace vcml

#endif
