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
    if (pci >= 0x30 && pci <= 0x3F) {                 // flow control (peer-initiated)
        return IsoTpStatus::Continue;                 // we are the receiver; ignore FC we sent
    }
    return IsoTpStatus::Error;
}

CanFrame IsoTp::flowControl(uint16_t emit_id, uint8_t st_ms) const {
    CanFrame fc{};
    fc.id = emit_id; fc.dlc = 3;
    fc.data[0] = 0x30;   // Clear To Send
    fc.data[1] = 0x00;   // block size = 0 -> send all remaining
    fc.data[2] = st_ms;  // min separation between consecutive frames (ms)
    return fc;
}

void IsoTp::reset() {
    rx_len_ = rx_have_ = 0;
    next_seq_ = 0x21;
    in_multi_ = false;
}

} // namespace psa
