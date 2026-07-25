// Flash engine — S-record parser and diagnostic flashing protocol state machine.
// Supports Motorola S-Record parsing and PSA-specific flashing sequences.
// Reference: Phase 4 of PSA CAN Interface Roadmap.
#pragma once
#include <cstdint>
#include <cstddef>
#include "psa/psa_protocol.hpp"

namespace psa {

struct SRecord {
    uint8_t type;         // 0-9
    uint32_t address;     // parsed memory address
    // Sized to what a TransferData request can actually carry (Req::buf minus
    // the service + sequence bytes). An S-record whose payload exceeds this is
    // rejected by the parser rather than truncated: silently flashing a short
    // block would leave a hole in the ECU's memory image.
    uint8_t data[kMaxFlashBlock];
    uint8_t data_len;     // payload length
    bool valid;           // true if checksum and format are valid
};

class SRecordParser {
public:
    // Parse a single S-record line. Returns true if parsed successfully.
    static bool parseLine(const char* line, SRecord& out);

    // True only for the record types that carry firmware payload (S1/S2/S3).
    static bool isDataRecord(uint8_t type);

private:
    static bool parseHexBytes(const char* src, size_t char_count, uint8_t* dest);
};

// CRC helper functions for PSA flashing validation
class FlashChecksum {
public:
    // Standard CRC-16/X-25 (used in PSA telecodage/flashing verification)
    static uint16_t crc16_x25(const uint8_t* data, size_t len, uint16_t preset = 0xFFFF);
};

// Flashing sequence controller
class FlashEngine {
public:
    enum class Step : uint8_t {
        Idle,
        RequestErase,
        EraseInProgress,
        RequestDownload,
        TransferData,
        RequestTransferExit,
        VerifyChecksum,
        Done,
        Error
    };

    void init(Protocol proto);
    Step step() const { return step_; }
    bool isError() const { return step_ == Step::Error; }
    // Payload bytes per TransferData the ECU said it can accept; 0 = not told.
    size_t maxBlock() const { return max_block_; }

    // Start address and total byte count of the staged image. RequestDownload
    // must announce the real extent — it used to be built from an empty dummy
    // record (address 0) with a hardcoded 64 KB size.
    void setDownloadExtent(uint32_t addr, uint32_t size) { dl_addr_ = addr; dl_size_ = size; }

    // Called by `flash end` once all data blocks are staged: leaves TransferData
    // so the next nextRequest() emits the RequestTransferExit (0x37) frame.
    void finishTransfer() { if (step_ == Step::TransferData) step_ = Step::RequestTransferExit; }

    // Generates the next request frame to be sent to the ECU.
    // data/len contains the current S-record block data being flashed.
    Req nextRequest(const SRecord& rec, uint8_t block_sequence_counter);

    // Feeds positive/negative responses back to the flash state machine
    void handleResponse(uint8_t service, const uint8_t* pdu, size_t len);

private:
    Protocol proto_ = Protocol::UDS;
    Step step_ = Step::Idle;
    size_t   max_block_ = 0;   // from the RequestDownload positive response
    uint32_t dl_addr_ = 0;     // staged image start address
    uint32_t dl_size_ = 0;     // staged image total byte count
};

} // namespace psa
