/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_M_CAN_H
#define VCML_CAN_M_CAN_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/can.h"

namespace vcml {
namespace can {

class m_can : public peripheral, public can_host
{
private:
    bool m_tx_rx_enabled;

    size_t m_tx_buf_elem_data_sz;
    size_t m_rx_buf_elem_data_sz;
    size_t m_rx_fifo0_elem_data_sz;
    size_t m_rx_fifo1_elem_data_sz;

    sc_event m_txev;
    sc_event m_rxev;

    bool is_cfg_allowed() const;

    size_t get_tx_buf_elems() const;
    size_t get_tx_buf_fq_elems() const;
    size_t get_tx_buf_dc_elems() const;
    bool is_tx_mode_queue() const;

    size_t get_tx_evfifo_elems() const;
    void tx_evfifo_upd_fill_lvl();

    size_t get_rx_fifo0_elems() const;
    void rx_fifo0_upd_fill_lvl();

    void write_dbtp(u32 val);
    void write_test(u32 val);
    void write_rwd(u32 val);
    void write_cccr(u32 val);
    void write_nbtp(u32 val);
    void write_tscc(u32 val);
    void write_tscv(u32 val);
    void write_tocc(u32 val);
    void write_tocv(u32 val);
    void write_tdcr(u32 val);
    void write_ir(u32 val);
    void write_gfc(u32 val);
    void write_sidfc(u32 val);
    void write_xidfc(u32 val);
    void write_xidam(u32 val);
    void write_rxf0c(u32 val);
    void write_rxf0a(u32 val);
    void write_rxbc(u32 val);
    void write_rxf1c(u32 val);
    void write_rxesc(u32 val);
    void write_txbc(u32 val);
    void write_txesc(u32 val);
    void write_txbar(u32 val);
    void write_txbcr(u32 val);
    void write_txefc(u32 val);
    void write_txefa(u32 val);

    u32 read_ecr();
    u32 read_psr();

    void raise_irq(u32 val);
    void add_txevent(const u32 tx_buf_elem_hdr[2]);

    void txthread();
    void rxthread();

public:
    reg<u32> crel;   // core release register
    reg<u32> endn;   // endian register
    reg<u32> dbtp;   // customer register
    reg<u32> test;   // test register
    reg<u32> rwd;    // ram watchdog register
    reg<u32> cccr;   // cc control register
    reg<u32> nbtp;   // nominal bit timing & prescaler register
    reg<u32> tscc;   // timestamp counter cfg register
    reg<u32> tscv;   // timestamp counter value register
    reg<u32> tocc;   // timeout counter cfg register
    reg<u32> tocv;   // timeout counter value register
    reg<u32> ecr;    // error counter register
    reg<u32> psr;    // protocol status register
    reg<u32> tdcr;   // transmitter delay compensation register
    reg<u32> ir;     // irq register
    reg<u32> ie;     // irq enable register
    reg<u32> ils;    // irq line select register
    reg<u32> ile;    // irq line enable register
    reg<u32> gfc;    // global filter cfg register
    reg<u32> sidfc;  // standard id filter cfg register
    reg<u32> xidfc;  // extended id filter cfg register
    reg<u32> xidam;  // extended id and mask register
    reg<u32> hpms;   // high priority message status register
    reg<u32> ndat1;  // new data 1 register
    reg<u32> ndat2;  // new data 2 register
    reg<u32> rxf0c;  // rx fifo 0 cfg register
    reg<u32> rxf0s;  // rx fifo 0 status register
    reg<u32> rxf0a;  // rx fifo 0 ack register
    reg<u32> rxbc;   // rx buffer config register
    reg<u32> rxf1c;  // rx fifo 1 cfg register
    reg<u32> rxf1s;  // rx fifo 1 status register
    reg<u32> rxf1a;  // rx fifo 1 ack register
    reg<u32> rxesc;  // rx buffer/fifo element sz cfg register
    reg<u32> txbc;   // tx buffer cfg register
    reg<u32> txfqs;  // tx fifo/queue status register
    reg<u32> txesc;  // tx buffer element sz cfg register
    reg<u32> txbrp;  // tx buffer request pending register
    reg<u32> txbar;  // tx buffer add request register
    reg<u32> txbcr;  // tx buffer cancellation request register
    reg<u32> txbto;  // tx buffer transmission occurred register
    reg<u32> txbcf;  // tx buffer cancellation finished register
    reg<u32> txbtie; // tx bufer transmission irg enable register
    reg<u32> txbcie; // tx buf cancellation finished irq enable register
    reg<u32> txefc;  // tx event fifo cfg register
    reg<u32> txefs;  // tx event fifo status register
    reg<u32> txefa;  // tx event fifo ack register

    property<range> msg_ram_addr;

    gpio_initiator_socket irq0;
    gpio_initiator_socket irq1;

    tlm_target_socket in;
    tlm_initiator_socket dma;

    can_initiator_socket can_tx;
    can_target_socket can_rx;

    m_can(const sc_module_name& name, const range& msg_ram = { 0, 0x3fff });
    virtual ~m_can();
    VCML_KIND(can::m_can);
    virtual void reset() override;

protected:
    virtual void can_receive(const can_target_socket& socket,
                             can_frame& rx) override;
};

} // namespace can
} // namespace vcml

#endif
