// ISO 15765-2 (ISO-TP) transport — implementation (host-compilable).
#include "psa/isotp.hpp"
#include <cstring>

namespace psa {

size_t IsoTp::encode(uint16_t emit_id, const uint8_t* pdu, size_t pdu_len,
                     CanFrame* out, size_t max_frames) {
    if (pdu_len == 0 || out == nullptr) return 0;

    if (pdu_len <= 7) {
        if (max_frames < 1) return 0;
        CanFrame& f = out[0];
        f.id = emit_id; f.dlc = static_cast<uint8_t>(pdu_len + 1);
        f.data[0] = static_cast<uint8_t>(pdu_len);        // single-frame PCI
        std::memcpy(&f.data[1], pdu, pdu_len);
        std::memset(&f.data[1 + pdu_len], 0, 7 - pdu_len);
        return 1;
    }

    // First frame
    if (max_frames < 2) return 0;
    if (pdu_len > kMaxPdu || pdu_len > 0xFFF) return 0;  // exceeds buffer or 12-bit FF length
    CanFrame& ff = out[0];
    ff.id = emit_id; ff.dlc = 8;
    ff.data[0] = static_cast<uint8_t>(0x10 | (pdu_len >> 8));
    ff.data[1] = static_cast<uint8_t>(pdu_len & 0xFF);
    std::memcpy(&ff.data[2], pdu, 6);

    size_t pos = 6;
    uint8_t seq = 0x21;
    size_t n = 1;
    while (pos < pdu_len) {
        if (n >= max_frames) return 0;
        CanFrame& cf = out[n];
        cf.id = emit_id; cf.dlc = 8;
        cf.data[0] = seq;
        size_t chunk = (pdu_len - pos < 7) ? (pdu_len - pos) : 7;
        std::memcpy(&cf.data[1], pdu + pos, chunk);
        std::memset(&cf.data[1 + chunk], 0, 7 - chunk);
        pos += chunk;
        ++n;
        seq = (seq == 0x2F) ? 0x20 : static_cast<uint8_t>(seq + 1);
    }
    return n;
}

IsoTpStatus IsoTp::feed(const CanFrame& f) {
    if (f.dlc == 0) return IsoTpStatus::Error;
    uint8_t pci = f.data[0];

    if (pci < 0x08) {                                 // single frame
        // A SF must physically carry the bytes it claims (pci <= dlc-1). A frame
        // claiming more than it holds is malformed — reject rather than return a
        // Done buffer padded with uninitialised bytes the caller would read.
        if (pci > f.dlc - 1 || pci > kMaxPdu) { reset(); return IsoTpStatus::Error; }
        rx_len_ = rx_have_ = pci;
        std::memcpy(rx_, &f.data[1], rx_have_);
        in_multi_ = false;
        return IsoTpStatus::Done;
    }
    if (pci >= 0x10 && pci <= 0x1F) {                 // first frame
        if (f.dlc < 2) { reset(); return IsoTpStatus::Error; }
        rx_len_ = ((pci & 0x0F) << 8) | f.data[1];
        // A valid FF carries >7 bytes; anything <=7 is malformed and would later
        // underflow the consecutive-frame chunk clamp (rx_len_ - rx_have_).
        if (rx_len_ <= 7 || rx_len_ > kMaxPdu) { reset(); return IsoTpStatus::Error; }
        size_t chunk = (f.dlc >= 8) ? 6 : (f.dlc - 2);
        std::memcpy(rx_, &f.data[2], chunk);
        rx_have_ = chunk;
        next_seq_ = 0x21;
        in_multi_ = true;
        return IsoTpStatus::NeedFlowControl;
    }
    if (pci >= 0x20 && pci <= 0x2F) {                 // consecutive frame
        if (!in_multi_) return IsoTpStatus::Error;
        // Validate sequence number
        if (pci != next_seq_) {
            // Out-of-order frame; reset to recover
            reset();
            return IsoTpStatus::Error;
        }
        next_seq_ = (next_seq_ == 0x2F) ? 0x20 : static_cast<uint8_t>(next_seq_ + 1);
        size_t chunk = (f.dlc >= 8) ? 7 : (f.dlc - 1);
        if (rx_have_ + chunk > rx_len_) chunk = rx_len_ - rx_have_;
        std::memcpy(rx_ + rx_have_, &f.data[1], chunk);
        rx_have_ += chunk;
        if (rx_have_ >= rx_len_) { in_multi_ = false; return IsoTpStatus::Done; }
        return IsoTpStatus::Continue;
    }
    if (pci >= 0x30 && pci <= 0x3F) {                 // flow control from the peer
        // This is the ECU answering a First Frame *we* sent. Ignoring it (as
        // this used to) meant blasting every consecutive frame at once, which a
        // real ECU drops — and an explicit refusal (OVERFLOW) was ignored too.
        if (tx_state_ == TxState::Idle) return IsoTpStatus::Continue;
        uint8_t fs = pci & 0x0F;
        if (fs == 0x01) {                             // WAIT — hold, restart N_Bs
            tx_state_ = TxState::WaitFlowControl;
            tx_wait_start_us_ = tx_last_now_us_;
            return IsoTpStatus::TxWait;
        }
        if (fs != 0x00) {                             // OVERFLOW (0x02) or reserved
            tx_state_ = TxState::Idle;
            return IsoTpStatus::TxAbort;
        }
        if (f.dlc < 3) { tx_state_ = TxState::Idle; return IsoTpStatus::TxAbort; }
        tx_bs_ = f.data[1];
        tx_block_left_ = tx_bs_;
        // STmin: 0x00-0x7F are milliseconds, 0xF1-0xF9 are 100..900 us,
        // everything else is reserved and ISO 15765-2 says use 127 ms.
        uint8_t st = f.data[2];
        if (st <= 0x7F)                     tx_stmin_us_ = static_cast<uint32_t>(st) * 1000u;
        else if (st >= 0xF1 && st <= 0xF9)  tx_stmin_us_ = (st - 0xF0) * 100u;
        else                                tx_stmin_us_ = 127000u;
        tx_state_ = TxState::Sending;
        tx_next_us_ = tx_last_now_us_;      // first CF may go out immediately
        return IsoTpStatus::TxClearToSend;
    }
    return IsoTpStatus::Error;
}

// --- Transmit side -----------------------------------------------------------

bool IsoTp::beginSend(uint16_t emit_id, const uint8_t* pdu, size_t pdu_len,
                      uint64_t now_us, CanFrame& out) {
    if (pdu == nullptr || pdu_len == 0 || pdu_len > kMaxPdu || pdu_len > 0xFFF) return false;
    tx_last_now_us_ = now_us;
    out = CanFrame{};
    out.id = emit_id;

    if (pdu_len <= 7) {                               // single frame, nothing owed
        out.dlc = static_cast<uint8_t>(pdu_len + 1);
        out.data[0] = static_cast<uint8_t>(pdu_len);
        std::memcpy(&out.data[1], pdu, pdu_len);
        tx_state_ = TxState::Idle;
        return true;
    }

    std::memcpy(tx_, pdu, pdu_len);
    tx_len_ = pdu_len;
    tx_id_ = emit_id;
    tx_seq_ = 0x21;
    tx_pos_ = 6;
    tx_bs_ = 0;
    tx_block_left_ = 0;
    tx_stmin_us_ = 0;
    tx_state_ = TxState::WaitFlowControl;
    tx_wait_start_us_ = now_us;

    out.dlc = 8;
    out.data[0] = static_cast<uint8_t>(0x10 | (pdu_len >> 8));
    out.data[1] = static_cast<uint8_t>(pdu_len & 0xFF);
    std::memcpy(&out.data[2], tx_, 6);
    return true;
}

bool IsoTp::nextTxFrame(uint64_t now_us, CanFrame& out) {
    tx_last_now_us_ = now_us;
    if (tx_state_ != TxState::Sending) return false;
    if (tx_pos_ >= tx_len_) { tx_state_ = TxState::Idle; return false; }
    if (now_us < tx_next_us_) return false;
    // A finite block size means the peer wants a fresh flow control once the
    // block is exhausted; go back to waiting rather than running it over.
    if (tx_bs_ != 0 && tx_block_left_ == 0) {
        tx_state_ = TxState::WaitFlowControl;
        tx_wait_start_us_ = now_us;
        return false;
    }

    out = CanFrame{};
    out.id = tx_id_;
    out.dlc = 8;
    out.data[0] = tx_seq_;
    size_t chunk = (tx_len_ - tx_pos_ < 7) ? (tx_len_ - tx_pos_) : 7;
    std::memcpy(&out.data[1], tx_ + tx_pos_, chunk);
    tx_pos_ += chunk;
    tx_seq_ = (tx_seq_ == 0x2F) ? 0x20 : static_cast<uint8_t>(tx_seq_ + 1);
    if (tx_bs_ != 0 && tx_block_left_ > 0) tx_block_left_--;
    tx_next_us_ = now_us + tx_stmin_us_;
    if (tx_pos_ >= tx_len_) tx_state_ = TxState::Idle;
    return true;
}

bool IsoTp::txTimedOut(uint64_t now_us) const {
    return tx_state_ == TxState::WaitFlowControl &&
           now_us - tx_wait_start_us_ > kTxFlowControlTimeoutUs;
}

CanFrame IsoTp::flowControl(uint16_t emit_id, uint8_t st_ms) const {
    CanFrame fc{};
    fc.id = emit_id; fc.dlc = 3;
    fc.data[0] = 0x30;          // Clear To Send
    // Block size and separation time must reflect what we can actually absorb:
    // the controller holds two frames, and one slow main-loop pass (lwIP, a
    // printf burst) is enough to miss one. BS=0/STmin=0 told the ECU to stream
    // flat out, which lost consecutive frames and failed the whole reassembly.
    fc.data[1] = kRxBlockSize;
    fc.data[2] = st_ms;         // min separation between consecutive frames (ms)
    return fc;
}

void IsoTp::reset() {
    rx_len_ = rx_have_ = 0;
    next_seq_ = 0x21;
    in_multi_ = false;
}

} // namespace psa
