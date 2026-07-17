// Dual-MCP2515 manager: CAN-HS (500k, spi0) + CAN-LS (125k, spi1).
// Zero bus contention: each MCP2515 on its own SPI peripheral.
// Reference: docs/psa_can_reference.md section 6.
#pragma once
#include <cstdint>
#include "psa/mcp2515.hpp"
#include "psa/isotp.hpp"

namespace psa {

#ifdef HOST_TEST
inline spi_inst_t* const spi0 = nullptr;
inline spi_inst_t* const spi1 = nullptr;
#endif

enum class Bus : uint8_t { HighSpeed, LowSpeed };

// Hardware pin map (Pico 2 W / RP2350) — see docs section 6.1.
struct DualCanPins {
    Mcp2515::Pins hs{spi0, 2, 3, 4, 5, 6};   // GP2 SCK, GP3 MOSI, GP4 MISO, GP5 CS, GP6 INT
    Mcp2515::Pins ls{spi1, 10, 11, 12, 13, 14}; // GP10..GP14
};

class CanManager {
public:
    bool init(const DualCanPins& pins, bool listen_only);
    bool hasRx(Bus b) const;
    McpError read(Bus b, CanFrame& out);
    McpError send(Bus b, const CanFrame& f);
    Mcp2515& hs() { return hs_; }
    Mcp2515& ls() { return ls_; }
private:
    Mcp2515 hs_;
    Mcp2515 ls_;
};

} // namespace psa
