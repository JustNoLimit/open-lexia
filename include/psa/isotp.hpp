// ISO 15765-2 (ISO-TP) transport layer — pure, host-compilable.
// Segments diagnostic PDUs (>7 bytes) into CAN frames and reassembles them.
// Reference: docs/psa_can_reference.md section 4.1.
#pragma once
#include <cstdint>
#include <cstddef>

namespace psa {

// A single CAN frame. Hardware-agnostic: the driver layer fills/reads this.
struct CanFrame {
    uint16_t id = 0;
    uint8_t  dlc = 0;
    uint8_t  data[8] = {0};
    bool     rtr = false;
    bool     ext = false;
};

// Decode outcome.
enum class IsoTpStatus : uint8_t { Idle, NeedFlowControl, Continue, Done, Error };

// ISO-TP state machine. Reassembles incoming first/consecutive frames and
// emits single/first/consecutive frames for outgoing PDUs.
class IsoTp {
public:
    static constexpr size_t kMaxPdu = 512;   // Big enough for PSA zones (max 12-bit = 4095, but 512 suffices)

    // Segment a PDU into up to N CAN frames at emit_id. Returns frame count.
    // flow_control_st_ms is the delay the peer should send consecutive frames at;
    // we are the sender, so we just stamp it into the FC frame we *expect*.
    static size_t encode(uint16_t emit_id, const uint8_t* pdu, size_t pdu_len,
                         CanFrame* out, size_t max_frames);

    // Feed an incoming CAN frame (from RECV_ID). Returns status.
    // On Done, the reassembled PDU is in pdu() with length pdu_len().
    IsoTpStatus feed(const CanFrame& f);

    // After NeedFlowControl, build the flow-control frame to send back.
    CanFrame flowControl(uint16_t emit_id, uint8_t st_ms = 0) const;

    const uint8_t* pdu()      const { return rx_; }
    size_t         pdu_len()  const { return rx_len_; }
    void           reset();

private:
    uint8_t  rx_[kMaxPdu] = {0};
    size_t   rx_len_ = 0;       // total expected length (from first frame)
    size_t   rx_have_ = 0;      // bytes received so far
    uint8_t  next_seq_ = 0x21;  // next expected consecutive-frame sequence
    bool     in_multi_ = false;
};

} // namespace psa
