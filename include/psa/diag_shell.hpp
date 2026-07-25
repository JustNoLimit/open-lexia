// Diagnostic shell — interactive USB serial command interface for ECU diagnostics.
// Provides a line-based CLI to connect to ECUs, read/clear DTCs, read zones,
// and send raw diagnostic PDUs. Auto-sends TesterPresent keep-alive.
// Reference: docs/psa_can_reference.md sections 3, 4.2, 4.3.
#pragma once
#include <cstdint>
#include <cstddef>
#include "psa/can_manager.hpp"
#include "psa/isotp.hpp"
#include "psa/psa_protocol.hpp"
#include "psa/flash_engine.hpp"
#include "psa/can_sniffer.hpp"

namespace psa {

class DiagShell {
public:
    // Shell state machine
    enum class State : uint8_t {
        Idle,             // No ECU connected, sniffer-only mode
        WaitingResponse,  // Sent a request, waiting for ISO-TP reassembly
        Connected,        // Session active, can accept commands
    };

    void init(CanManager* can);

    // Call from main loop: check for serial input, manage keep-alive timer.
    // Returns true if a command was processed.
    bool poll();

    // Feed a CAN frame from the active bus. If we are in a diagnostic session,
    // this routes the frame to the ISO-TP reassembler for the connected ECU.
    // Returns true if the frame was consumed (matched the ECU's recv_id).
    bool feedDiagFrame(const CanFrame& f);

    // Guided sniffer: true while a baseline/count/hold/sweep window is open.
    bool capturing() const { return sniffer_.active(); }
    // True whenever the guided sniffer wants frames at all (capturing or watching
    // a single ID) — main.cpp routes frames here instead of the passive decoder.
    bool gsniffActive() const { return sniffer_.active() || sniffer_.isWatching(); }
    void feedCaptureFrame(Bus b, const CanFrame& f) { sniffer_.feed(b, f); }

    // Feed a command line directly (useful for scripting and unit testing)
    void feedCommandLine(const char* line);

    // Accessors
    State state() const { return state_; }
    bool  sniffEnabled() const { return sniff_enabled_; }
    bool  isConnected() const { return state_ == State::Connected || state_ == State::WaitingResponse; }
    bool  isUnlocked() const { return unlocked_; }
    bool  isScanning() const { return scan_active_; }
    const char* ecuFamily() const { return ecu_ ? ecu_->family : "none"; }
    Bus   activeBus() const { return active_bus_; }
    Bus   sniffBus() const { return sniff_bus_; }

    // Register a log sink callback. When set, every printf-style log line from
    // the shell also gets forwarded to this sink (e.g. WifiServer::broadcastLog).
    using LogSink = void(*)(const char*);
    void setLogSink(LogSink sink) { log_sink_ = sink; }

private:
    // --- Command handlers ---
    void cmdList();
    void cmdConnect(const char* arg);
    void cmdDisconnect();
    void cmdDtc();
    void cmdClear();
    void cmdRead(const char* arg);
    void cmdWrite(const char* arg);
    void cmdTrace();
    void cmdUnlock();
    void cmdRaw(const char* arg);
    void cmdSniff(const char* arg);
    void cmdLive(const char* arg);
    void cmdActuator(const char* arg);
    void cmdFlash(const char* arg);
    void cmdScan();
    void cmdConfig(const char* arg);
    void cmdIdent();
    void cmdStatus();
    void cmdMeas(const char* arg);
    void cmdObd(const char* arg);
    void cmdService(const char* arg);
    void cmdProgram(const char* arg);
    void cmdEsp(const char* arg);
    void cmdPdi();
    void cmdPin(const char* arg);
    void cmdHwtest();
    void cmdGuidedSniff(const char* arg);
    void cmdHelp();

    // --- Internal helpers ---
    void sendReq(const Req& req);
    void sendPdu(const uint8_t* pdu, size_t len);
    // Put one frame on the active bus, retrying while the controller's three
    // transmit buffers are full. Returns false once it gives up.
    bool sendFrame(const CanFrame& f);
    static constexpr int kTxRetries = 32;
    void handleResponse(const uint8_t* pdu, size_t len);
    void printDtcKwp(const uint8_t* pdu, size_t len);
    void printDtcUds(const uint8_t* pdu, size_t len);
    void printZoneData(const uint8_t* pdu, size_t len);
    void printNegResponse(const uint8_t* pdu, size_t len);
    void printHex(const uint8_t* data, size_t len);
    void sendKeepAlive();
    bool parseHexByte(const char* s, uint8_t* out);
    // Parse 1..4 hex nibbles from [begin, end) into a uint16_t. end==nullptr means
    // "until end of string". Returns false on empty, overflow (>4 nibbles) or bad hex.
    bool parseHexU16(const char* begin, const char* end, uint16_t* out) const;
    // Effective SecurityAccess PIN: manual override if set, else the family default.
    uint16_t effectivePin() const;
    Bus  busForEcu(const EcuAddr* ecu) const;
    void connectByIndex(uint8_t index);
    void printScanResults();
    void advanceScan();
    bool isConfigZone(uint16_t zone_id);

    // --- Line buffer ---
    static constexpr size_t kLineBufSize = 128;
    char     line_buf_[kLineBufSize] = {0};
    size_t   line_pos_ = 0;
    bool     readLine();  // non-blocking; returns true when a full line is ready
    void     processLine();

    // --- Session state ---
    CanManager*     can_ = nullptr;
    IsoTp           tp_;
    State           state_ = State::Idle;
    const EcuAddr*  ecu_ = nullptr;     // currently connected ECU
    Bus             active_bus_ = Bus::HighSpeed;
    bool            sniff_enabled_ = true;
    Bus             sniff_bus_ = Bus::HighSpeed;
    CanSniffer      sniffer_;
    bool            unlocked_ = false;
    uint16_t        manual_pin_ = 0;        // user-supplied SecurityAccess PIN (`pin` cmd)
    bool            manual_pin_valid_ = false;
    uint8_t         pending_count_ = 0;     // consecutive 0x78 ResponsePending replies
    bool            live_polling_active_ = false;
    uint16_t        live_param_id_ = 0;
    uint64_t        last_poll_us_ = 0;
    // Standard OBD-II Mode 01 polling (single-frame, no session, HS bus).
    bool            obd_mode_ = false;
    uint8_t         obd_pid_ = 0;
    uint64_t        last_obd_us_ = 0;

    // --- Keep-alive timer ---
    uint64_t  last_keepalive_us_ = 0;
    static constexpr uint64_t kKeepAliveIntervalUs = 2'000'000; // 2 seconds

    // --- Response timeout ---
    uint64_t  response_start_us_ = 0;
    static constexpr uint64_t kResponseTimeoutUs = 5'000'000; // 5 seconds
    static constexpr uint8_t  kMaxResponsePending = 50;       // consecutive 0x78 cap

    // --- Scan state ---
    struct ScanEntry { const char* family; bool scanned; bool comm_ok; bool has_dtc; };
    ScanEntry scan_results_[24]; // kEcuCount (17+8 new)
    uint8_t   scan_index_;
    bool      scan_active_;
    bool      pdi_active_;

    // --- Config read state ---
    bool      config_readall_active_;
    uint16_t  config_zones_[32];
    uint8_t   config_zone_count_;
    uint8_t   config_zone_index_;

    // --- Flash engine ---
    FlashEngine       flash_engine_;
    SRecord           staged_records_[256];
    uint16_t          staged_count_ = 0;
    uint16_t          staged_index_ = 0;
    uint8_t           flash_seq_ = 1;
    bool              flash_active_ = false;
    bool              flash_session_pending_ = false;  // awaiting programming-session reply

    // --- Procedure state machines ---
    enum class Procedure : uint8_t {
        None,
        ServiceReset,
        ServiceSchedule,
        EspCalib,
        EspBleed,
        KeyLearn,
        KeyAdd,
        BsiInit,
    };
    Procedure proc_ = Procedure::None;
    uint8_t   proc_step_ = 0;
    uint8_t   proc_substep_ = 0;
    uint32_t  proc_data_ = 0;
    char      proc_name_[16];
    void advanceProcedure();
    void abortProcedure(const char* reason);
    void cmdServiceReset();
    void cmdServiceSchedule(const char* arg);
    void cmdEspCalib();
    void cmdEspBleed();
    void cmdKeyLearn();
    void cmdBsiInit();

    // --- Log sink (optional; set by WifiServer for wireless forwarding) ---
    LogSink  log_sink_ = nullptr;
    void     diagLog(const char* fmt, ...) const;
};

} // namespace psa
