// Single-MCP2515 CAN manager with runtime baud rate switching.
// One MCP2515 (spi0, GP2-6) handles both CAN-HS (500k) and CAN-LS (125k) —
// the physical wire is moved between buses and `gsniff rate hs/ls` matches the baud rate.
// Reference: docs/psa_can_reference.md section 6.
#pragma once
#include <cstdint>
#include "psa/mcp2515.hpp"
#include "psa/isotp.hpp"

namespace psa {

#ifdef HOST_TEST
inline spi_inst_t* const spi0 = nullptr;
#endif

enum class Bus : uint8_t { HighSpeed, LowSpeed };

class CanManager {
public:
    bool init(const Mcp2515::Pins& pins, bool listen_only);
    bool hasRx(Bus b) const;
    McpError read(Bus b, CanFrame& out);
    McpError send(Bus b, const CanFrame& f);
    // Single chip: ready if the MCP2515 answered at init(). The Bus parameter
    // is kept so all existing callers still compile; both values return the
    // same answer because there is only one controller.
    bool ready(Bus) const { return can_ready_; }

    // Raw controller access for hardware diagnostics. Returns nullptr if the
    // chip never answered, so a caller cannot wedge the SPI peripheral.
    Mcp2515* bus(Bus) {
        if (!can_ready_) return nullptr;
        return &can_;
    }

    // Latched controller error flags (0 if the chip is not ready).
    uint8_t errorFlags(Bus) {
        if (!can_ready_) return 0;
        return can_.errorFlags();
    }

    // Runtime baud rate change: enters config mode, writes CNF registers,
    // re-applies TX priority and RX filters, returns to listen-only mode.
#ifndef HOST_TEST
    bool reconfigureBus(Bus, CanBitrate rate) {
        if (!can_ready_) return false;
        return can_.setBaudRate(rate) == McpError::Ok;
    }
#else
    bool reconfigureBus(Bus, CanBitrate) { return false; }
#endif
private:
    Mcp2515 can_;
    bool    can_ready_ = false;
};

} // namespace psa
