/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/m_can.h"

namespace vcml {
namespace can {

enum crel_bits : u32 {
    CREL_DAY = bitmask(8),         // time stamp day
    CREL_MON = bitmask(8, 8),      // time stamp month
    CREL_YEAR = bitmask(4, 16),    // time stamp year
    CREL_SUBSTEP = bitmask(4, 20), // sub-step of core release
    CREL_STEP = bitmask(4, 24),    // step of core release
    CREL_REL = bitmask(4, 28),     // core release

    // Core release: 3.3.0, time stamp: 01/04/2 (dd/mm/y)
    CREL_RESET = 0x3 << 28 | 0x3 << 24 | 0x2 << 16 | 0x4 << 8 | 0x1 << 0
};

enum endn_bits : u32 {
    ENDN_ETV = bitmask(32), // endianess test val
    ENDN_RESET = 0x87654321
};

enum dbtp_bits : u32 {
    DBTP_DSJW = bitmask(4),      // data (re)sync jump width
    DBTP_DTSEG2 = bitmask(4, 4), // data time segment after sample point
    DBTP_DTSEG1 = bitmask(5, 8), // data time segment before sample point
    DBTP_DBRP = bitmask(5, 16),  // data bit rate prescaler
    DBTP_TDC = bit(23),          // transmitter delay compensation

    DBTP_RESET = 0xa << 8 | 0x3 << 4 | 0x3 << 0,
    DBTP_MASK = DBTP_DSJW | DBTP_DTSEG2 | DBTP_DTSEG1 | DBTP_DBRP
};

enum test_bits : u32 {
    TEST_LBCK = bit(4),          // loop back mode
    TEST_TX = bitmask(2, 5),     // control of transmit pin
    TEST_RX = bit(7),            // receive pin
    TEST_TXBNP = bitmask(5, 8),  // tx buffer number prepared
    TEST_PVAL = bit(13),         // prepared valid
    TEST_TXBNS = bitmask(5, 16), // tx buffer number started
    TEST_SVAL = bit(21),         // started value

    TEST_MASK = TEST_LBCK | TEST_TX
};

enum rwd_bits : u32 {
    RWD_WDC = bitmask(8),    // watchdog cfg
    RWD_WDV = bitmask(8, 8), // watchdog value
    RWD_MASK = RWD_WDV
};

enum cccr_bits : u32 {
    CCCR_INIT = bit(0),  // initialization
    CCCR_CCE = bit(1),   // configuration change enable
    CCCR_ASM = bit(2),   // restricted operation mode
    CCCR_CSA = bit(3),   // clock stop acknowledge
    CCCR_CSR = bit(4),   // clock stop request
    CCCR_MON = bit(5),   // bus monitoring mode
    CCCR_DAR = bit(6),   // disable automatic retransmission
    CCCR_TEST = bit(7),  // test mode enable
    CCCR_FDOE = bit(8),  // fd operation enable
    CCCR_BRSE = bit(9),  // bit rate switch enable
    CCCR_UTSU = bit(10), // use timestamping unit
    CCCR_WMM = bit(11),  // wide message marker
    CCCR_PXHD = bit(12), // protocol exception handling disable
    CCCR_EFBI = bit(13), // edge filtering during bus integration
    CCCR_TXP = bit(14),  // transmit pause
    CCCR_NISO = bit(15), // non iso operation

    CCCR_RESET = bit(0),
    CCCR_MASK = CCCR_INIT | CCCR_CCE | CCCR_ASM | CCCR_CSR | CCCR_MON |
                CCCR_DAR | CCCR_TEST | CCCR_FDOE | CCCR_BRSE | CCCR_UTSU |
                CCCR_WMM | CCCR_PXHD | CCCR_EFBI | CCCR_TXP | CCCR_NISO
};

enum nbtp_bits : u32 {
    NBTP_NTSEG2 = bitmask(7),    // nominal time segemnt after sample point
    NBTP_NTSEG1 = bitmask(8, 8), // nominal time segment before sample pt
    NBTP_NBRP = bitmask(9, 16),  // nominal bit rate prescaler
    NBTP_NSJW = bitmask(7, 25),  // nominal (re)sync jump width

    NBTP_RESET = 0x3 << 25 | 0xa << 8 | 0x3 << 0,
    NBTP_MASK = NBTP_NTSEG2 | NBTP_NTSEG1 | NBTP_NBRP | NBTP_NSJW
};

enum tscc_bits : u32 {
    TSCC_TSS = bitmask(2),     // timestamp select
    TSCC_TCP = bitmask(4, 16), // timestamp counter prescaler

    TSCC_MASK = TSCC_TSS | TSCC_TCP
};

typedef field<0, 16, u32> TSCV_TSC; // timestamp counter

enum tscv_bits : u32 {
    TSCV_MASK = TSCV_TSC::MASK,
};

typedef field<1, 2, u32> TOCC_TOS;   // timeout select
typedef field<16, 16, u32> TOCC_TOP; // timeout period

enum tocc_bits : u32 {
    TOCC_ETOC = bit(0), // timeout counter enabled
    TOCC_RESET = 0xffff << 12,
    TOCC_MASK = TOCC_ETOC | TOCC_TOS::MASK | TOCC_TOP::MASK
};

typedef field<0, 16, u32> TOCV_TOC; // timeout counter

enum tocv_bits : u32 { TOCV_RESET = 0xffff << 0, TOCV_MASK = TOCV_TOC::MASK };

enum ecr_bits : u32 {
    ECR_TEC = bitmask(8),    // transmit error counter
    ECR_REC = bitmask(7, 8), // receive error counter
    ECR_RP = bit(15),        // receive error passive
    ECR_CEL = bitmask(8, 16) // cam error logging
};

enum psr_bits : u32 {
    PSR_LEC = bitmask(3),      // last error code
    PSR_ACT = bitmask(2, 3),   // activity
    PSR_EP = bit(5),           // error passive
    PSR_EW = bit(6),           // warning status
    PSR_BO = bit(7),           // bus_off status
    PSR_DLEC = bitmask(3, 8),  // data phase last error code
    PSR_RESI = bit(11),        // esi flag of last received can fd msg
    PSR_RBRS = bit(12),        // brs flag of last received can fd msg
    PSR_RFDF = bit(13),        // received a can fd message
    PSR_PXE = bit(14),         // protocol exception event
    PSR_TDCV = bitmask(7, 16), // transmitter delay compensation value

    PSR_RESET = 0x7 << 0 | 0x7 << 8,
    PSR_MASK = PSR_RESI | PSR_RBRS | PSR_RFDF | PSR_PXE,
};

enum tdcr_bits : u32 {
    TDCR_TDCF = bitmask(7),    // tx delay compensation filter window length
    TDCR_TDCO = bitmask(7, 8), // transmitter delay compensation ssp offset
    TDCR_MASK = TDCR_TDCF | TDCR_TDCO,
};

enum ir_bits : u32 {
    IR_RF0N = bit(0),  // rx fifo 0 new message
    IR_RF0W = bit(1),  // rx fifo 0 watermark reached
    IR_RF0F = bit(2),  // rx fifo 0 full
    IR_RF0L = bit(3),  // rx fifo 0 message lost
    IR_RF1N = bit(4),  // rx fifo 1 new message
    IR_RF1W = bit(5),  // rx fifo 1 watermark reached
    IR_RF1F = bit(6),  // rx fifo 1 full
    IR_RF1L = bit(7),  // rx fifo 1 message lost
    IR_HPM = bit(8),   // high priority message
    IR_TC = bit(9),    // transmission completed
    IR_TCF = bit(10),  // transmission cancellation finished
    IR_TFE = bit(11),  // tx fifo empty
    IR_TEFN = bit(12), // tx event fifo new entry
    IR_TEFW = bit(13), // tx event fifo watermark reached
    IR_TEFF = bit(14), // tx event fifo full
    IR_TEFL = bit(15), // tex event fifo element lost
    IR_TSW = bit(16),  // timestamp wraparound
    IR_MRAF = bit(17), // message ram access failure
    IR_TOO = bit(18),  // timeout occurred
    IR_DRX = bit(19),  // message stored to dedicated rx buffer
    IR_BEC = bit(20),  // bit error corrected
    IR_BEU = bit(21),  // bit error uncorrected
    IR_ELO = bit(22),  // error logging overflow
    IR_EP = bit(23),   // error passive
    IR_EW = bit(24),   // warning statue
    IR_BO = bit(25),   // bus_off status
    IR_WDI = bit(26),  // watchdog interrupt
    IR_PEA = bit(27),  // protocol error in arbitratio phase
    IR_PED = bit(28),  // protocol error in data phase
    IR_TOA = bit(29),  // access to reserved address
};

enum ile_bits : u32 {
    ILE_EINT0 = bit(0), // enable interrupt line 0
    ILE_EINT1 = bit(1), // enable interrupt line 1
    ILE_MASK = ILE_EINT0 | ILE_EINT1,
};

enum gfc_bits : u32 {
    GFC_RRFE = bit(0),        // reject remote frames extended
    GFC_RRFS = bit(1),        // reject remote frames standard
    GFC_ANFE = bitmask(2, 2), // accept non-matching frames extended
    GFC_ANFS = bitmask(2, 4), // accept non-matching frames standard

    GFC_MASK = GFC_RRFE | GFC_RRFS | GFC_ANFE | GFC_ANFS,
};

enum sidfc_bits : u32 {
    SIDFC_FLSSA = bitmask(16),  // filter list standard start address
    SIDFC_LSS = bitmask(8, 16), // list size standard

    SIDFC_MASK = (SIDFC_FLSSA & ~3u) | SIDFC_LSS,
};

enum xidfc_bits : u32 {
    XIDFC_FLESA = bitmask(16),  // filter list extended start address
    XIDFC_LSE = bitmask(7, 16), // list size extended
    XIDFC_MASK = (XIDFC_FLESA & ~3u) | XIDFC_LSE,
};

enum xidam_bits : u32 {
    XIDAM_EIDM = bitmask(29), // extended id mask
    XIDAM_RESET = 0x1fffffff,
    XIDAM_MASK = XIDAM_EIDM,
};

enum hpms_bits : u32 {
    HPMS_BIDX = bitmask(6),    // buffer idx
    HPMS_MSI = bitmask(2, 6),  // message storage indicator
    HPMS_FIDX = bitmask(7, 8), // filter idx
    HPMS_FLST = bit(15),       // filter list
};

typedef field<0, 16, u32> RXFC_FSA; // rx fifo start address
typedef field<16, 7, u32> RXFC_FS;  // rx fifo max elems
typedef field<24, 7, u32> RXFC_FWM; // rx fifo watermark

enum rxfc_bits : u32 {  // rxf0c/rxf1c
    RXFC_FOM = bit(31), // rx fifo operation mode (0: block, 1: overwrite)
    RXFC_MASK = (RXFC_FSA::MASK & ~3u) | RXFC_FS::MASK | RXFC_FWM::MASK |
                RXFC_FOM,
};

typedef field<0, 7, u32> RXFS_FFL;  // rx fifo fill lvl
typedef field<8, 6, u32> RXFS_FGI;  // rx fifo get idx
typedef field<16, 6, u32> RXFS_FPI; // rx fifo put idx

enum rxfs_bits : u32 {  // rxf0s/rxf1
    RXFS_FF = bit(24),  // rx fifo full
    RXFS_RFL = bit(25), // rx fifo message lost
};

enum rxfa_bits : u32 {     // rxf0a/rxf1a
    RXFA_FAI = bitmask(6), // rx fifo acknowledge idx
};

enum rxbc_bits : u32 {
    RXBC_RBSA = bitmask(16), // rx buffer start address
    RXBC_MASK = RXBC_RBSA & ~3u,
};

typedef field<0, 3, u32> RXESC_F0DS; // rx fifo 0 data field size
typedef field<4, 3, u32> RXESC_F1DS; // rx fifo 1 data field size
typedef field<8, 3, u32> RXESC_RBDS; // rx buffer data field size

enum rxesc_bits : u32 {
    RXESC_MASK = RXESC_F0DS::MASK | RXESC_F1DS::MASK | RXESC_RBDS::MASK,
};

typedef field<0, 16, u32> TXBC_TBSA; // tx buffers start address
typedef field<16, 6, u32> TXBC_NDTB; // number of dedicated tx buffers
typedef field<24, 6, u32> TXBC_TFQS; // transmit fifo/queue size

enum txbc_bits : u32 {
    TXBC_TFQM = bit(30), // tx fifo/queue mode
    TXBC_MASK = (TXBC_TBSA::MASK & ~3u) | TXBC_NDTB::MASK | TXBC_TFQS::MASK |
                TXBC_TFQM,
};

enum txfqs_bits : u32 {
    TXFQS_TFFL = bitmask(6),      // tx fifo free lvl
    TXFQS_TFGI = bitmask(5, 8),   // tx fifo get idx
    TXFQS_TFQPI = bitmask(5, 16), // tx fifo/queue put idx
    TXFQS_TFQF = bit(21),         // tx fifo/queue full
};

typedef field<0, 3, u32> TXESC_TBDS;

enum txesc_bits : u32 {
    TXESC_TBDS_MASK = TXESC_TBDS::MASK, // tx buffer data field size
    TXESC_MASK = TXESC_TBDS_MASK,
};

typedef field<0, 16, u32> TXEFC_EFSA; // tx event fifo start address
typedef field<16, 6, u32> TXEFC_EFS;  // tx event fifo size
typedef field<24, 6, u32> TXEFC_EFWM; // tx event fifo watermark

enum txefc_bits : u32 {
    TXEFC_MASK = (TXEFC_EFSA::MASK & ~3u) | TXEFC_EFS::MASK | TXEFC_EFWM::MASK,
};

typedef field<0, 6, u32> TXEFS_EFFL;  // tx event fifo fill lvl
typedef field<8, 5, u32> TXEFS_EFGI;  // tx event fifo get idx
typedef field<16, 5, u32> TXEFS_EFPI; // tx event fifo put idx

enum txefs_bits : u32 {
    TXEFS_EFF = bit(24),  // tx event fifo full
    TXEFS_TEFL = bit(25), // tx event fifo element lost
};

enum txefa_bits : u32 {
    TXEFA_EFAI = bitmask(5), // tx event fifo acknowledge idx
};

// Message ram entry header fields
typedef field<0, 29, u32> BUF_HDR0_ID_XTD;  // frame id extended
typedef field<18, 11, u32> BUF_HDR0_ID_STD; // frame id standard

enum buf_hdr0_bits : u32 {
    BUF_HDR0_RTR = bit(29),
    BUF_HDR0_XTD = bit(30),
    BUF_HDR0_ESI = bit(31),
};

typedef field<8, 8, u32> BUF_HDR1_MM_HI;  // message marker high
typedef field<16, 4, u32> BUF_HDR1_DLC;   // data length code
typedef field<23, 8, u32> BUF_HDR1_MM_LO; // message marker low

enum buf_hdr1_bits : u32 {
    BUF_HDR1_BRS = bit(20),
    BUF_HDR1_FDF = bit(21),
};

enum tx_buf_t0_bits : u32 {
    TXBUF_T0_ID_XTD = BUF_HDR0_ID_XTD::MASK, // identifier extended
    TXBUF_T0_ID_STD = BUF_HDR0_ID_STD::MASK, // identifier standard
    TXBUF_T0_RTR = BUF_HDR0_RTR,             // remote transmission request
    TXBUF_T0_XTD = BUF_HDR0_XTD,             // extended identifier
    TXBUF_T0_ESI = BUF_HDR0_ESI,             // error state indicator
};

enum tx_buf_t1_bits : u32 {
    TXBUF_T1_MM_HI = BUF_HDR1_MM_HI::MASK, // message marker high
    TXBUF_T1_DLC = BUF_HDR1_DLC::MASK,     // data length code
    TXBUF_T1_BRS = BUF_HDR1_BRS,           // bit rate switch
    TXBUF_T1_FDF = BUF_HDR1_FDF,           // fd format
    TXBUF_T1_TSCE = bit(22),               // timestamp capture enable for tsu
    TXBUF_T1_EFC = bit(23),                // tx event fifo control
    TXBUF_T1_MM_LO = BUF_HDR1_MM_LO::MASK, // message marker low
};

enum tx_event_fifo_e0_bits : u32 {
    TXEFF_E0_ID_XTD = BUF_HDR0_ID_XTD::MASK, // identifier extended
    TXEFF_E0_ID_STD = BUF_HDR0_ID_STD::MASK, // identifier standard
};

enum tx_event_fifo_e1a_bits : u32 {
    TXEFF_E1A_TXTS = bitmask(15),           // tx timestamp
    TXEFF_E1A_DLC = BUF_HDR1_DLC::MASK,     // data length code
    TXEFF_E1A_BRS = BUF_HDR1_BRS,           // bit rate switch
    TXEFF_E1A_FDF = BUF_HDR1_FDF,           // fd format
    TXEFF_E1A_ET = bit(22),                 // event type
    TXEFF_E1A_MM_LO = BUF_HDR1_MM_LO::MASK, // message marker low
};

enum tx_event_fifo_e1b_bits : u32 {
    TXEFF_E1B_TXTSP = bitmask(3),           // tx timestamp pointer
    TXEFF_E1B_TSC = bit(4),                 // timestamp captured
    TXEFF_E1B_MM_HI = bitmask(8, 8),        // message marker high
    TXEFF_E1B_DLC = BUF_HDR1_DLC::MASK,     // data length code
    TXEFF_E1B_ET = bit(22),                 // event type
    TXEFF_E1B_MM_LO = BUF_HDR1_MM_LO::MASK, // message marker low
};

enum rx_buf_r0_bits : u32 {
    RXBUF_R0_ID_XTD = BUF_HDR0_ID_XTD::MASK, // identifier extended
    RXBUF_R0_ID_STD = BUF_HDR0_ID_STD::MASK, // identifier standard
};

enum rx_buf_r1a_bits : u32 {
    RXBUF_R1A_TXTS = bitmask(16),
    RXBUF_R1A_DLC_MASK = BUF_HDR1_DLC::MASK, // data length code
    RXBUF_R1A_FIDX = bitmask(7, 21),         // filter idx
    RXBUF_R1A_ANMF = bit(31),                // accepted non-matching frame
};

enum rx_buf_r1b_bits : u32 {
    RXBUF_R1B_RXTSP = bitmask(4),
    RXBUF_R1B_TSC = bit(4),
    RXBUF_R1B_DLC = BUF_HDR1_DLC::MASK, // data length code
    RXBUF_R1B_FIDX = bitmask(7, 21),    // filter idx
    RXBUF_R1B_ANMF = bit(31),           // accepted non-matching frame
};

constexpr size_t TX_BUF_ELEM_HDR_SZ = 8;
constexpr size_t TX_EFIFO_ELEM_SZ = 8;
constexpr size_t RX_BUF_ELEM_HDR_SZ = 8;

bool m_can::is_cfg_allowed() const {
    return (cccr & CCCR_CCE) && (cccr & CCCR_INIT);
}

size_t m_can::get_tx_buf_elems() const {
    size_t tmp = get_tx_buf_dc_elems() + get_tx_buf_fq_elems();
    return tmp < 32 ? tmp : 32;
}

size_t m_can::get_tx_buf_fq_elems() const {
    size_t tmp = txbc.get_field<TXBC_TFQS>();
    return tmp < 32 ? tmp : 32;
}

size_t m_can::get_tx_buf_dc_elems() const {
    size_t tmp = txbc.get_field<TXBC_NDTB>();
    return tmp < 32 ? tmp : 32;
}

bool m_can::is_tx_mode_queue() const {
    return txbc & TXBC_TFQM;
}

size_t m_can::get_tx_evfifo_elems() const {
    size_t tmp = txefc.get_field<TXEFC_EFS>();
    return tmp < 32 ? tmp : 32;
}

void m_can::tx_evfifo_upd_fill_lvl() {
    u8 get_idx = txefs.get_field<TXEFS_EFGI>();
    u8 put_idx = txefs.get_field<TXEFS_EFPI>();
    u8 fill_lvl = put_idx >= get_idx
                      ? put_idx - get_idx
                      : get_tx_evfifo_elems() - get_idx + put_idx;

    txefs.set_field<TXEFS_EFFL>(fill_lvl);

    const u8 watermark = txefc.get_field<TXEFC_EFWM>();
    if ((watermark > 0) && (watermark <= 32) && (fill_lvl >= watermark))
        raise_irq(IR_RF0W);

    if (fill_lvl >= get_tx_evfifo_elems()) {
        txefs |= TXEFS_EFF;
        raise_irq(IR_TEFF);
    } else {
        txefs &= ~TXEFS_EFF;
    }
}

size_t m_can::get_rx_fifo0_elems() const {
    size_t tmp = rxf0c.get_field<RXFC_FS>();
    return tmp < 64 ? tmp : 64;
}

void m_can::rx_fifo0_upd_fill_lvl() {
    size_t get_idx = rxf0s.get_field<RXFS_FGI>();
    size_t put_idx = rxf0s.get_field<RXFS_FPI>();
    size_t fill_lvl = put_idx >= get_idx
                          ? put_idx - get_idx
                          : get_rx_fifo0_elems() - get_idx + put_idx;

    rxf0s.set_field<RXFS_FFL>(fill_lvl);

    size_t watermark = rxf0c.get_field<RXFC_FWM>();
    if (watermark > 0 && watermark <= 64 && fill_lvl >= watermark)
        raise_irq(IR_RF0W);

    if (fill_lvl >= get_rx_fifo0_elems()) {
        rxf0s |= RXFS_FF;
        if (rxf0c & RXFC_FOM) { // overwrite mode
            rxf0s.set_field<RXFS_FGI>(get_idx + 1);
            rxf0s.set_field<RXFS_FPI>(put_idx + 1);
        } else { // blocking mode
            raise_irq(IR_RF0F);
        }
    } else {
        rxf0s &= ~RXFS_FF;
    }
}

void m_can::write_dbtp(u32 val) {
    if (is_cfg_allowed())
        dbtp = val & DBTP_MASK;
}

void m_can::write_test(u32 val) {
    if (cccr & CCCR_TEST)
        test = val & TEST_MASK;
}

void m_can::write_rwd(u32 val) {
    if (is_cfg_allowed())
        rwd = val & RWD_MASK;
}

void m_can::write_cccr(u32 val) {
    if (is_cfg_allowed()) {
        cccr = val & CCCR_MASK;
        if (!(val & CCCR_TEST)) {
            test.reset();
        }
    }

    if (cccr & CCCR_INIT) {
        cccr |= (val & CCCR_CCE);
        if (val & CCCR_CCE) {
            hpms.reset();
            rxf0s.reset();
            rxf1s.reset();
            txfqs.reset();
            txbrp.reset();
            txbto.reset();
            txbcf.reset();
            txefs.reset();
            tocv.set_field<TOCV_TOC>(tocc.get_field<TOCC_TOP>());
        }
        m_tx_rx_enabled = false;
    } else {
        cccr &= ~CCCR_CCE;
        m_tx_rx_enabled = true;
    }

    cccr |= val & CCCR_INIT;
}

void m_can::write_nbtp(u32 val) {
    if (is_cfg_allowed())
        nbtp = val & NBTP_MASK;
}

void m_can::write_tscc(u32 val) {
    if (is_cfg_allowed())
        tscc = val & TSCC_MASK;
}

void m_can::write_tscv(u32 val) {
    tscv.set_field<TSCV_TSC>();
}

void m_can::write_tocc(u32 val) {
    if (is_cfg_allowed())
        tocc = val & TOCC_MASK;
}

void m_can::write_tocv(u32 val) {
    tocv = (tocv & ~(val & TOCV_MASK)) | TOCV_RESET;
}

void m_can::write_tdcr(u32 val) {
    if (is_cfg_allowed())
        tdcr = val & TDCR_MASK;
}

void m_can::write_ir(u32 val) {
    ir &= ~val;

    if (val & IR_TEFL)
        txefs &= ~TXEFS_TEFL;
    if (val & IR_RF0L)
        rxf0s &= ~RXFS_RFL;

    irq0.lower();
    irq1.lower();
}

void m_can::write_gfc(u32 val) {
    if (is_cfg_allowed())
        gfc = val & GFC_MASK;
}

void m_can::write_sidfc(u32 val) {
    if (is_cfg_allowed())
        sidfc = val & SIDFC_MASK;
}

void m_can::write_xidfc(u32 val) {
    if (is_cfg_allowed())
        xidfc = val & XIDFC_MASK;
}

void m_can::write_xidam(u32 val) {
    if (is_cfg_allowed())
        xidam = val & XIDAM_MASK;
}

void m_can::write_rxf0c(u32 val) {
    if (is_cfg_allowed())
        rxf0c = val & RXFC_MASK;
}

void m_can::write_rxf0a(u32 val) {
    rxf0a = (val & RXFA_FAI);
    rxf0s.set_field<RXFS_FGI>(((rxf0a & RXFA_FAI) + 1) % get_rx_fifo0_elems());

    rx_fifo0_upd_fill_lvl();
}

void m_can::write_rxbc(u32 val) {
    if (is_cfg_allowed())
        rxbc = val & RXBC_MASK;
}

void m_can::write_rxf1c(u32 val) {
    if (is_cfg_allowed())
        rxf1c = val & RXFC_MASK;
}

void m_can::write_rxesc(u32 val) {
    if (is_cfg_allowed()) {
        rxesc = val & RXESC_MASK;

        m_rx_buf_elem_data_sz = dlc2len(rxesc.get_field<RXESC_RBDS>() + 8);
        m_rx_fifo0_elem_data_sz = dlc2len(rxesc.get_field<RXESC_F0DS>() + 8);
        m_rx_fifo1_elem_data_sz = dlc2len(rxesc.get_field<RXESC_F1DS>() + 8);
    }
}

void m_can::write_txbc(u32 val) {
    if (is_cfg_allowed())
        txbc = val & TXBC_MASK;
}

void m_can::write_txesc(u32 val) {
    if (is_cfg_allowed()) {
        txesc = val & TXESC_MASK;
        m_tx_buf_elem_data_sz = dlc2len(txesc.get_field<TXESC_TBDS>() + 8);
    }
}

void m_can::write_txbar(u32 val) {
    txbto &= ~val;
    txbar |= val;

    if (m_tx_rx_enabled)
        m_txev.notify(SC_ZERO_TIME);
}

void m_can::write_txbcr(u32 val) {
    if (cccr & CCCR_CCE)
        txbcr = val;
}

void m_can::write_txefc(u32 val) {
    if (is_cfg_allowed())
        txefc = val & TXEFC_MASK;
}

void m_can::write_txefa(u32 val) {
    txefa = (val & TXEFA_EFAI);

    txefs.set_field<TXEFS_EFGI>(((txefa & TXEFA_EFAI) + 1) %
                                get_tx_evfifo_elems());
    tx_evfifo_upd_fill_lvl();
}

u32 m_can::read_ecr() {
    u32 val = ecr;
    ecr &= ~ECR_CEL;
    return val;
}

u32 m_can::read_psr() {
    u32 val = psr;
    psr &= ~PSR_MASK;
    return val;
}

void m_can::raise_irq(u32 val) {
    u32 enabled_irqs = val & ie;
    ir |= enabled_irqs;

    if ((ile & ILE_EINT0) && (~ils & enabled_irqs))
        irq0.raise();

    if ((ile & ILE_EINT1) && (ils & enabled_irqs))
        irq1.raise();
}

void m_can::add_txevent(const u32 tx_buf_elem_hdr[2]) {
    if (txefs & TXEFS_EFF) {
        txefs |= TXEFS_TEFL;
        raise_irq(IR_TEFL);
        return;
    }

    u8 put_idx = txefs.get_field<TXEFS_EFPI>();
    u64 addr = msg_ram_addr.get().start + txefc.get_field<TXEFC_EFSA>() +
               put_idx * TX_EFIFO_ELEM_SZ;

    u32 tx_efifo_elem[2] = {};
    tx_efifo_elem[0] = tx_buf_elem_hdr[0];
    if ((cccr & CCCR_WMM) || (cccr & CCCR_UTSU)) { // E1B
        tx_efifo_elem[1] = (tx_buf_elem_hdr[1] &
                            (TXBUF_T1_MM_HI | TXBUF_T1_DLC | TXBUF_T1_BRS |
                             TXBUF_T1_FDF | TXBUF_T1_MM_LO)) |
                           TXEFF_E1B_ET;
    } else { // E1A
        tx_efifo_elem[1] = (tx_buf_elem_hdr[1] &
                            (TXBUF_T1_DLC | TXBUF_T1_BRS | TXBUF_T1_FDF |
                             TXBUF_T1_MM_LO)) |
                           TXEFF_E1A_ET;
    }

    if (failed(dma.writew(addr, tx_efifo_elem)))
        log_warn("failed to write txevent data to 0x%llx", addr);

    txefs.set_field<TXEFS_EFPI>((put_idx + 1) % get_tx_evfifo_elems());
    tx_evfifo_upd_fill_lvl();
    raise_irq(IR_TEFN);
}

void m_can::txthread() {
    while (true) {
        wait(m_txev);

        if (!m_tx_rx_enabled)
            continue;

        for (size_t idx = 0; idx < get_tx_buf_elems(); idx++) {
            if ((txbar & bit(idx)) == 0)
                continue;

            u32 tx_buf_elem_hdr[2] = {};
            u64 addr = msg_ram_addr.get().start + txbc.get_field<TXBC_TBSA>() +
                       idx * (TX_BUF_ELEM_HDR_SZ + m_tx_buf_elem_data_sz);
            if (failed(dma.readw(addr, tx_buf_elem_hdr))) {
                log_error("failed to access message %zu header", idx);
                continue;
            }

            can_frame tx{};
            if (tx_buf_elem_hdr[0] & BUF_HDR0_XTD) {
                tx.msgid = get_field<BUF_HDR0_ID_XTD>(tx_buf_elem_hdr[0]);
                tx.msgid |= CAN_EFF;
            } else {
                tx.msgid = get_field<BUF_HDR0_ID_STD>(tx_buf_elem_hdr[0]);
            }

            if (tx_buf_elem_hdr[0] & BUF_HDR0_RTR)
                tx.msgid |= CAN_RTR;

            tx.dlc = get_field<BUF_HDR1_DLC>(tx_buf_elem_hdr[1]);
            tx.flags = 0;

            if (tx_buf_elem_hdr[1] & BUF_HDR1_FDF) {
                tx.flags |= CANFD_FDF;
                if (tx_buf_elem_hdr[1] & BUF_HDR1_BRS)
                    tx.flags |= CANFD_BRS;
                if (tx_buf_elem_hdr[0] & BUF_HDR0_ESI)
                    tx.flags |= CANFD_ESI;
            }

            addr += TX_BUF_ELEM_HDR_SZ;

            if (failed(dma.read(addr, tx.data, tx.length())))
                log_error("failed to read message %zu data", idx);

            can_tx.send(tx);

            txbar &= ~bit(idx);
            txbto |= bit(idx);

            if (tx_buf_elem_hdr[1] & TXBUF_T1_EFC)
                add_txevent(tx_buf_elem_hdr);
        }

        if (txbtie & txbto)
            raise_irq(IR_TC);
    }
}

void m_can::rxthread() {
    while (true) {
        wait(m_rxev);

        can_frame rx{};
        while (can_rx_pop(rx)) {
            if (!m_tx_rx_enabled || get_tx_buf_elems() == 0) {
                log_debug("rx disabled, frame dropped");
                break;
            }

            if (rxf0s & RXFS_FF) {
                rxf0s |= RXFS_RFL;
                raise_irq(IR_RF0L);
                break;
            }

            size_t put_idx = rxf0s.get_field<RXFS_FPI>();
            u64 addr = msg_ram_addr.get().start + rxf0c.get_field<RXFC_FSA>();
            u32 rx_buf_elem_hdr[2] = {};

            if (rx.is_eff()) {
                set_field<BUF_HDR0_ID_XTD>(rx_buf_elem_hdr[0], rx.msgid);
                rx_buf_elem_hdr[0] |= BUF_HDR0_XTD;
            } else {
                set_field<BUF_HDR0_ID_STD>(rx_buf_elem_hdr[0], rx.msgid);
            }

            set_bit<BUF_HDR0_RTR>(rx_buf_elem_hdr[0], rx.is_rtr());
            set_bit<BUF_HDR1_FDF>(rx_buf_elem_hdr[1], rx.is_fdf());
            set_field<BUF_HDR1_DLC>(rx_buf_elem_hdr[1], rx.dlc);

            addr += put_idx * (RX_BUF_ELEM_HDR_SZ + m_rx_fifo0_elem_data_sz);
            if (failed(dma.writew(addr, rx_buf_elem_hdr))) {
                log_warn("DMA access failed at 0x%llx", addr);
                break;
            }

            addr += RX_BUF_ELEM_HDR_SZ;
            if (failed(dma.write(addr, rx.data, m_rx_fifo0_elem_data_sz))) {
                log_warn("DMA access failed at 0x%llx", addr);
                break;
            }

            rxf0s.set_field<RXFS_FPI>((put_idx + 1) % get_rx_fifo0_elems());
            rx_fifo0_upd_fill_lvl();
            raise_irq(IR_RF0N);
        }
    }
}

m_can::m_can(const sc_module_name& nm, const range& msg_ram):
    peripheral(nm),
    m_tx_rx_enabled(false),
    m_tx_buf_elem_data_sz(0),
    m_rx_buf_elem_data_sz(0),
    m_rx_fifo0_elem_data_sz(0),
    m_rx_fifo1_elem_data_sz(0),
    m_txev("txev"),
    m_rxev("rxev"),
    crel("crel", 0x00, CREL_RESET),
    endn("endn", 0x04, ENDN_RESET),
    dbtp("dbtp", 0x0c, DBTP_RESET),
    test("test", 0x10, 0x00000000),
    rwd("rwd", 0x14, 0x00000000),
    cccr("cccr", 0x18, CCCR_RESET),
    nbtp("nbtp", 0x1c, NBTP_RESET),
    tscc("tscc", 0x20, 0x00000000),
    tscv("tscv", 0x24, 0x00000000),
    tocc("tocc", 0x28, TOCC_RESET),
    tocv("tocv", 0x2c, TOCV_RESET),
    ecr("ecr", 0x40, 0x00000000),
    psr("psr", 0x44, PSR_RESET),
    tdcr("tdcr", 0x48, 0x00000000),
    ir("ir", 0x50, 0x00000000),
    ie("ie", 0x54, 0x00000000),
    ils("ils", 0x58, 0x00000000),
    ile("ile", 0x5c, 0x00000000),
    gfc("gfc", 0x80, 0x00000000),
    sidfc("sidfc", 0x84, 0x00000000),
    xidfc("xidfc", 0x88, 0x00000000),
    xidam("xidam", 0x90, XIDAM_RESET),
    hpms("hpms", 0x94, 0x00000000),
    ndat1("ndat1", 0x98, 0x00000000),
    ndat2("ndat2", 0x9c, 0x00000000),
    rxf0c("rxf0c", 0xa0, 0x00000000),
    rxf0s("rxf0s", 0xa4, 0x00000000),
    rxf0a("rxf0a", 0xa8, 0x00000000),
    rxbc("rxbc", 0xac, 0x00000000),
    rxf1c("rxf1c", 0xb0, 0x00000000),
    rxf1s("rxf1s", 0xb4, 0x00000000),
    rxf1a("rxf1a", 0xb8, 0x00000000),
    rxesc("rxesc", 0xbc, 0x00000000),
    txbc("txbc", 0xc0, 0x00000000),
    txfqs("txfqs", 0xc4, 0x00000000),
    txesc("txesc", 0xc8, 0x00000000),
    txbrp("txbrp", 0xcc, 0x00000000),
    txbar("txbar", 0xd0, 0x00000000),
    txbcr("txbcr", 0xd4, 0x00000000),
    txbto("txbto", 0xd8, 0x00000000),
    txbcf("txbcf", 0xdc, 0x00000000),
    txbtie("txbtie", 0xe0, 0x00000000),
    txbcie("txbcie", 0xe4, 0x00000000),
    txefc("txefc", 0xf0, 0x00000000),
    txefs("txefs", 0xf4, 0x00000000),
    txefa("txefa", 0xf8, 0x00000000),
    msg_ram_addr("msg_ram_addr", msg_ram),
    irq0("irq0"),
    irq1("irq1"),
    in("in"),
    dma("dma"),
    can_tx("can_tx"),
    can_rx("can_rx") {
    crel.sync_never();
    crel.allow_read_only();

    endn.sync_never();
    endn.allow_read_only();

    dbtp.sync_always();
    dbtp.allow_read_write();
    dbtp.on_write(&m_can::write_dbtp);

    test.sync_always();
    test.allow_read_write();
    test.on_write(&m_can::write_test);

    rwd.sync_always();
    rwd.allow_read_write();
    rwd.on_write(&m_can::write_rwd);

    cccr.sync_always();
    cccr.allow_read_write();
    cccr.on_write(&m_can::write_cccr);

    nbtp.sync_always();
    nbtp.allow_read_write();
    nbtp.on_write(&m_can::write_nbtp);

    tscc.sync_always();
    tscc.allow_read_write();
    tscc.on_write(&m_can::write_tscc);

    tscv.sync_always();
    tscv.allow_read_write();
    tscv.on_write(&m_can::write_tscv);

    tocc.sync_always();
    tocc.allow_read_write();
    tocc.on_write(&m_can::write_tocc);

    tocv.sync_always();
    tocv.allow_read_write();
    tocv.on_write(&m_can::write_tocv);

    ecr.sync_always();
    ecr.allow_read_write();
    ecr.on_read(&m_can::read_ecr);

    psr.sync_always();
    psr.allow_read_only();
    psr.on_read(&m_can::read_psr);

    tdcr.sync_always();
    tdcr.allow_read_write();
    tdcr.on_write(&m_can::write_tdcr);

    ir.sync_always();
    ir.allow_read_write();
    ir.on_write(&m_can::write_ir);

    ie.sync_always();
    ie.allow_read_write();

    ils.sync_always();
    ils.allow_read_write();

    ile.sync_always();
    ile.allow_read_write();

    gfc.sync_always();
    gfc.allow_read_write();
    gfc.on_write(&m_can::write_gfc);

    sidfc.sync_always();
    sidfc.allow_read_write();
    sidfc.on_write(&m_can::write_sidfc);

    xidfc.sync_always();
    xidfc.allow_read_write();
    xidfc.on_write(&m_can::write_xidfc);

    xidam.sync_always();
    xidam.allow_read_write();
    xidam.on_write(&m_can::write_xidam);

    hpms.sync_on_read();
    hpms.allow_read_only();

    ndat1.sync_never();
    ndat1.allow_read_write();

    ndat2.sync_never();
    ndat2.allow_read_write();

    rxf0c.sync_always();
    rxf0c.allow_read_write();
    rxf0c.on_write(&m_can::write_rxf0c);

    rxf0s.sync_always();
    rxf0s.allow_read_only();

    rxf0a.sync_always();
    rxf0a.allow_read_write();
    rxf0a.on_write(&m_can::write_rxf0a);

    rxbc.sync_always();
    rxbc.allow_read_write();
    rxbc.on_write(&m_can::write_rxbc);

    rxf1c.sync_always();
    rxf1c.allow_read_write();
    rxf1c.on_write(&m_can::write_rxf1c);

    rxf1s.sync_on_read();
    rxf1s.allow_read_only();

    rxf1a.sync_always();
    rxf1a.allow_read_write();

    rxesc.sync_always();
    rxesc.allow_read_write();
    rxesc.on_write(&m_can::write_rxesc);

    txbc.sync_always();
    txbc.allow_read_write();
    txbc.on_write(&m_can::write_txbc);

    txfqs.sync_on_read();
    txfqs.allow_read_only();

    txesc.sync_always();
    txesc.allow_read_write();
    txesc.on_write(&m_can::write_txesc);

    txbrp.sync_on_read();
    txbrp.allow_read_only();

    txbar.sync_always();
    txbar.allow_read_write();
    txbar.on_write(&m_can::write_txbar);

    txbcr.sync_always();
    txbcr.allow_read_write();
    txbcr.on_write(&m_can::write_txbcr);

    txbto.sync_on_read();
    txbto.allow_read_only();

    txbcf.sync_on_read();
    txbcf.allow_read_only();

    txbtie.sync_always();
    txbtie.allow_read_write();

    txbcie.sync_always();
    txbcie.allow_read_write();

    txefc.sync_always();
    txefc.allow_read_write();
    txefc.on_write(&m_can::write_txefc);

    txefs.sync_on_read();
    txefs.allow_read_only();

    txefa.sync_always();
    txefa.allow_read_write();
    txefa.on_write(&m_can::write_txefa);

    SC_HAS_PROCESS(m_can);
    SC_THREAD(txthread);
    SC_THREAD(rxthread);
}

m_can::~m_can() {
    // nothing to do
}

void m_can::reset() {
    peripheral::reset();
}

void m_can::can_receive(const can_target_socket& socket, can_frame& rx) {
    can_host::can_receive(socket, rx);
    m_rxev.notify(SC_ZERO_TIME);
}

VCML_EXPORT_MODEL(vcml::can::m_can, name, args) {
    range msgram{ 0, 0x3fff };
    if (!args.empty())
        msgram = from_string<range>(args[0]);
    return new m_can(name, msgram);
}

} // namespace can
} // namespace vcml
