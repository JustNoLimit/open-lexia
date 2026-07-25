// Flash engine implementation.
#include "psa/flash_engine.hpp"
#include <cstdio>
#include <cstring>

namespace psa {

// --- S-Record Parser ---------------------------------------------------------

bool SRecordParser::parseHexBytes(const char* src, size_t char_count, uint8_t* dest) {
    for (size_t i = 0; i < char_count; i += 2) {
        uint8_t hi = 0, lo = 0;
        char c = src[i];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'A' && c <= 'F') hi = 10 + (c - 'A');
        else if (c >= 'a' && c <= 'f') hi = 10 + (c - 'a');
        else return false;

        c = src[i+1];
        if (c >= '0' && c <= '9') lo = c - '0';
        else if (c >= 'A' && c <= 'F') lo = 10 + (c - 'A');
        else if (c >= 'a' && c <= 'f') lo = 10 + (c - 'a');
        else return false;

        dest[i/2] = (hi << 4) | lo;
    }
    return true;
}

bool SRecordParser::parseLine(const char* line, SRecord& out) {
    out.valid = false;
    if (line == nullptr || line[0] != 'S') return false;

    char type_char = line[1];
    if (type_char < '0' || type_char > '9') return false;
    out.type = type_char - '0';

    // Parse length byte
    uint8_t byte_count = 0;
    if (!parseHexBytes(line + 2, 2, &byte_count)) return false;

    // Remaining string must contain at least byte_count * 2 hex chars
    size_t line_len = std::strlen(line);
    // Strip trailing newlines/carriage returns
    while (line_len > 0 && (line[line_len-1] == '\r' || line[line_len-1] == '\n')) {
        line_len--;
    }
    if (line_len < 4 + static_cast<size_t>(byte_count) * 2) return false;

    // Allocate temp buffer to parse all remaining bytes (including checksum)
    uint8_t buf[260];
    if (byte_count < 3) return false;
    if (!parseHexBytes(line + 4, byte_count * 2, buf)) return false;

    // Validate checksum
    uint8_t sum = byte_count;
    for (int i = 0; i < byte_count - 1; ++i) {
        sum += buf[i];
    }
    uint8_t computed_chk = ~sum;
    uint8_t file_chk = buf[byte_count - 1];
    if (computed_chk != file_chk) {
        return false;
    }

    // Determine address length based on S-record type
    size_t addr_len = 0;
    switch (out.type) {
        case 0:
        case 1:
        case 5:
        case 9:
            addr_len = 2; // 16-bit address
            break;
        case 2:
        case 8:
            addr_len = 3; // 24-bit address
            break;
        case 3:
        case 7:
            addr_len = 4; // 32-bit address
            break;
        default:
            addr_len = 0; // S4, S6 are reserved/unused
            break;
    }

    if (addr_len == 0 || byte_count < addr_len + 1) return false;

    // Parse address
    out.address = 0;
    for (size_t i = 0; i < addr_len; ++i) {
        out.address = (out.address << 8) | buf[i];
    }

    // Parse data. A record that carries more than one TransferData request can
    // hold is refused outright — truncating it would flash a partial block and
    // leave a gap in the ECU image, which is worse than not flashing at all.
    size_t data_len = byte_count - addr_len - 1;
    if (data_len > sizeof(out.data)) return false;
    out.data_len = static_cast<uint8_t>(data_len);
    std::memcpy(out.data, buf + addr_len, out.data_len);
    out.valid = true;

    return true;
}

bool SRecordParser::isDataRecord(uint8_t type) {
    // Only S1/S2/S3 carry firmware bytes. S0 is a header, S5/S6 are record
    // counts, S7/S8/S9 are start addresses — flashing any of them would push
    // metadata into the ECU as if it were code.
    return type == 1 || type == 2 || type == 3;
}

// --- CRC Checksum ------------------------------------------------------------

uint16_t FlashChecksum::crc16_x25(const uint8_t* data, size_t len, uint16_t preset) {
    uint16_t crc = preset;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x8408; // X-25 polynomial reversed (standard CRC-16-CCITT)
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc; // X-25 standard takes one's complement
}

// --- Flash Engine State Machine ----------------------------------------------

void FlashEngine::init(Protocol proto) {
    proto_ = proto;
    step_ = Step::Idle;
    max_block_ = 0;
    dl_addr_ = 0;
    dl_size_ = 0;
}

Req FlashEngine::nextRequest(const SRecord& rec, uint8_t block_sequence_counter) {
    Req r{{}, 0};
    switch (step_) {
        case Step::Idle:
            // Connect should have already happened. We start by requesting erase.
            step_ = Step::RequestErase;
            if (proto_ == Protocol::UDS) {
                // RoutineControl StartRoutine EraseMemory (0x31 01 FF 00) + address + size
                r.buf[0] = 0x31;
                r.buf[1] = 0x01;
                r.buf[2] = 0xFF;
                r.buf[3] = 0x00;
                r.buf[4] = 0x81; // memory block ID or routine options
                r.buf[5] = 0xF0;
                r.buf[6] = 0x5A;
                r.len = 7;
            } else {
                // KWP EraseMemory: 31 81 81 F0 5A
                r.buf[0] = 0x31;
                r.buf[1] = 0x81;
                r.buf[2] = 0x81;
                r.buf[3] = 0xF0;
                r.buf[4] = 0x5A;
                r.len = 5;
            }
            break;

        case Step::RequestDownload: {
            step_ = Step::TransferData;
            // The caller stages the whole image before we get here, so announce
            // its real extent. Fall back to the current record's address only if
            // nobody called setDownloadExtent().
            uint32_t addr = dl_size_ ? dl_addr_ : rec.address;
            uint32_t size = dl_size_ ? dl_size_ : rec.data_len;
            if (proto_ == Protocol::UDS) {
                // UDS: 34 (compression/encryption = 00) (addrFormat/sizeFormat = 44) (addr 4B) (size 4B)
                r.buf[0] = 0x34;
                r.buf[1] = 0x00;
                r.buf[2] = 0x44; // 4-byte address, 4-byte size
                r.buf[3] = static_cast<uint8_t>(addr >> 24);
                r.buf[4] = static_cast<uint8_t>(addr >> 16);
                r.buf[5] = static_cast<uint8_t>(addr >> 8);
                r.buf[6] = static_cast<uint8_t>(addr & 0xFF);
                r.buf[7] = static_cast<uint8_t>(size >> 24);
                r.buf[8] = static_cast<uint8_t>(size >> 16);
                r.buf[9] = static_cast<uint8_t>(size >> 8);
                r.buf[10] = static_cast<uint8_t>(size & 0xFF);
                r.len = 11;
            } else {
                // KWP RequestDownload: 34 (addr 3B) (compression) (size 2B)
                r.buf[0] = 0x34;
                r.buf[1] = static_cast<uint8_t>(addr >> 16);
                r.buf[2] = static_cast<uint8_t>(addr >> 8);
                r.buf[3] = static_cast<uint8_t>(addr & 0xFF);
                r.buf[4] = 0x00; // compression
                r.buf[5] = static_cast<uint8_t>(size >> 8);
                r.buf[6] = static_cast<uint8_t>(size & 0xFF);
                r.len = 7;
            }
            break;
        }

        case Step::TransferData: {
            // Send the S-record payload. The parser already refuses anything
            // larger than kMaxFlashBlock, and the ECU's own maxNumberOfBlockLength
            // (learned from the RequestDownload reply) caps it further; clamp
            // here too so no future caller can walk off the end of buf.
            size_t cap = sizeof(r.buf) - 2;
            if (max_block_ != 0 && max_block_ < cap) cap = max_block_;
            size_t n = (rec.data_len < cap) ? rec.data_len : cap;
            r.buf[0] = 0x36;
            r.buf[1] = block_sequence_counter;
            std::memcpy(r.buf + 2, rec.data, n);
            r.len = static_cast<uint8_t>(2 + n);
            break;
        }

        case Step::RequestTransferExit:
            step_ = Step::VerifyChecksum;
            r.buf[0] = 0x37;
            r.len = 1;
            break;

        case Step::VerifyChecksum:
            step_ = Step::Done;
            // Verify checksum: RoutineControl StartRoutine Checksum (31 01 02 02 ...)
            r.buf[0] = 0x31;
            r.buf[1] = 0x01;
            r.buf[2] = 0x02;
            r.buf[3] = 0x02;
            r.len = 4;
            break;

        default:
            break;
    }
    return r;
}

void FlashEngine::handleResponse(uint8_t service, const uint8_t* pdu, size_t len) {
    (void)pdu;
    if (len == 0) return;

    if (service == 0x7F) {
        step_ = Step::Error;
        return;
    }

    switch (step_) {
        case Step::RequestErase:
            if (service == 0x71 || service == 0x70) {
                step_ = Step::RequestDownload;
            }
            break;
        case Step::RequestDownload:
            // Positive response to 0x34: 0x74/0x76 carrying
            // lengthFormatIdentifier + maxNumberOfBlockLength. That length
            // includes the 0x36 service and sequence bytes, so subtract them.
            if (service != 0x7F) {
                if (len >= 2) {
                    size_t nbytes = pdu[1] >> 4;          // high nibble = byte count
                    if (nbytes > 0 && nbytes <= 4 && len >= 2 + nbytes) {
                        size_t maxlen = 0;
                        for (size_t i = 0; i < nbytes; ++i) maxlen = (maxlen << 8) | pdu[2 + i];
                        max_block_ = (maxlen > 2) ? (maxlen - 2) : 0;
                    }
                }
                step_ = Step::TransferData;
            }
            break;
        case Step::TransferData:
            if (service == 0x76) {
                // Stay in TransferData for subsequent blocks
            }
            break;
        case Step::RequestTransferExit:
            // Positive response to 0x37
            if (service != 0x7F) {
                step_ = Step::VerifyChecksum;
            }
            break;
        case Step::VerifyChecksum:
            if (service == 0x71 || service == 0x70) {
                step_ = Step::Done;
            }
            break;
        default:
            break;
    }
}

} // namespace psa
