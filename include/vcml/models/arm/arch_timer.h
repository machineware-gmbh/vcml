/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ARM_ARCH_TIMER_H
#define VCML_ARM_ARCH_TIMER_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

namespace vcml {
namespace arm {

class arch_timer : public peripheral
{
private:
    u64 get_phys_counter();
    u64 get_virt_counter(size_t idx);

    void write_cntvoff(u64 val, size_t idx);

public:
    static constexpr size_t MAX_FRAMES = 8;

    class cntframe : public peripheral
    {
    private:
        size_t m_idx;
        arch_timer& m_parent;
        sc_event m_trigger;

        u32 read_cntp_tval();
        u32 read_cntv_tval();

        void write_cntp_cval(u64 val);
        void write_cntp_tval(u32 val);
        void write_cntp_ctl(u32 val);

        void write_cntv_cval(u64 val);
        void write_cntv_tval(u32 val);
        void write_cntv_ctl(u32 val);

    public:
        reg<u64> cntpct;
        reg<u64> cntvct;
        reg<u32> cntfrq;
        reg<u32> cntel0acr;
        reg<u64> cntvoff;

        reg<u64> cntp_cval;
        reg<u32> cntp_tval;
        reg<u32> cntp_ctl;

        reg<u64> cntv_cval;
        reg<u32> cntv_tval;
        reg<u32> cntv_ctl;

        tlm_target_socket in;
        gpio_initiator_socket irq_phys;
        gpio_initiator_socket irq_virt;

        cntframe(const sc_module_name& nm, arch_timer& timer, size_t idx);
        virtual ~cntframe() = default;
        VCML_KIND(arm::arch_timer::cntframe);

        void update();
    };

    property<size_t> nframes;

    sc_vector<cntframe> frames;

    reg<u32> cntfrq;
    reg<u32> cntnsar;
    reg<u32> cnttidr;
    reg<u32, MAX_FRAMES> cntacr;
    reg<u64, MAX_FRAMES> cntvoff;

    tlm_target_socket timer_in;
    tlm_base_target_array<MAX_FRAMES> frame_in;

    gpio_base_initiator_array<MAX_FRAMES> irq_phys;
    gpio_base_initiator_array<MAX_FRAMES> irq_virt;

    arch_timer(const sc_module_name& nm, size_t ntimers = 1);
    virtual ~arch_timer() = default;
    VCML_KIND(arm::arch_timer);
};

} // namespace arm
} // namespace vcml

#endif
