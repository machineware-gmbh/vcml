/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/ocspi.h"

namespace vcml {
namespace spi {

void ocspi::write_txdata(u8 val) {
    txdata = rxdata.get();

    status &= ~STATUS_TXR;
    status &= ~STATUS_TXE;

    spi_payload spi(val);

    spi_out.transport(spi);
    rxdata = spi.miso;

    status |= STATUS_TXR;
    if (m_txr_irq && !irq) {
        log_debug("setting TXR interrupt");
        irq = true;
    }

    status |= STATUS_TXE;
    if (m_txe_irq && !irq) {
        log_debug("setting TXE interrupt");
        irq = true;
    }
}

void ocspi::write_status(u8 val) {
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

    if (irq.read() && !m_txe_irq && !m_txr_irq) {
        log_debug("clearing interrupt");
        irq = false;
    }

    if (!irq.read() && m_txe_irq && (status & STATUS_TXE)) {
        log_debug("setting TXE interrupt");
        irq = true;
    }

    if (!irq.read() && m_txr_irq && (status & STATUS_TXR)) {
        log_debug("setting TXR interrupt");
        irq = true;
    }

    status = val;
}

void ocspi::write_control(u32 val) {
    if (val >= 4) {
        log_warn("invalid mode specified SPI%u", val);
        val = 0;
    }

    if (val != control)
        log_debug("changed mode to SPI%u", val);

    control = val;
}

void ocspi::write_bauddiv(u32 val) {
    if (val != bauddiv) {
        unsigned int divider = val + 1;
        log_debug("changed transmission speed to %lu Hz", clock / divider);
    }

    bauddiv = val;
}

ocspi::ocspi(const sc_module_name& nm):
    peripheral(nm),
    m_txe_irq(false),
    m_txr_irq(false),
    rxdata("rxdata", 0x0, 0),
    txdata("txdata", 0x4, 0),
    status("status", 0x8, STATUS_TXR | STATUS_TXE),
    control("control", 0xC, 0),
    bauddiv("bauddiv", 0x10, 0),
    irq("irq"),
    in("in"),
    spi_out("spi_out"),
    clock("clock", 50 * MHz) {
    rxdata.sync_always();
    rxdata.allow_read_only();

    txdata.sync_always();
    txdata.allow_read_write();
    txdata.on_write(&ocspi::write_txdata);

    status.allow_read_write();
    status.on_write(&ocspi::write_status);

    control.allow_read_write();
    control.on_write(&ocspi::write_control);

    bauddiv.allow_read_write();
    bauddiv.on_write(&ocspi::write_bauddiv);
}

ocspi::~ocspi() {
    // nothing to do
}

void ocspi::reset() {
    peripheral::reset();
}

VCML_EXPORT_MODEL(vcml::spi::ocspi, name, args) {
    return new ocspi(name);
}

} // namespace spi
} // namespace vcml
