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
    // Whether this bus's MCP2515 answered during init(). A bus with no chip
    // wired up must never be touched again — its SPI peripheral can wedge
    // (spi_write_blocking spins forever) once Wi-Fi/cyw43 comes up.
    bool ready(Bus b) const { return b == Bus::HighSpeed ? hs_ready_ : ls_ready_; }

    // Raw controller access for hardware diagnostics. Returns nullptr for a bus
    // whose chip never answered, so a caller physically cannot reach around the
    // ready() guard and wedge the SPI peripheral — hwtest used to do exactly
    // that, hanging the main loop on the very hardware it exists to diagnose.
    Mcp2515* bus(Bus b) {
        if (!ready(b)) return nullptr;
        return b == Bus::HighSpeed ? &hs_ : &ls_;
    }

    // Latched controller error flags for a bus (0 if the bus is not ready).
    uint8_t errorFlags(Bus b) {
        Mcp2515* m = bus(b);
        return m ? m->errorFlags() : 0;
    }
private:
    Mcp2515 hs_;
    Mcp2515 ls_;
    bool hs_ready_ = false;
    bool ls_ready_ = false;
};

} // namespace psa
