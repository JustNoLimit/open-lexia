// Dual-MCP2515 manager — implementation.
#include "psa/can_manager.hpp"

namespace psa {

bool CanManager::init(const DualCanPins& pins, bool listen_only) {
    hs_.attach(pins.hs);
    ls_.attach(pins.ls);
    if (hs_.init(CanBitrate::Bps500k) != McpError::Ok) return false;
    if (ls_.init(CanBitrate::Bps125k) != McpError::Ok) return false;
    if (listen_only) {
        if (hs_.setListenOnlyMode() != McpError::Ok) return false;
        if (ls_.setListenOnlyMode() != McpError::Ok) return false;
    } else {
        if (hs_.setNormalMode() != McpError::Ok) return false;
        if (ls_.setNormalMode() != McpError::Ok) return false;
    }
    return true;
}

bool CanManager::hasRx(Bus b) const {
    return const_cast<Mcp2515&>(b == Bus::HighSpeed ? hs_ : ls_).hasRx();
}

McpError CanManager::read(Bus b, CanFrame& out) {
    return (b == Bus::HighSpeed ? hs_ : ls_).read(out);
}

McpError CanManager::send(Bus b, const CanFrame& f) {
    return (b == Bus::HighSpeed ? hs_ : ls_).send(f);
}

} // namespace psa
