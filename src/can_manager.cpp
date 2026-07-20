// Dual-MCP2515 manager — implementation.
#include "psa/can_manager.hpp"

namespace psa {

bool CanManager::init(const DualCanPins& pins, bool listen_only) {
    hs_.attach(pins.hs);
    ls_.attach(pins.ls);

    // Each bus is brought up independently — a chip missing on one bus must
    // not stop the other, and must never be touched again after this (see
    // ready()/hasRx()/read() below): its SPI peripheral can wedge once
    // Wi-Fi/cyw43 comes up, hanging the whole main loop forever.
    hs_ready_ = hs_.init(CanBitrate::Bps500k) == McpError::Ok;
    if (hs_ready_) {
        hs_ready_ = (listen_only ? hs_.setListenOnlyMode() : hs_.setNormalMode()) == McpError::Ok;
    }
    ls_ready_ = ls_.init(CanBitrate::Bps125k) == McpError::Ok;
    if (ls_ready_) {
        ls_ready_ = (listen_only ? ls_.setListenOnlyMode() : ls_.setNormalMode()) == McpError::Ok;
    }
    return hs_ready_ && ls_ready_;
}

bool CanManager::hasRx(Bus b) const {
    if (!ready(b)) return false;
    return const_cast<Mcp2515&>(b == Bus::HighSpeed ? hs_ : ls_).hasRx();
}

McpError CanManager::read(Bus b, CanFrame& out) {
    if (!ready(b)) return McpError::Fail;
    return (b == Bus::HighSpeed ? hs_ : ls_).read(out);
}

McpError CanManager::send(Bus b, const CanFrame& f) {
    if (!ready(b)) return McpError::Fail;
    return (b == Bus::HighSpeed ? hs_ : ls_).send(f);
}

} // namespace psa
