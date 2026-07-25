// Single-MCP2515 CAN manager — implementation.
#include "psa/can_manager.hpp"

namespace psa {

bool CanManager::init(const Mcp2515::Pins& pins, bool listen_only) {
    can_.attach(pins);

    can_ready_ = can_.init(CanBitrate::Bps500k) == McpError::Ok;
    if (can_ready_) {
        can_ready_ = (listen_only ? can_.setListenOnlyMode() : can_.setNormalMode()) == McpError::Ok;
    }
    return can_ready_;
}

bool CanManager::hasRx(Bus) const {
    if (!can_ready_) return false;
    return const_cast<Mcp2515&>(can_).hasRx();
}

McpError CanManager::read(Bus, CanFrame& out) {
    if (!can_ready_) return McpError::Fail;
    return can_.read(out);
}

McpError CanManager::send(Bus, const CanFrame& f) {
    if (!can_ready_) return McpError::Fail;
    return can_.send(f);
}

} // namespace psa
