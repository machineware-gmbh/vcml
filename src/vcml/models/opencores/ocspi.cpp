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

#include "vcml/models/opencores/ocspi.h"

namespace vcml { namespace opencores {

    u8 ocspi::read_RXDATA() {
        log_debug("read RXDATA = %02x", (unsigned int)RXDATA);
        return RXDATA;
    }

    u8 ocspi::read_TXDATA() {
        log_debug("read TXDATA = %02x", (unsigned int)TXDATA);
        return TXDATA;
    }

    u8 ocspi::write_TXDATA(u8 val) {
        TXDATA = RXDATA.get();

        STATUS &= ~STATUS_TXR;
        STATUS &= ~STATUS_TXE;

        trace_fw(val);
        RXDATA = SPI_OUT->spi_transport(val);
        trace_bw(m_shift_reg);

        STATUS |= STATUS_TXR;
        if (m_txr_irq && !IRQ) {
            log_debug("setting TXR interrupt");
            IRQ = true;
        }

        STATUS |= STATUS_TXE;
        if (m_txe_irq && !IRQ) {
            log_debug("setting TXE interrupt");
            IRQ = true;
        }

        return TXDATA;
    }

    u8 ocspi::write_STATUS(u8 val) {
        if (m_txe_irq && !(val & STATUS_TXE))
            log_debug("disabling TXE interrupt");
        if (!m_txe_irq && (val & STATUS_TXE))
            log_debug("enabling TXE interrupt");

        if (m_txr_irq && !(val & STATUS_TXR))
            log_debug("disabling TXR interrupt");
        if (!m_txr_irq && (val & STATUS_TXR))
            log_debug("enabling TXR interrupt");

        m_txe_irq = val & STATUS_TXE;
        m_txr_irq = val & STATUS_TXR;

        if (IRQ.read() && !m_txe_irq && !m_txr_irq) {
            log_debug("clearing interrupt");
            IRQ = false;
        }

        if (!IRQ.read() && m_txe_irq && (STATUS & STATUS_TXE)) {
            log_debug("setting TXE interrupt");
            IRQ = true;
        }

        if (!IRQ.read() && m_txr_irq && (STATUS & STATUS_TXR)) {
            log_debug("setting TXR interrupt");
            IRQ = true;
        }

        return STATUS;
    }

    u32 ocspi::write_CONTROL(u32 val) {
        if (val >= 4) {
            log_warn("invalid mode specified SPI%u", val);
            val = 0;
        }

        if (val != CONTROL)
            log_debug("changed mode to SPI%u", val);

        return val;
    }

    u32 ocspi::write_BAUDDIV(u32 val) {
        if (val != BAUDDIV) {
            unsigned int divider = val + 1;
            log_debug("changed transmission speed to %lu Hz", clock / divider);
        }

        return val;
    }

    ocspi::ocspi(const sc_module_name& nm):
        peripheral(nm),
        m_shift_reg(0xff),
        m_txe_irq(false),
        m_txr_irq(false),
        RXDATA("RXDATA", 0x0, 0),
        TXDATA("TXDATA", 0x4, 0),
        STATUS("STATUS", 0x8, STATUS_TXR | STATUS_TXE),
        CONTROL("CONTROL", 0xC, 0),
        BAUDDIV("BAUDDIV", 0x10, 0),
        IRQ("IRQ"),
        IN("IN"),
        SPI_OUT("SPI_OUT"),
        clock("clock", 50000000) { /* 50 MHz default clock */
        SPI_OUT.bind(*this);

        RXDATA.allow_read_only();
        RXDATA.read = &ocspi::read_RXDATA;

        TXDATA.allow_read_write();
        TXDATA.read = &ocspi::read_TXDATA;
        TXDATA.write = &ocspi::write_TXDATA;

        STATUS.allow_read_write();
        STATUS.write = &ocspi::write_STATUS;

        CONTROL.allow_read_write();
        CONTROL.write = &ocspi::write_CONTROL;

        BAUDDIV.allow_read_write();
        BAUDDIV.write = &ocspi::write_BAUDDIV;
    }

    ocspi::~ocspi() {
        /* nothing to do */
    }

    void ocspi::reset() {
        peripheral::reset();
    }

}}
