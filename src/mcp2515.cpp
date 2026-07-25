// Minimal MCP2515 driver over Pico SDK SPI. 8 MHz crystal.
#include "psa/mcp2515.hpp"
#include "psa/isotp.hpp"   // CanFrame
#include <cstring>

namespace psa {

// 8 MHz crystal CNF presets (autowp/arduino-mcp2515, interop-tested vs PSA ECUs).
//   125k: CNF1=0x01 CNF2=0xB1 CNF3=0x85  (16 TQ, sample 62.5%)
//   500k: CNF1=0x00 CNF2=0x90 CNF3=0x82  ( 8 TQ, sample 62.5%)
struct Cnf { uint8_t c1, c2, c3; };
static constexpr Cnf kCnf8MHz125k = {0x01, 0xB1, 0x85};
static constexpr Cnf kCnf8MHz500k = {0x00, 0x90, 0x82};

static constexpr uint8_t REQOP_CONFIG     = 0x80;
static constexpr uint8_t REQOP_LISTENONLY = 0x60;
static constexpr uint8_t REQOP_NORMAL     = 0x00;
static constexpr uint8_t CANSTAT_OPMOD    = 0xE0;
static constexpr uint8_t CANINTF_RX0IF    = 0x01;
static constexpr uint8_t CANINTF_RX1IF    = 0x02;
static constexpr uint8_t STAT_RX0IF       = 0x01;
static constexpr uint8_t STAT_RX1IF       = 0x02;
static constexpr uint8_t RXBnCTRL_RXM_STDEXT = 0x00;
static constexpr uint8_t RXBnCTRL_RXM_MASK   = 0x60;
static constexpr uint8_t RXB0CTRL_BUKT       = 0x04;

void Mcp2515::csLow()  { gpio_put(pins_.cs, 0); }
void Mcp2515::csHigh() { gpio_put(pins_.cs, 1); }

void Mcp2515::writeReg(Reg r, uint8_t v) {
    csLow();
    spi_buf_[0]=INSTR_WRITE; spi_buf_[1]=r; spi_buf_[2]=v;
    spi_write_blocking(pins_.spi, spi_buf_, 3);
    csHigh();
}
uint8_t Mcp2515::readReg(Reg r) {
    csLow();
    spi_buf_[0]=INSTR_READ; spi_buf_[1]=r;
    spi_write_blocking(pins_.spi, spi_buf_, 2);
    uint8_t v = 0;
    spi_read_blocking(pins_.spi, 0, &v, 1);
    csHigh();
    return v;
}
void Mcp2515::bitMod(Reg r, uint8_t mask, uint8_t bits) {
    csLow();
    spi_buf_[0]=INSTR_BITMOD; spi_buf_[1]=r; spi_buf_[2]=mask; spi_buf_[3]=bits;
    spi_write_blocking(pins_.spi, spi_buf_, 4);
    csHigh();
}
void Mcp2515::writeBytes(Reg r, const uint8_t* b, size_t n) {
    csLow();
    spi_buf_[0]=INSTR_WRITE; spi_buf_[1]=r;
    spi_write_blocking(pins_.spi, spi_buf_, 2);
    spi_write_blocking(pins_.spi, b, n);
    csHigh();
}
void Mcp2515::readBytes(Reg r, uint8_t* b, size_t n) {
    csLow();
    spi_buf_[0]=INSTR_READ; spi_buf_[1]=r;
    spi_write_blocking(pins_.spi, spi_buf_, 2);
    spi_read_blocking(pins_.spi, 0, b, n);
    csHigh();
}

McpError Mcp2515::setMode(uint8_t reqop) {
    bitMod(MCP_CANCTRL, 0xE0, reqop);
    for (int i = 0; i < 40; ++i) {                 // ~10ms worst case at 4MHz core
        if ((readReg(MCP_CANSTAT) & CANSTAT_OPMOD) == reqop) return McpError::Ok;
        sleep_ms(1);
    }
    return McpError::Fail;
}

McpError Mcp2515::init(CanBitrate br) {
    // SPI + GPIO setup
    spi_init(pins_.spi, 8'000'000);
    spi_set_format(pins_.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(pins_.sck,  GPIO_FUNC_SPI);
    gpio_set_function(pins_.mosi, GPIO_FUNC_SPI);
    gpio_set_function(pins_.miso, GPIO_FUNC_SPI);
    gpio_init(pins_.cs); gpio_set_dir(pins_.cs, GPIO_OUT); gpio_put(pins_.cs, 1);
    gpio_init(pins_.interrupt); gpio_set_dir(pins_.interrupt, GPIO_IN);
    gpio_pull_up(pins_.interrupt);   // MCP2515 INT is active-low

    // Hardware reset via SPI reset command
    csLow();
    spi_buf_[0] = INSTR_RESET;
    spi_write_blocking(pins_.spi, spi_buf_, 1);
    csHigh();
    sleep_ms(10);

    if (setMode(REQOP_CONFIG) != McpError::Ok) return McpError::Fail;

    const Cnf& cnf = (br == CanBitrate::Bps500k) ? kCnf8MHz500k : kCnf8MHz125k;
    writeReg(MCP_CNF1, cnf.c1);
    writeReg(MCP_CNF2, cnf.c2);
    writeReg(MCP_CNF3, cnf.c3);

    // Descending transmit priority: TXB0 > TXB1 > TXB2. At the reset value all
    // three sit at the same priority, and the datasheet then transmits the
    // HIGHEST-numbered buffer first — which reversed the consecutive frames of
    // every multi-frame ISO-TP request and made the ECU reject it.
    bitMod(MCP_TXB0CTRL, 0x03, 0x03);
    bitMod(MCP_TXB1CTRL, 0x03, 0x02);
    bitMod(MCP_TXB2CTRL, 0x03, 0x01);

    // RX interrupts on, roll-over RXB0 -> RXB1
    writeReg(MCP_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF);
    bitMod(MCP_RXB0CTRL, RXBnCTRL_RXM_MASK | RXB0CTRL_BUKT,
                       RXBnCTRL_RXM_STDEXT | RXB0CTRL_BUKT);
    bitMod(MCP_RXB1CTRL, RXBnCTRL_RXM_MASK, RXBnCTRL_RXM_STDEXT);

    setSnifferFilters();
    return McpError::Ok;
}

void Mcp2515::setSnifferFilters() {
    // masks=0, filters=0 -> receive everything (sniffer mode)
    uint8_t zeros[4] = {0,0,0,0};
    writeBytes(MCP_RXM0SIDH, zeros, 4);
    writeBytes(MCP_RXM1SIDH, zeros, 4);
    static constexpr Reg filt[] = {
        (Reg)0x00,(Reg)0x04,(Reg)0x08,(Reg)0x10,(Reg)0x14,(Reg)0x18
    };
    for (Reg r : filt) writeBytes(r, zeros, 4);
}

uint8_t Mcp2515::errorFlags() { return readReg(MCP_EFLG); }

void Mcp2515::recoverBus() {
    // ABAT aborts every pending transmission; it must be released again or the
    // next RTS is aborted too. RX-overflow bits are latched and have to be
    // cleared by hand, otherwise the buffers stay marked full forever.
    bitMod(MCP_CANCTRL, 0x10, 0x10);
    bitMod(MCP_CANCTRL, 0x10, 0x00);
    static constexpr Reg tx_ctrl[3] = { MCP_TXB0CTRL, MCP_TXB1CTRL, MCP_TXB2CTRL };
    for (Reg r : tx_ctrl) bitMod(r, 0x08, 0x00);      // clear TXREQ
    bitMod(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0x00);
}

McpError Mcp2515::setNormalMode()     { return setMode(REQOP_NORMAL); }
McpError Mcp2515::setListenOnlyMode() { return setMode(REQOP_LISTENONLY); }
McpError Mcp2515::setLoopbackMode() {
    static constexpr uint8_t REQOP_LOOPBACK = 0x40;
    return setMode(REQOP_LOOPBACK);
}

bool Mcp2515::hasRx() {
    // INT pin low => an interrupt is pending. Cheap poll, no status read needed.
    return gpio_get(pins_.interrupt) == 0;
}

McpError Mcp2515::read(CanFrame& out) {
    uint8_t stat = 0;
    csLow();
    spi_buf_[0] = INSTR_READ_STATUS;
    spi_write_blocking(pins_.spi, spi_buf_, 1);
    spi_read_blocking(pins_.spi, 0, &stat, 1);
    csHigh();

    Reg sidh; Reg dreg; uint8_t rxb_if;
    if (stat & STAT_RX0IF)      { sidh = MCP_RXB0SIDH; dreg = MCP_RXB0DATA; rxb_if = CANINTF_RX0IF; }
    else if (stat & STAT_RX1IF) { sidh = MCP_RXB1SIDH; dreg = MCP_RXB1DATA; rxb_if = CANINTF_RX1IF; }
    else return McpError::NoMsg;

    uint8_t hdr[5];
    readBytes(sidh, hdr, 5);
    uint32_t id = (static_cast<uint32_t>(hdr[0]) << 3) | (hdr[1] >> 5);
    bool ext = (hdr[1] & 0x08) != 0;
    if (ext) {
        id = (id << 2) | (hdr[1] & 0x03);
        id = (id << 8) | hdr[2];
        id = (id << 8) | hdr[3];
    }
    out.id  = id;
    out.ext = ext;
    out.dlc = hdr[4] & 0x0F;
    if (out.dlc > 8) out.dlc = 8;
    readBytes(dreg, out.data, out.dlc);
    bitMod(MCP_CANINTF, rxb_if, 0);
    return McpError::Ok;
}

McpError Mcp2515::send(const CanFrame& f) {
    // pick first free TX buffer (TXB0..TXB2)
    static const Reg ctrl[3] = { MCP_TXB0CTRL, MCP_TXB1CTRL, MCP_TXB2CTRL };
    static const Reg sidh[3] = { MCP_TXB0SIDH, MCP_TXB1SIDH, MCP_TXB2SIDH };
    static const Reg data[3] = { MCP_TXB0DATA, MCP_TXB1DATA, MCP_TXB2DATA };
    static const uint8_t rts[3] = { INSTR_RTS_TX0, INSTR_RTS_TX1, INSTR_RTS_TX2 };

    int idx = -1;
    for (int i = 0; i < 3; ++i)
        if ((readReg(ctrl[i]) & 0x08) == 0) { idx = i; break; }  // TXREQ == 0
    if (idx < 0) {
        // Every buffer still latched. Either the bus is saturated, or — far more
        // likely on a bench or with the ignition off — nothing is acknowledging
        // us, so TXREQ never clears and the transmitter would stay dead until a
        // power cycle. Clear the queue once the chip admits it has given up.
        if (errorFlags() & (EFLG_TXBO | EFLG_TXEP)) recoverBus();
        return McpError::AllTxBusy;
    }

    uint8_t buf[5];
    if (f.ext) {
        uint32_t id = f.id;
        buf[0] = (id >> 21) & 0xFF;
        buf[1] = ((id >> 13) & 0xE0) | ((id >> 16) & 0x03) | 0x08;
        buf[2] = (id >> 8) & 0xFF;
        buf[3] = id & 0xFF;
    } else {
        buf[0] = (f.id >> 3) & 0xFF;
        buf[1] = (f.id & 0x07) << 5;
        buf[2] = 0; buf[3] = 0;
    }
    buf[4] = f.dlc;
    writeBytes(sidh[idx], buf, 5);
    writeBytes(data[idx], f.data, f.dlc);

    csLow();
    spi_buf_[0] = rts[idx];
    spi_write_blocking(pins_.spi, spi_buf_, 1);
    csHigh();
    return McpError::Ok;
}

} // namespace psa
