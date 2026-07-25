// ISO 15765-2 (ISO-TP) transport layer — pure, host-compilable.
// Segments diagnostic PDUs (>7 bytes) into CAN frames and reassembles them.
// Reference: docs/psa_can_reference.md section 4.1.
#pragma once
#include <cstdint>
#include <cstddef>

namespace psa {

// A single CAN frame. Hardware-agnostic: the driver layer fills/reads this.
// id is 32-bit because the sniffer sees whatever is on the bus: an extended
// frame carries a 29-bit identifier, which a uint16_t silently truncated while
// still reporting ext=true, so the sniffer printed a mangled ID.
struct CanFrame {
    uint32_t id = 0;
    uint8_t  dlc = 0;
    uint8_t  data[8] = {0};
    bool     rtr = false;
    bool     ext = false;
};

// Decode outcome.
//   NeedFlowControl — we received a First Frame; caller must send flowControl().
//   TxClearToSend   — the peer accepted our First Frame; consecutive frames may flow.
//   TxWait          — the peer asked us to hold (FC.WAIT); its N_Bs timer restarts.
//   TxAbort         — the peer refused the transfer (FC.OVERFLOW) or sent a bad FC.
enum class IsoTpStatus : uint8_t {
    Idle, NeedFlowControl, Continue, Done, Error,
    TxClearToSend, TxWait, TxAbort,
};

// ISO-TP state machine. Reassembles incoming first/consecutive frames and
// emits single/first/consecutive frames for outgoing PDUs.
class IsoTp {
public:
    static constexpr size_t kMaxPdu = 512;   // Big enough for PSA zones (max 12-bit = 4095, but 512 suffices)

    // Block size we advertise in our own flow-control frames. The RX path stages
    // frames through a 2-deep MCP2515 buffer, so we make the ECU pause for a new
    // FC every 8 consecutive frames instead of letting it stream unbounded.
    static constexpr uint8_t kRxBlockSize = 8;
    // Separation time we ask the peer for, in milliseconds.
    static constexpr uint8_t kRxStMinMs = 1;

    // Segment a PDU into up to N CAN frames at emit_id. Returns frame count.
    // Pure helper, kept for tests and for callers that want the whole burst;
    // the live send path uses beginSend()/nextTxFrame(), which honour the
    // peer's flow control instead of dumping every frame on the bus at once.
    static size_t encode(uint16_t emit_id, const uint8_t* pdu, size_t pdu_len,
                         CanFrame* out, size_t max_frames);

    // Feed an incoming CAN frame (from RECV_ID). Returns status.
    // On Done, the reassembled PDU is in pdu() with length pdu_len().
    IsoTpStatus feed(const CanFrame& f);

    // After NeedFlowControl, build the flow-control frame to send back.
    CanFrame flowControl(uint16_t emit_id, uint8_t st_ms = kRxStMinMs) const;

    const uint8_t* pdu()      const { return rx_; }
    size_t         pdu_len()  const { return rx_len_; }
    void           reset();

    // --- Transmit side (ISO 15765-2 sender) ---------------------------------
    // Stage a PDU and produce the frame to put on the bus now. For <=7 bytes
    // that is the single frame and the transfer is complete. For longer PDUs it
    // is the First Frame, after which the sender waits for the peer's flow
    // control before any consecutive frame is emitted. Returns false if the PDU
    // does not fit. now_us drives the N_Bs timeout and STmin pacing.
    bool beginSend(uint16_t emit_id, const uint8_t* pdu, size_t pdu_len,
                   uint64_t now_us, CanFrame& out);

    // True while consecutive frames are still owed to the peer.
    bool txActive() const { return tx_state_ != TxState::Idle; }

    // Produce the next consecutive frame if one is due (clear-to-send, block
    // size not exhausted, STmin elapsed). Returns false when nothing is due yet.
    bool nextTxFrame(uint64_t now_us, CanFrame& out);

    // True once the peer has gone silent past N_Bs while we wait on flow control.
    bool txTimedOut(uint64_t now_us) const;

    void txReset() { tx_state_ = TxState::Idle; }

private:
    uint8_t  rx_[kMaxPdu] = {0};
    size_t   rx_len_ = 0;       // total expected length (from first frame)
    size_t   rx_have_ = 0;      // bytes received so far
    uint8_t  next_seq_ = 0x21;  // next expected consecutive-frame sequence
    bool     in_multi_ = false;

    enum class TxState : uint8_t { Idle, WaitFlowControl, Sending };
    // N_Bs: how long we wait for the peer's flow control before giving up.
    static constexpr uint64_t kTxFlowControlTimeoutUs = 1'000'000;

    uint8_t  tx_[kMaxPdu] = {0};
    size_t   tx_len_ = 0;
    size_t   tx_pos_ = 0;          // payload bytes already on the bus
    uint16_t tx_id_ = 0;
    uint8_t  tx_seq_ = 0x21;
    TxState  tx_state_ = TxState::Idle;
    uint8_t  tx_block_left_ = 0;   // frames left in this block; 0 with bs_ == 0 means unlimited
    uint8_t  tx_bs_ = 0;           // block size the peer asked for (0 = unlimited)
    uint32_t tx_stmin_us_ = 0;     // separation time the peer asked for
    uint64_t tx_next_us_ = 0;      // earliest time the next frame may go out
    uint64_t tx_wait_start_us_ = 0;
    // feed() has no clock of its own; beginSend()/nextTxFrame() keep this fresh
    // (poll() calls nextTxFrame every iteration) so an arriving flow-control
    // frame can stamp its timers without threading a timestamp through feed().
    uint64_t tx_last_now_us_ = 0;
};

} // namespace psa
