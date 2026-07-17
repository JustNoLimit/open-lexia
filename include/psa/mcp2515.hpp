// Minimal MCP2515 driver over Pico SDK SPI. 8 MHz crystal assumed.
// Reference: docs/psa_can_reference.md sections 5 & 6.
// Only the register set and bit-timing presets we need; no abstractions
// for features this project does not use (one-shot, clkout, masks beyond sniff).
#pragma once
#include <cstdint>
#ifndef HOST_TEST
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#else
struct spi_inst_t;
#endif

namespace psa {

struct CanFrame; // forward decl, defined in isotp.hpp

enum class CanBitrate : uint8_t { Bps125k, Bps500k };
enum class McpError : uint8_t { Ok, Fail, NoMsg, AllTxBusy };

class Mcp2515 {
public:
    struct Pins {
        spi_inst_t* spi;
        uint8_t sck, mosi, miso, cs, interrupt;
    };

    // MCP2515 register addresses (subset). datasheet DS21801.
    enum Reg : uint8_t {
        MCP_CNF3     = 0x28, MCP_CNF2 = 0x29, MCP_CNF1 = 0x2A,
        MCP_CANINTE  = 0x2B, MCP_CANINTF = 0x2C, MCP_EFLG = 0x2D,
        MCP_CANSTAT  = 0x0E, MCP_CANCTRL = 0x0F,
        MCP_RXB0CTRL = 0x60, MCP_RXB1CTRL = 0x70,
        MCP_TXB0CTRL = 0x30, MCP_TXB0SIDH = 0x31, MCP_TXB0DATA = 0x36,
        MCP_TXB1CTRL = 0x40, MCP_TXB1SIDH = 0x41, MCP_TXB1DATA = 0x46,
        MCP_TXB2CTRL = 0x50, MCP_TXB2SIDH = 0x51, MCP_TXB2DATA = 0x56,
        MCP_RXB0SIDH = 0x61, MCP_RXB0DATA = 0x66,
        MCP_RXB1SIDH = 0x71, MCP_RXB1DATA = 0x76,
        MCP_RXM0SIDH = 0x20, MCP_RXM1SIDH = 0x24,
    };
    enum Instr : uint8_t {
        INSTR_WRITE  = 0x02, INSTR_READ = 0x03, INSTR_BITMOD = 0x05,
        INSTR_RTS_TX0 = 0x81, INSTR_RTS_TX1 = 0x82, INSTR_RTS_TX2 = 0x84,
        INSTR_READ_STATUS = 0xA0, INSTR_RX_STATUS = 0xB0, INSTR_RESET = 0xC0,
    };

    Mcp2515() = default;
    explicit Mcp2515(const Pins& pins) : pins_(pins) {}
    void attach(const Pins& pins) { pins_ = pins; }

    McpError init(CanBitrate bitrate);
    McpError setNormalMode();
    McpError setListenOnlyMode();   // passive sniffer mode
    void     setSnifferFilters();   // accept all standard frames

    bool     hasRx();               // check INT pin / status
    McpError read(CanFrame& out);   // defined in isotp.hpp; reuse psa::CanFrame
    McpError send(const CanFrame& f);

private:
    Pins pins_;
    uint8_t spi_buf_[16];

    void    csLow();  void csHigh();
    void    writeReg(Reg r, uint8_t v);
    uint8_t readReg(Reg r);
    void    bitMod(Reg r, uint8_t mask, uint8_t bits);
    void    writeBytes(Reg r, const uint8_t* b, size_t n);
    void    readBytes(Reg r, uint8_t* b, size_t n);
    McpError setMode(uint8_t reqop);
};

} // namespace psa
