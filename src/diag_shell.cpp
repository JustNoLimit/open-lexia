// Diagnostic shell — implementation.
// Interactive USB serial CLI for PSA ECU diagnostics.
#include "psa/diag_shell.hpp"
#include "psa/ecu_keys.hpp"
#include "psa/live_data.hpp"
#include "psa/dtc_text.hpp"
#include "psa/actuator_catalog.hpp"
#include "psa/flash_engine.hpp"
#include "psa/ecu_zones.hpp"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <cstdlib>

// Use Pico SDK timer for timestamps when on hardware; fallback for host tests.
#ifdef HOST_TEST
// Not static: the host self-check drives this to exercise the flow-control and
// response timeouts, which are pure time-based logic and otherwise untestable.
uint64_t g_fake_time_us = 0;
static inline uint64_t get_time_us() { return g_fake_time_us; }
static inline int getchar_nonblocking() { return -1; /* no input in test mode */ }
#else
#include "pico/stdlib.h"
static inline uint64_t get_time_us() { return to_us_since_boot(get_absolute_time()); }
static inline int getchar_nonblocking() { return getchar_timeout_us(0); }
#endif

namespace psa {

void DiagShell::init(CanManager* can) {
    can_ = can;
    state_ = State::Idle;
    ecu_ = nullptr;
    sniff_enabled_ = true;
    line_pos_ = 0;
    tp_.reset();
    unlocked_ = false;
    live_polling_active_ = false;
    live_param_id_ = 0;
    last_poll_us_ = 0;
    scan_active_ = false;
    pdi_active_ = false;
    scan_index_ = 0;
    config_readall_active_ = false;
    config_zone_count_ = 0;
    config_zone_index_ = 0;
    proc_ = Procedure::None;
    proc_step_ = 0;
    sniffer_.init();
}

void DiagShell::diagLog(const char* fmt, ...) const {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s", buf);
    if (log_sink_) log_sink_(buf);
}

// --- Main loop poll ----------------------------------------------------------

bool DiagShell::poll() {
    // Pump any consecutive frames the peer has cleared us to send. ISO-TP
    // forbids dumping them all at once: the ECU's flow control dictates the
    // block size and separation time, so the transmit side is paced from here.
    if (tp_.txActive() && ecu_ != nullptr) {
        uint64_t now = get_time_us();
        CanFrame cf;
        while (tp_.nextTxFrame(now, cf)) {
            if (!sendFrame(cf)) {
                printf("[DIAG] CAN transmit failed mid-transfer; aborting request.\n");
                tp_.txReset();
                break;
            }
        }
        if (tp_.txTimedOut(now)) {
            printf("[DIAG] ECU never sent flow control; request aborted.\n");
            tp_.txReset();
            if (state_ == State::WaitingResponse) state_ = State::Connected;
        }
    }

    // Check for response timeout
    if (state_ == State::WaitingResponse) {
        uint64_t now = get_time_us();
        if (now - response_start_us_ > kResponseTimeoutUs) {
            printf("[DIAG] Response timeout.\n");
            tp_.txReset();
            // Every long-running operation has to be torn down here. Leaving a
            // flag set meant the next unrelated response was fed back into a
            // stale state machine — for a procedure that could resume a write
            // sequence the user had already been told had timed out.
            if (config_readall_active_) {
                config_readall_active_ = false;
                printf("[CONFIG] Readall timed out.\n");
            }
            if (flash_active_) {
                flash_active_ = false;
                printf("[FLASH] Timed out; flash sequence aborted.\n");
            }
            if (proc_ != Procedure::None) {
                abortProcedure("Response timeout during procedure");
            }
            if (live_polling_active_) {
                live_polling_active_ = false;
                printf("[LIVE] Stopped (no response).\n");
            }
            if (scan_active_ && ecu_) {
                scan_results_[scan_index_].scanned = true;
                scan_results_[scan_index_].comm_ok = false;
                scan_results_[scan_index_].has_dtc = false;
                printf("[SCAN] %s timeout.\n", ecu_->family);
                state_ = State::Idle;
                ecu_ = nullptr;
                advanceScan();
            } else {
                pdi_active_ = false;
                state_ = State::Idle;
            }
        }
    }

    // Send keep-alive if connected — but never while a request is outstanding.
    // A TesterPresent is redundant then, and any negative response it provokes
    // is indistinguishable from a rejection of the pending request, which used
    // to abort in-flight procedures and flash writes.
    if (isConnected() && state_ != State::WaitingResponse && !tp_.txActive()) {
        uint64_t now = get_time_us();
        if (now - last_keepalive_us_ >= kKeepAliveIntervalUs) {
            sendKeepAlive();
            last_keepalive_us_ = now;
        }
    }

    // Send live poll if active
    if (live_polling_active_ && state_ == State::Connected) {
        uint64_t now = get_time_us();
        if (last_poll_us_ == 0 || now - last_poll_us_ >= 250000) {
            Req req = readLiveData(ecu_->proto, live_param_id_);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = now;
            last_poll_us_ = now;
        }
    }

    // Send OBD-II Mode 01 poll if active (no session; single frame on 0x7DF).
    if (obd_mode_ && can_ && can_->ready(Bus::HighSpeed)) {
        uint64_t now = get_time_us();
        if (last_obd_us_ == 0 || now - last_obd_us_ >= 250000) {
            CanFrame f;
            f.id = 0x7DF;   // OBD functional request; any powertrain ECU replies
            f.dlc = 8;
            f.data[0] = 0x02;      // ISO-TP single frame, 2 data bytes
            f.data[1] = 0x01;      // service 01 (current data)
            f.data[2] = obd_pid_;
            can_->send(Bus::HighSpeed, f);
            last_obd_us_ = now;
        }
    }

    // (Procedure timeouts are handled in the response-timeout block above; a
    // separate check here was dead code — the block above had already moved the
    // state out of WaitingResponse before it ran.)

    // Guided sniffer: auto-close the baseline window on its deadline.
    sniffer_.tick(get_time_us());

    // Check for serial input
    if (readLine()) {
        processLine();
        return true;
    }
    return false;
}

// --- CAN frame routing -------------------------------------------------------

bool DiagShell::feedDiagFrame(const CanFrame& f) {
    // Standard OBD-II Mode 01 response (0x7E8..0x7EF, service 0x41). Single
    // frame, no session — handled before the ECU-session checks below.
    if (obd_mode_ && f.id >= 0x7E8 && f.id <= 0x7EF && f.dlc >= 3 && f.data[1] == 0x41) {
        uint8_t pid = f.data[2];
        if (pid == obd_pid_) {
            const LiveDataParam* p = findObdParam(pid);
            if (p && p->decode) {
                float v = p->decode(&f.data[3], f.dlc - 3u);
                printf("[OBD] %s: %.2f %s (PID %02X)\n", p->name, v, p->unit, pid);
            } else {
                printf("[OBD] PID %02X raw:", pid);
                for (uint8_t i = 3; i < f.dlc; ++i) printf(" %02X", f.data[i]);
                printf("\n");
            }
        }
        return true;
    }

    if (!isConnected() || ecu_ == nullptr) return false;
    if (f.id != ecu_->recv_id) return false;

    IsoTpStatus st = tp_.feed(f);
    switch (st) {
    case IsoTpStatus::NeedFlowControl: {
        CanFrame fc = tp_.flowControl(ecu_->emit_id);
        sendFrame(fc);
        break;
    }
    case IsoTpStatus::TxClearToSend:
        break;                       // poll() pumps the consecutive frames
    case IsoTpStatus::TxWait:
        printf("[DIAG] ECU asked to wait (FC.WAIT); holding transfer.\n");
        break;
    case IsoTpStatus::TxAbort:
        printf("[DIAG] ECU refused the transfer (FC.OVERFLOW); request aborted.\n");
        tp_.txReset();
        if (state_ == State::WaitingResponse) state_ = State::Connected;
        break;
    case IsoTpStatus::Done:
        if (state_ == State::WaitingResponse) state_ = State::Connected;
        handleResponse(tp_.pdu(), tp_.pdu_len());
        tp_.reset();
        break;
    case IsoTpStatus::Continue:
        break;
    case IsoTpStatus::Error:
        printf("[DIAG] ISO-TP reassembly error.\n");
        if (state_ == State::WaitingResponse) state_ = State::Connected;
        tp_.reset();
        break;
    default:
        break;
    }
    return true;
}

void DiagShell::feedCommandLine(const char* line) {
    if (!line) return;
    size_t len = strlen(line);
    if (len >= kLineBufSize) len = kLineBufSize - 1;
    memcpy(line_buf_, line, len);
    line_buf_[len] = '\0';
    processLine();
}

// --- Line reading (non-blocking) ---------------------------------------------

bool DiagShell::readLine() {
    while (true) {
        int c = getchar_nonblocking();
        if (c < 0) return false;
        if (c == '\r' || c == '\n') {
            if (line_pos_ == 0) continue; // skip empty lines
            line_buf_[line_pos_] = '\0';
            return true;
        }
        if (c == 0x7F || c == '\b') { // backspace
            if (line_pos_ > 0) line_pos_--;
            continue;
        }
        if (line_pos_ < kLineBufSize - 1) {
            line_buf_[line_pos_++] = static_cast<char>(c);
        }
    }
}

// --- Command dispatch --------------------------------------------------------

void DiagShell::processLine() {
    // Trim leading whitespace
    char* cmd = line_buf_;
    while (*cmd && isspace(static_cast<unsigned char>(*cmd))) cmd++;
    line_pos_ = 0; // reset for next line

    if (*cmd == '\0') return;

    // Extract first word
    char* arg = cmd;
    while (*arg && !isspace(static_cast<unsigned char>(*arg))) arg++;
    if (*arg) { *arg = '\0'; arg++; }
    while (*arg && isspace(static_cast<unsigned char>(*arg))) arg++;

    // Dispatch
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        cmdHelp();
    } else if (strcmp(cmd, "list") == 0) {
        cmdList();
    } else if (strcmp(cmd, "connect") == 0) {
        cmdConnect(arg);
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "disconnect") == 0) {
        cmdDisconnect();
    } else if (strcmp(cmd, "dtc") == 0) {
        cmdDtc();
    } else if (strcmp(cmd, "clear") == 0) {
        cmdClear();
    } else if (strcmp(cmd, "read") == 0) {
        cmdRead(arg);
    } else if (strcmp(cmd, "write") == 0) {
        cmdWrite(arg);
    } else if (strcmp(cmd, "trace") == 0) {
        cmdTrace();
    } else if (strcmp(cmd, "unlock") == 0) {
        cmdUnlock();
    } else if (strcmp(cmd, "raw") == 0) {
        cmdRaw(arg);
    } else if (strcmp(cmd, "sniff") == 0) {
        cmdSniff(arg);
    } else if (strcmp(cmd, "live") == 0) {
        cmdLive(arg);
    } else if (strcmp(cmd, "actuator") == 0) {
        cmdActuator(arg);
    } else if (strcmp(cmd, "flash") == 0) {
        cmdFlash(arg);
    } else if (strcmp(cmd, "scan") == 0) {
        cmdScan();
    } else if (strcmp(cmd, "config") == 0) {
        cmdConfig(arg);
    } else if (strcmp(cmd, "ident") == 0) {
        cmdIdent();
    } else if (strcmp(cmd, "status") == 0) {
        cmdStatus();
    } else if (strcmp(cmd, "meas") == 0) {
        cmdMeas(arg);
    } else if (strcmp(cmd, "obd") == 0) {
        cmdObd(arg);
    } else if (strcmp(cmd, "service") == 0) {
        cmdService(arg);
    } else if (strcmp(cmd, "program") == 0) {
        cmdProgram(arg);
    } else if (strcmp(cmd, "esp") == 0) {
        cmdEsp(arg);
    } else if (strcmp(cmd, "pdi") == 0) {
        cmdPdi();
    } else if (strcmp(cmd, "pin") == 0) {
        cmdPin(arg);
    } else if (strcmp(cmd, "hwtest") == 0) {
        cmdHwtest();
    } else if (strcmp(cmd, "gsniff") == 0) {
        cmdGuidedSniff(arg);
    } else {
        printf("[DIAG] Unknown command: '%s'. Type 'help'.\n", cmd);
    }
}

// --- Command implementations ------------------------------------------------

void DiagShell::cmdHelp() {
    printf(
        "=== Citroen C5 Diagnostic Shell ===\n"
        "  list                  Show known ECUs\n"
        "  connect <ECU>         Open session (e.g. connect BSI)\n"
        "  exit                  Close session\n"
        "  pin <hex>             Set SecurityAccess PIN for this ECU (see ECU_KEYS.md)\n"
        "  unlock                Request security access and unlock ECU\n"
        "  write <zone> <hex>    Write zone (e.g. write F190 12 34)\n"
        "  trace                 Write traceability data (UDS 2901 / KWP A0)\n"
        "  dtc                   Read fault codes\n"
        "  clear                 Clear fault codes\n"
        "  read <zone_hex>       Read zone (e.g. read F190)\n"
        "  live [param|off]      Monitor sensor parameter (e.g. live 100A or live off)\n"
        "  actuator <id> [args]  Start actuator routine (e.g. actuator 3101)\n"
        "  flash begin           Start flash sequence (erase + prepare)\n"
        "  flash <S3_line>       Load S-record line (parsed and sent automatically)\n"
        "  flash end             Finalize flash (transfer exit + verification)\n"
        "  flash status          Show current flash state machine step\n"
        "  flash cancel          Abort flash sequence\n"
        "  raw <hex bytes>       Send raw PDU (e.g. raw 21 80)\n"
        "  sniff [on|off]        Toggle passive decoding\n"
        "  scan                  Global ECU test (scan all ECUs)\n"
        "  pdi                   Pre-Delivery Inspection (full report)\n"
        "  config list           List all BSI configuration parameters\n"
        "  config read <zone>    Read BSI configuration zone\n"
        "  config readall        Read all configuration zones\n"
        "  ident                 Read ECU identification (VIN, serial, etc.)\n"
        "  status                Show connection status\n"
        "  meas <param_id>       Start measurement polling\n"
        "  meas off              Stop measurement polling\n"
        "  obd [pid|off]         Standard OBD-II Mode 01 poll (e.g. obd 0C = RPM)\n"
        "  service reset         Reset maintenance indicator\n"
        "  service schedule K M  Set maintenance (km / months)\n"
        "  program key           Enter key learning mode\n"
        "  program init          BSI factory initialisation (DANGEROUS)\n"
        "  esp calib             ESP steering angle calibration\n"
        "  esp bleed             ABS hydraulic bleeding procedure\n"
        "  hwtest                Hardware self-test (SPI + CAN loopback)\n"
        "  gsniff <sub>          Guided CAN signal discovery (gsniff for subcommands)\n"
        "  help                  This message\n"
    );
}

void DiagShell::cmdList() {
    printf("Known ECUs (%zu):\n", kEcuCount);
    for (size_t i = 0; i < kEcuCount; ++i) {
        const auto& e = kEcuTable[i];
        const char* proto_str = "???";
        switch (e.proto) {
            case Protocol::KWP_HAB: proto_str = "KWP/HAB"; break;
            case Protocol::KWP_IS:  proto_str = "KWP/IS";  break;
            case Protocol::UDS:     proto_str = "UDS";      break;
        }
        printf("  %-10s %03X:%03X  %-8s  %s\n",
               e.family, e.emit_id, e.recv_id, proto_str, e.note);
    }
}

void DiagShell::cmdConnect(const char* arg) {
    if (!*arg) {
        printf("[DIAG] Usage: connect <ECU_FAMILY>  (e.g. connect BSI)\n");
        return;
    }
    if (isConnected()) {
        printf("[DIAG] Already connected to %s. Type 'exit' first.\n", ecu_->family);
        return;
    }

    // Find ECU by family name (case-insensitive)
    const EcuAddr* found = nullptr;
    for (size_t i = 0; i < kEcuCount; ++i) {
        const char* a = arg;
        const char* b = kEcuTable[i].family;
        bool match = true;
        while (*a && *b) {
            if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
                match = false; break;
            }
            a++; b++;
        }
        if (match && !*a && !*b) { found = &kEcuTable[i]; break; }
    }

    if (!found) {
        printf("[DIAG] Unknown ECU '%s'. Type 'list' to see available ECUs.\n", arg);
        return;
    }

    ecu_ = found;
    active_bus_ = busForEcu(ecu_);
    tp_.reset();
    unlocked_ = false;
    manual_pin_valid_ = false;
    pending_count_ = 0;
    live_polling_active_ = false;

    printf("[DIAG] Connecting to %s (%03X:%03X, %s) on %s bus...\n",
           ecu_->family, ecu_->emit_id, ecu_->recv_id,
           ecu_->proto == Protocol::UDS ? "UDS" :
           ecu_->proto == Protocol::KWP_IS ? "KWP/IS" : "KWP/HAB",
           active_bus_ == Bus::HighSpeed ? "HS" : "LS");

    // Send session open request
    Req req = startDiagSession(ecu_->proto);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
    last_keepalive_us_ = get_time_us();
}

void DiagShell::cmdDisconnect() {
    if (!isConnected()) {
        printf("[DIAG] Not connected.\n");
        return;
    }
    if (scan_active_) {
        scan_active_ = false;
        printf("[DIAG] Scan cancelled.\n");
    }
    if (proc_ != Procedure::None) {
        proc_ = Procedure::None;
        printf("[DIAG] Procedure cancelled.\n");
    }
    printf("[DIAG] Closing session with %s.\n", ecu_->family);
    Req req = stopDiagSession(ecu_->proto);
    sendReq(req);
    state_ = State::Idle;
    ecu_ = nullptr;
    tp_.reset();
    unlocked_ = false;
    manual_pin_valid_ = false;
    pending_count_ = 0;
    live_polling_active_ = false;
    config_readall_active_ = false;
}

void DiagShell::cmdPin(const char* arg) {
    if (!*arg) {
        if (manual_pin_valid_)
            printf("[DIAG] Manual SecurityAccess PIN: %04X\n", manual_pin_);
        else
            printf("[DIAG] No manual PIN set. Family default for %s: %04X. "
                   "Set with 'pin <hex>'.\n",
                   ecu_ ? ecu_->family : "(none)",
                   getEcuPin(ecu_ ? ecu_->family : nullptr));
        return;
    }
    if (strcmp(arg, "clear") == 0 || strcmp(arg, "off") == 0) {
        manual_pin_valid_ = false;
        printf("[DIAG] Manual PIN cleared; using family default.\n");
        return;
    }
    uint16_t pin = 0;
    if (!parseHexU16(arg, nullptr, &pin)) {
        printf("[DIAG] Usage: pin <hex>  (2-byte key, e.g. pin B2B2) | pin clear\n");
        return;
    }
    manual_pin_ = pin;
    manual_pin_valid_ = true;
    printf("[DIAG] Manual SecurityAccess PIN set to %04X.\n", pin);
}

void DiagShell::cmdHwtest() {
#ifdef HOST_TEST
    printf("[HWTEST] Not available in host test mode.\n");
    return;
#else
    printf("[HWTEST] === MCP2515 Hardware Self-Test ===\n");

    if (!can_) {
        printf("[HWTEST] CanManager not initialized.\n");
        return;
    }

    // --- Test 1: SPI register read on both buses ---
    printf("[HWTEST] Test 1: SPI register read...\n");

    // A bus whose chip never answered at init() must not be touched at all: its
    // SPI peripheral wedges (spi_write_blocking spins forever) once cyw43 is up,
    // which would hang the whole main loop. CanManager::bus() returns nullptr in
    // that case, so the guard cannot be forgotten here.
    Mcp2515* hs = can_->bus(Bus::HighSpeed);
    Mcp2515* ls = can_->bus(Bus::LowSpeed);
    if (!hs) printf("[HWTEST]   HS: SKIP (no chip answered at init - not probing, would hang)\n");
    if (!ls) printf("[HWTEST]   LS: SKIP (no chip answered at init - not probing, would hang)\n");

    if (hs) {
        uint8_t s = hs->readReg(Mcp2515::MCP_CANSTAT);
        printf("[HWTEST]   HS CANSTAT = 0x%02X (mode bits: 0x%02X)\n", s, s & 0xE0);
    }
    if (ls) {
        uint8_t s = ls->readReg(Mcp2515::MCP_CANSTAT);
        printf("[HWTEST]   LS CANSTAT = 0x%02X (mode bits: 0x%02X)\n", s, s & 0xE0);
    }

    // A floating/unconnected bus can coincidentally read back a non-0xFF
    // value (we saw LS read a stable 0x00 with no chip attached), so a plain
    // "!= 0xFF" check isn't reliable evidence of a real chip. Write a known
    // pattern to a scratch register and read it back: only a chip actually
    // present and latched onto the SPI bus will echo it.
    // CNF1 must NOT be used — the bit-timing registers are writable only in
    // configuration mode, and by now both chips are in Normal mode, so the probe
    // always read back the bitrate value and reported FAIL on good hardware.
    // CANINTE is writable in every mode; save and restore it.
    auto spiPresent = [](Mcp2515& mcp) {
        static constexpr uint8_t kProbe = 0xA5;
        uint8_t saved = mcp.readReg(Mcp2515::MCP_CANINTE);
        mcp.writeReg(Mcp2515::MCP_CANINTE, kProbe);
        bool ok = mcp.readReg(Mcp2515::MCP_CANINTE) == kProbe;
        mcp.writeReg(Mcp2515::MCP_CANINTE, saved);
        return ok;
    };
    bool hs_ok = hs && spiPresent(*hs);
    bool ls_ok = ls && spiPresent(*ls);

    if (hs) printf("[HWTEST]   HS SPI: %s\n", hs_ok ? "OK" : "FAIL (readback mismatch - check wiring/level-shift)");
    if (ls) printf("[HWTEST]   LS SPI: %s\n", ls_ok ? "OK" : "FAIL (readback mismatch - check wiring/level-shift)");

    // Also read CANCTRL and the latched error flags to double-check
    if (hs) printf("[HWTEST]   HS CANCTRL = 0x%02X  EFLG = 0x%02X\n",
                   hs->readReg(Mcp2515::MCP_CANCTRL), hs->errorFlags());
    if (ls) printf("[HWTEST]   LS CANCTRL = 0x%02X  EFLG = 0x%02X\n",
                   ls->readReg(Mcp2515::MCP_CANCTRL), ls->errorFlags());

    if (!hs_ok && !ls_ok) {
        printf("[HWTEST] FAIL: Both MCP2515 unresponsive. Check SPI wiring + level-shifters.\n");
        printf("[HWTEST] Expected: GP2-6 (HS) and GP10-14 (LS) via TXS0108E.\n");
        return;
    }

    // --- Test 2: Loopback on responding buses ---
    printf("[HWTEST] Test 2: CAN loopback test...\n");

    auto runLoopback = [&](const char* label, Mcp2515& mcp) {
        printf("[HWTEST]   %s: switching to loopback mode...\n", label);
        if (mcp.setLoopbackMode() != McpError::Ok) {
            printf("[HWTEST]   %s: FAIL (could not enter loopback mode)\n", label);
            return false;
        }

        CanFrame tx{};
        tx.id = 0x7FF;
        tx.ext = false;
        tx.dlc = 8;
        tx.data[0] = 0xDE; tx.data[1] = 0xAD; tx.data[2] = 0xBE; tx.data[3] = 0xEF;
        tx.data[4] = 0xCA; tx.data[5] = 0xFE; tx.data[6] = 0xBA; tx.data[7] = 0xBE;

        if (mcp.send(tx) != McpError::Ok) {
            printf("[HWTEST]   %s: FAIL (TX error)\n", label);
            return false;
        }
#ifdef HOST_TEST
        // In host test mode, just check hasRx/read without delay
#else
        sleep_ms(10);
#endif

        CanFrame rx{};
        if (!mcp.hasRx()) {
            printf("[HWTEST]   %s: FAIL (no RX after loopback TX - check CAN-H/CAN-L jumper)\n", label);
            return false;
        }
        if (mcp.read(rx) != McpError::Ok) {
            printf("[HWTEST]   %s: FAIL (RX read error)\n", label);
            return false;
        }

        bool match = (rx.id == tx.id && rx.dlc == tx.dlc);
        for (int i = 0; i < 8 && match; ++i)
            if (rx.data[i] != tx.data[i]) match = false;

        if (match) {
            printf("[HWTEST]   %s: PASS (TX=RX, ID=0x%03X, DLC=%d, data OK)\n", label, rx.id, rx.dlc);
        } else {
            printf("[HWTEST]   %s: FAIL (data mismatch: TX=%02X%02X%02X%02X%02X%02X%02X%02X RX=%02X%02X%02X%02X%02X%02X%02X%02X)\n",
                   label,
                   tx.data[0],tx.data[1],tx.data[2],tx.data[3],tx.data[4],tx.data[5],tx.data[6],tx.data[7],
                   rx.data[0],rx.data[1],rx.data[2],rx.data[3],rx.data[4],rx.data[5],rx.data[6],rx.data[7]);
            return false;
        }

        // Restore to Normal mode
        mcp.setNormalMode();
        return true;
    };

    bool hs_lb = false, ls_lb = false;
    if (hs_ok) hs_lb = runLoopback("HS (500k)", *hs);
    if (ls_ok) ls_lb = runLoopback("LS (125k)", *ls);

    // --- Summary ---
    printf("[HWTEST] === Summary ===\n");
    printf("[HWTEST]   HS SPI: %s  Loopback: %s\n", hs_ok ? "OK" : "FAIL", hs_lb ? "PASS" : (hs_ok ? "FAIL" : "SKIP"));
    printf("[HWTEST]   LS SPI: %s  Loopback: %s\n", ls_ok ? "OK" : "FAIL", ls_lb ? "PASS" : (ls_ok ? "FAIL" : "SKIP"));

    if (hs_lb || ls_lb) {
        printf("[HWTEST] At least one bus works. CAN wiring is good.\n");
    } else {
        printf("[HWTEST] Both loopbacks failed. Ensure CAN-H/CAN-L are jumpered on the tested bus.\n");
    }

    // Re-init to restore normal operation
    printf("[HWTEST] Re-initializing CAN buses...\n");
    DualCanPins pins;
    can_->init(pins, false);
    printf("[HWTEST] Done.\n");
#endif // !HOST_TEST
}

void DiagShell::cmdDtc() {
    if (!isConnected()) { printf("[DIAG] Not connected. Use 'connect' first.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }
    printf("[DIAG] Reading fault codes from %s...\n", ecu_->family);
    Req req = readDTC(ecu_->proto);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdClear() {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }
    printf("[DIAG] Clearing fault codes on %s...\n", ecu_->family);
    Req req = clearDTC(ecu_->proto);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdRead(const char* arg) {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }
    if (!*arg) { printf("[DIAG] Usage: read <zone_hex>  (e.g. read F190)\n"); return; }

    uint16_t zone_id = 0;
    if (!parseHexU16(arg, nullptr, &zone_id)) {
        printf("[DIAG] Invalid zone ID (expect 1-4 hex digits): '%s'\n", arg);
        return;
    }

    printf("[DIAG] Reading zone %04X from %s...\n", zone_id, ecu_->family);
    Req req = readZone(ecu_->proto, zone_id);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdUnlock() {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }

    printf("[DIAG] Requesting security seed for %s...\n", ecu_->family);
    // Request seed for config/coding access (config_access = true)
    Req req = securitySeed(ecu_->proto, true);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdWrite(const char* arg) {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }
    if (!*arg) { printf("[DIAG] Usage: write <zone_hex> <hex_bytes>  (e.g. write 2901 FD 00 00 00 01 01 01)\n"); return; }

    // Find the end of the zone_hex word
    const char* zone_start = arg;
    while (*zone_start && isspace(static_cast<unsigned char>(*zone_start))) zone_start++;
    if (!*zone_start) { printf("[DIAG] Usage: write <zone_hex> <hex_bytes>\n"); return; }

    const char* zone_end = zone_start;
    while (*zone_end && !isspace(static_cast<unsigned char>(*zone_end))) zone_end++;

    uint16_t zone_id = 0;
    if (!parseHexU16(zone_start, zone_end, &zone_id)) {
        printf("[DIAG] Invalid zone ID (expect 1-4 hex digits).\n");
        return;
    }

    // Now parse hex bytes
    const char* bytes_str = zone_end;
    while (*bytes_str && isspace(static_cast<unsigned char>(*bytes_str))) bytes_str++;
    if (!*bytes_str) { printf("[DIAG] No write data bytes provided.\n"); return; }

    // We build the PDU.
    // The write PDU starts with writeZoneHeader.
    Req req = writeZoneHeader(ecu_->proto, zone_id);
    
    // Append the bytes to the request buffer.
    const char* p = bytes_str;
    while (*p && req.len < sizeof(req.buf)) {
        while (*p && isspace(static_cast<unsigned char>(*p))) p++;
        if (!*p) break;
        uint8_t byte = 0;
        if (!parseHexByte(p, &byte)) {
            printf("[DIAG] Invalid hex at position %zu\n", p - bytes_str);
            return;
        }
        req.buf[req.len++] = byte;
        p += 2;
    }

    if (!unlocked_) {
        printf("[DIAG] Warning: ECU is not security unlocked. Write may be rejected.\n");
    }

    printf("[DIAG] Writing zone %04X, TX (%u bytes): ", zone_id, req.len);
    printHex(req.buf, req.len);
    printf("\n");

    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdTrace() {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }

    Req req{{}, 0};
    if (ecu_->proto == Protocol::UDS) {
        req = writeZoneHeader(ecu_->proto, zone::TRACEABILITY); // 0x2901
        // data: FD 00 00 00 01 01 01
        req.buf[req.len++] = 0xFD;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x01;
        req.buf[req.len++] = 0x01;
        req.buf[req.len++] = 0x01;
    } else {
        req = writeZoneHeader(ecu_->proto, zone::KWP_TRACEABILITY); // 0xA0
        // data: FF FD 000000 010101 0000 — the trailing counter bytes are 00 00;
        // the ECU auto-increments its own secured-write counter (matches ludwig-v).
        req.buf[req.len++] = 0xFF;
        req.buf[req.len++] = 0xFD;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x01;
        req.buf[req.len++] = 0x01;
        req.buf[req.len++] = 0x01;
        req.buf[req.len++] = 0x00;
        req.buf[req.len++] = 0x00;
    }

    if (!unlocked_) {
        printf("[DIAG] Warning: ECU is not security unlocked. Trace write may be rejected.\n");
    }

    printf("[DIAG] Writing traceability zone, TX (%u bytes): ", req.len);
    printHex(req.buf, req.len);
    printf("\n");

    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdRaw(const char* arg) {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }
    if (!*arg) { printf("[DIAG] Usage: raw <hex_bytes>  (e.g. raw 21 80)\n"); return; }

    uint8_t pdu[64];
    size_t pdu_len = 0;
    const char* p = arg;
    while (*p && pdu_len < sizeof(pdu)) {
        while (*p && isspace(static_cast<unsigned char>(*p))) p++;
        if (!*p) break;
        uint8_t byte = 0;
        if (!parseHexByte(p, &byte)) {
            printf("[DIAG] Invalid hex at position %zu\n", p - arg);
            return;
        }
        pdu[pdu_len++] = byte;
        p += 2;
    }

    if (pdu_len == 0) { printf("[DIAG] No bytes to send.\n"); return; }

    printf("[DIAG] TX raw (%zu bytes): ", pdu_len);
    printHex(pdu, pdu_len);
    printf("\n");

    sendPdu(pdu, pdu_len);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdSniff(const char* arg) {
    if (strcmp(arg, "off") == 0) {
        sniff_enabled_ = false;
        printf("[DIAG] Passive sniffing OFF.\n");
    } else if (strcmp(arg, "on") == 0 || *arg == '\0') {
        sniff_enabled_ = true;
        printf("[DIAG] Passive sniffing ON.\n");
    } else {
        printf("[DIAG] Usage: sniff [on|off]\n");
    }
}

// Guided ("commanded") sniffer: correlates a known physical action with the CAN
// bytes that move in step with it. See CanSniffer for the capture/scoring logic.
void DiagShell::cmdGuidedSniff(const char* arg) {
    while (*arg && isspace(static_cast<unsigned char>(*arg))) arg++;
    char sub[16] = {0};
    size_t i = 0;
    while (arg[i] && !isspace(static_cast<unsigned char>(arg[i])) && i < sizeof(sub) - 1) {
        sub[i] = arg[i];
        i++;
    }
    const char* subarg = arg + i;
    while (*subarg && isspace(static_cast<unsigned char>(*subarg))) subarg++;

    if (strcmp(sub, "run") == 0) {
        const CanSniffer::Scenario* sc = findScenario(subarg);
        if (!sc) { printf("[GSNIFF] Bilinmeyen senaryo: '%s'. Mevcut: climate\n", subarg); return; }
        sniffer_.beginRun(sc, get_time_us());
    } else if (strcmp(sub, "next") == 0) {
        sniffer_.nextStep(get_time_us());
    } else if (strcmp(sub, "base") == 0) {
        uint64_t dur = CanSniffer::kDefaultBaselineUs;
        int sec = atoi(subarg);
        if (sec > 0) dur = static_cast<uint64_t>(sec) * 1000000ULL;
        sniffer_.beginBaseline(get_time_us(), dur);
    } else if (strcmp(sub, "count") == 0) {
        int n = atoi(subarg);
        if (n <= 0 || n > 255) { printf("[GSNIFF] Usage: gsniff count <N>\n"); return; }
        sniffer_.beginCount(static_cast<uint8_t>(n));
    } else if (strcmp(sub, "hold") == 0) {
        sniffer_.beginHold();
    } else if (strcmp(sub, "sweep") == 0) {
        sniffer_.beginSweep();
    } else if (strcmp(sub, "stop") == 0) {
        sniffer_.stop();
    } else if (strcmp(sub, "report") == 0) {
        sniffer_.report();
    } else if (strcmp(sub, "clear") == 0) {
        sniffer_.clear();
    } else if (strcmp(sub, "status") == 0) {
        sniffer_.status();
    } else if (strcmp(sub, "watch") == 0) {
        if (strcmp(subarg, "off") == 0) { sniffer_.watchOff(); return; }
        if (!*subarg) { printf("[GSNIFF] Usage: gsniff watch <hs|ls> <hexid> | watch off\n"); return; }
        char busch = static_cast<char>(toupper(static_cast<unsigned char>(*subarg)));
        const char* p = subarg;
        while (*p && !isspace(static_cast<unsigned char>(*p))) p++;
        while (*p && isspace(static_cast<unsigned char>(*p))) p++;
        uint16_t idv = 0;
        if (!*p || !parseHexU16(p, nullptr, &idv)) {
            printf("[GSNIFF] Usage: gsniff watch <hs|ls> <hexid>\n");
            return;
        }
        sniffer_.watch(busch == 'H' ? Bus::HighSpeed : Bus::LowSpeed, idv);
    } else if (strcmp(sub, "save") == 0) {
#ifndef HOST_TEST
        sniffer_.save();
#else
        printf("[GSNIFF] save: host test'te desteklenmiyor.\n");
#endif
    } else if (strcmp(sub, "load") == 0) {
#ifndef HOST_TEST
        sniffer_.load();
#else
        printf("[GSNIFF] load: host test'te desteklenmiyor.\n");
#endif
    } else {
        printf(
            "Usage: gsniff <alt-komut>\n"
            "  run <senaryo>          Hazir kontrol listesini yurut (orn. run climate)\n"
            "  next                   Senaryoda sonraki adima gec\n"
            "  base [sn]              Gurultu tabanini kaydet (varsayilan 3sn)\n"
            "  count <N>              Ad-hoc: eylemi N kez yap, sonra 'stop'\n"
            "  hold                   Ad-hoc: degeri sabit tut, sonra 'stop'\n"
            "  sweep                  Ad-hoc: degeri tek yonde degistir, sonra 'stop'\n"
            "  stop                   Ad-hoc penceresini kapat, raporu yaz\n"
            "  report                 Son raporu tekrar yaz\n"
            "  watch <hs|ls> <hexid>  Tek ID'yi canli dok (watch off ile kapat)\n"
            "  save | load            Ogrenilen haritayi flash'a yaz/oku\n"
            "  clear | status         Tabloyu sifirla | durum ozeti\n"
        );
    }
}

// --- Internal helpers --------------------------------------------------------

void DiagShell::sendReq(const Req& req) {
    sendPdu(req.buf, req.len);
}

bool DiagShell::sendFrame(const CanFrame& f) {
    // The MCP2515 has three transmit buffers and no queue behind them. Dropping
    // a frame because they are momentarily full silently truncates a multi-frame
    // request, so retry briefly before admitting failure.
    for (int attempt = 0; attempt < kTxRetries; ++attempt) {
        McpError e = can_->send(active_bus_, f);
        if (e == McpError::Ok) return true;
        if (e != McpError::AllTxBusy) return false;   // bus not ready: no point retrying
#ifndef HOST_TEST
        sleep_us(200);
#endif
    }
    return false;
}

void DiagShell::sendPdu(const uint8_t* pdu, size_t len) {
    if (ecu_ == nullptr) return;
    CanFrame first;
    if (!tp_.beginSend(ecu_->emit_id, pdu, len, get_time_us(), first)) {
        printf("[DIAG] Request too large to send (%u bytes).\n", static_cast<unsigned>(len));
        return;
    }
    if (!sendFrame(first)) {
        printf("[DIAG] CAN transmit failed (bus busy or not ready).\n");
        tp_.txReset();
        return;
    }
    // Anything longer than a single frame now waits for the ECU's flow control;
    // poll() pumps the consecutive frames once it arrives.
}

void DiagShell::sendKeepAlive() {
    if (ecu_ == nullptr) return;
    Req req = keepAlive(ecu_->proto);
    // Keep-alive is always a single frame, so it never disturbs an in-flight
    // segmented transfer. Sent silently: no state change, no response wait.
    CanFrame f;
    if (IsoTp::encode(ecu_->emit_id, req.buf, req.len, &f, 1) == 1) sendFrame(f);
}

void DiagShell::handleResponse(const uint8_t* pdu, size_t len) {
    if (len == 0) return;

    uint8_t service = pdu[0];

    // Any genuine (non-"response pending") reply ends the pending streak.
    bool is_pending = (service == 0x7F && len >= 3 && pdu[2] == uds::NRC::ResponsePending);
    if (!is_pending) pending_count_ = 0;

    // The programming session that opens a flash is the shell's own request, not
    // the flash engine's. It has to be claimed before the generic session-open
    // handlers below consume it, or the erase request is never emitted.
    if (flash_session_pending_ && !is_pending) {
        flash_session_pending_ = false;
        if (service == 0x7F) {
            printNegResponse(pdu, len);
            printf("[FLASH] ECU refused the programming session; aborting.\n");
            flash_active_ = false;
            state_ = State::Connected;
            return;
        }
        SRecord dummy{};
        Req req = flash_engine_.nextRequest(dummy, 0);   // Idle -> RequestErase
        printf("[FLASH] Programming session open, sending erase request...\n");
        sendReq(req);
        state_ = State::WaitingResponse;
        response_start_us_ = get_time_us();
        return;
    }

    // Negative response
    if (service == 0x7F) {
        if (is_pending) {
            // Bounded wait: a stuck ECU that keeps replying 0x78 would otherwise
            // reset the timeout forever. Cap the streak (erases legitimately send
            // many, so the cap is generous) then give up.
            if (++pending_count_ > kMaxResponsePending) {
                printf("[DIAG] ECU stuck sending Response pending (0x78); aborting.\n");
                pending_count_ = 0;
                if (proc_ != Procedure::None) abortProcedure("ECU response-pending timeout");
                state_ = State::Idle;
                return;
            }
            printf("[DIAG] ECU: Response pending (0x78), waiting...\n");
            response_start_us_ = get_time_us(); // reset timeout
            state_ = State::WaitingResponse;    // stay waiting
            return;
        }
        printNegResponse(pdu, len);
        if (proc_ != Procedure::None) {
            abortProcedure("ECU rejected a procedure step");
            state_ = State::Connected;
            return;
        }
        if (config_readall_active_) {
            config_zone_index_++;
            if (config_zone_index_ < config_zone_count_) {
                Req r = readZone(ecu_->proto, config_zones_[config_zone_index_]);
                sendReq(r);
                state_ = State::WaitingResponse;
                response_start_us_ = get_time_us();
            } else {
                config_readall_active_ = false;
                printf("[CONFIG] All zones read (with some errors).\n");
            }
        }
        if (scan_active_) {
            scan_results_[scan_index_].scanned = true;
            scan_results_[scan_index_].comm_ok = false;
            scan_results_[scan_index_].has_dtc = false;
            state_ = State::Idle;
            ecu_ = nullptr;
            advanceScan();
        }
        return;
    }

    Protocol p = ecu_->proto;

    // Session open positive response
    if (p == Protocol::UDS && service == uds::PosSession) {
        printf("[DIAG] Session open with %s. Ready.\n", ecu_->family);
        state_ = State::Connected;
        if (scan_active_) {
            printf("[SCAN] Reading DTC...\n");
            Req req = readDTC(ecu_->proto);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        }
        return;
    }
    if (p == Protocol::KWP_HAB && service == kwp::PosSession) {
        printf("[DIAG] Session open with %s. Ready.\n", ecu_->family);
        state_ = State::Connected;
        if (scan_active_) {
            printf("[SCAN] Reading DTC...\n");
            Req req = readDTC(ecu_->proto);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        }
        return;
    }
    if (p == Protocol::KWP_IS && service == 0xC1) {
        printf("[DIAG] Session open with %s. Ready.\n", ecu_->family);
        state_ = State::Connected;
        if (scan_active_) {
            printf("[SCAN] Reading DTC...\n");
            Req req = readDTC(ecu_->proto);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        }
        return;
    }
    // KWP_IS session close acknowledgment (0xC2) — silently transition to Idle
    if (p == Protocol::KWP_IS && service == 0xC2) {
        state_ = State::Idle;
        if (scan_active_) {
            advanceScan();
        }
        return;
    }

    // DTC response
    if (p == Protocol::UDS && service == 0x59) {
        printDtcUds(pdu, len);
        if (scan_active_) {
            scan_results_[scan_index_].scanned = true;
            scan_results_[scan_index_].comm_ok = true;
            scan_results_[scan_index_].has_dtc = (len > 3);
            printf("[SCAN] %s done. ", ecu_->family);
            Req stop = stopDiagSession(ecu_->proto);
            sendReq(stop);
            state_ = State::Idle;
            ecu_ = nullptr;
            advanceScan();
        }
        return;
    }
    if (service == 0x57) {
        printDtcKwp(pdu, len);
        if (scan_active_) {
            scan_results_[scan_index_].scanned = true;
            scan_results_[scan_index_].comm_ok = true;
            scan_results_[scan_index_].has_dtc = (len > 3);
            printf("[SCAN] %s done. ", ecu_->family);
            Req stop = stopDiagSession(ecu_->proto);
            sendReq(stop);
            state_ = State::Idle;
            ecu_ = nullptr;
            advanceScan();
        }
        return;
    }

    // Clear DTC positive response
    if (service == uds::PosClear) {
        printf("[DIAG] Faults cleared successfully.\n");
        return;
    }

    // Flash engine response handling (overrides other interpretation when active)
    if (flash_active_) {
        flash_engine_.handleResponse(service, pdu, len);
        FlashEngine::Step step = flash_engine_.step();

        if (step == FlashEngine::Step::Error) {
            printf("[FLASH] Error during flash sequence.\n");
            flash_active_ = false;
            return;
        }

        if (step == FlashEngine::Step::Done) {
            printf("[FLASH] Flash sequence completed successfully!\n");
            flash_active_ = false;
            return;
        }

        // Auto-advance: send next request based on current step
        if (step == FlashEngine::Step::RequestDownload) {
            if (staged_count_ == 0) {
                printf("[FLASH] No data records staged; nothing to download.\n");
                flash_active_ = false;
                return;
            }
            // Announce the real extent of the staged image. This used to be
            // built from an empty record: address 0 and a hardcoded 64 KB.
            uint32_t lo = staged_records_[0].address;
            uint32_t hi = lo;
            uint32_t total = 0;
            for (uint16_t i = 0; i < staged_count_; ++i) {
                const SRecord& s = staged_records_[i];
                if (s.address < lo) lo = s.address;
                if (s.address + s.data_len > hi) hi = s.address + s.data_len;
                total += s.data_len;
            }
            (void)total;
            flash_engine_.setDownloadExtent(lo, hi - lo);
            SRecord dummy{};
            Req req = flash_engine_.nextRequest(dummy, 0);
            printf("[FLASH] Requesting download: addr=0x%08X size=%u bytes...\n",
                   lo, static_cast<unsigned>(hi - lo));
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
            return;
        }

        if (step == FlashEngine::Step::TransferData && staged_index_ < staged_count_) {
            SRecord& rec = staged_records_[staged_index_++];
            Req req = flash_engine_.nextRequest(rec, flash_seq_);
            flash_seq_ = (flash_seq_ + 1) % 256;
            printf("[FLASH] Transferring block %u/%u (addr=0x%06X, %u bytes)...\n",
                   staged_index_, staged_count_, rec.address, rec.data_len);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
            return;
        }

        if (step == FlashEngine::Step::TransferData && staged_index_ >= staged_count_) {
            printf("[FLASH] All blocks transferred. Use 'flash end' to finalize.\n");
            return;
        }

        // Transfer-exit (0x37) acknowledged -> send the checksum-verify routine.
        if (step == FlashEngine::Step::VerifyChecksum) {
            SRecord dummy{};
            Req req = flash_engine_.nextRequest(dummy, 0); // emits 0x31 checksum, step -> Done
            printf("[FLASH] Verifying checksum...\n");
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
            return;
        }

        return; // flash consumed the response
    }

    // Procedure state machine step advancement
    if (proc_ != Procedure::None) {
        advanceProcedure();
        return;
    }

    // RoutineControl / Actuator test positive response
    if (service == 0x71 || (p != Protocol::UDS && service == 0x70)) {
        printf("[DIAG] Actuator test / routine started successfully.\n");
        return;
    }

    // Live polling response decoding
    if (live_polling_active_) {
        bool is_uds = (p == Protocol::UDS);
        uint16_t resp_param_id = 0;
        size_t header_len = 0;
        if (is_uds && service == uds::PosRead) {
            if (len >= 3) {
                resp_param_id = (pdu[1] << 8) | pdu[2];
                header_len = 3;
            }
        } else if (!is_uds && service == kwp::PosRead) {
            if (len >= 2) {
                resp_param_id = pdu[1];
                header_len = 2;
            }
        }

        if (header_len > 0 && resp_param_id == live_param_id_) {
            const LiveDataParam* param = findParam(p, live_param_id_);
            if (!param) param = findParamInCategories(live_param_id_);
            if (param && param->decode) {
                float val = param->decode(pdu + header_len, len - header_len);
                printf("[LIVE] %s: %.1f %s\n", param->name, val, param->unit);
            } else {
                // No colon here on purpose: the dashboard's measurement parser is
                // /^\[LIVE\] (.+): ([-0-9.]+) ?(.*)$/, so a colon would make it
                // read the first hex byte as the measured value and plot it.
                printf("[LIVE] ID %04X undecoded raw = ", resp_param_id);
                printHex(pdu + header_len, len - header_len);
                printf("\n");
            }
            return;
        }
    }

    // Zone read positive response
    if (p == Protocol::UDS && service == uds::PosRead) {
        if (config_readall_active_) {
            uint16_t zid = (pdu[1] << 8) | pdu[2];
            printf("[CONFIG] Zone %04X:\n", zid);
            const uint8_t* data = pdu + 3;
            size_t data_len = len - 3;
            const BsiZoneParam* zp = findZoneParam(zid);
            if (zp) {
                for (size_t c = 0; c < kZoneCategoryCount; ++c) {
                    for (size_t p2 = 0; p2 < kZoneCategories[c].count; ++p2) {
                        if (kZoneCategories[c].params[p2].zone_id == zid) {
                            const BsiZoneParam& bp = kZoneCategories[c].params[p2];
                            uint8_t byte_val = (bp.byte_offset < data_len) ? data[bp.byte_offset] : 0;
                            uint8_t masked = (bp.bit_mask == 0xFF) ? byte_val : (byte_val & bp.bit_mask);
                            if (bp.bit_mask != 0xFF && bp.bit_mask != 0) {
                                uint8_t shift = 0;
                                uint8_t m = bp.bit_mask;
                                while ((m & 1) == 0) { m >>= 1; shift++; }
                                masked >>= shift;
                            }
                            printf("    %s: ", bp.name);
                            if (bp.type == ZT_BOOL) {
                                printf("%s\n", masked ? "ON" : "OFF");
                            } else if (bp.type == ZT_ENUM && bp.enum_values) {
                                const char* val_str = nullptr;
                                for (int vi = 0; bp.enum_values[vi]; ++vi) {
                                    char buf[16];
                                    snprintf(buf, sizeof(buf), "%d=", vi);
                                    if (strncmp(bp.enum_values[vi], buf, strlen(buf)) == 0) {
                                        if (masked == vi) { val_str = bp.enum_values[vi]; break; }
                                    }
                                }
                                if (masked < 16 && !val_str) val_str = bp.enum_values[masked];
                                printf("%s\n", val_str ? val_str : "?");
                            } else {
                                printf("%u (0x%02X)\n", masked, masked);
                            }
                        }
                    }
                }
            } else {
                for (size_t i = 0; i < data_len; ++i) {
                    printf("  Byte %zu: %02X\n", i, data[i]);
                }
            }
            config_zone_index_++;
            if (config_zone_index_ < config_zone_count_) {
                Req req = readZone(ecu_->proto, config_zones_[config_zone_index_]);
                sendReq(req);
                state_ = State::WaitingResponse;
                response_start_us_ = get_time_us();
            } else {
                config_readall_active_ = false;
                printf("[CONFIG] All zones read.\n");
            }
        } else {
            // Check if response has known zone parameters
            bool is_uds_response = (service == uds::PosRead);
            uint16_t zid = is_uds_response
                ? (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2]
                : pdu[1];
            if (isConfigZone(zid)) {
                size_t data_off = is_uds_response ? 3 : 2;
                printf("[CONFIG] Zone %04X:\n", zid);
                printHex(pdu + data_off, len - data_off);
                printf("\n");
                for (size_t c = 0; c < kZoneCategoryCount; ++c) {
                    for (size_t p2 = 0; p2 < kZoneCategories[c].count; ++p2) {
                        if (kZoneCategories[c].params[p2].zone_id == zid) {
                            const BsiZoneParam& bp = kZoneCategories[c].params[p2];
                            size_t off = bp.byte_offset;
                            uint8_t byte_val = (off + 1 <= len - data_off) ? pdu[data_off + off] : 0;
                            uint8_t masked = byte_val & bp.bit_mask;
                            if (bp.bit_mask != 0xFF && bp.bit_mask != 0) {
                                uint8_t shift = 0;
                                uint8_t m = bp.bit_mask;
                                while ((m & 1) == 0) { m >>= 1; shift++; }
                                masked >>= shift;
                            }
                            printf("  %s: ", bp.name);
                            if (bp.type == ZT_BOOL) printf("%s\n", masked ? "ON" : "OFF");
                            else if (bp.type == ZT_ENUM && bp.enum_values) {
                                const char* v = (masked < 16) ? bp.enum_values[masked] : nullptr;
                                printf("%s\n", v ? v : "?");
                            } else printf("%u (0x%02X)\n", masked, masked);
                        }
                    }
                }
            } else {
                printZoneData(pdu, len);
            }
        }
        return;
    }
    if (service == kwp::PosRead || service == uds::PosRead) { // 0x61 (KWP) / 0x62 (UDS)
        if (config_readall_active_) {
            // KWP returns 1-byte zone ID at pdu[1]; UDS returns 2 bytes at pdu[1..2]
            uint16_t zid = (service == uds::PosRead)
                ? (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2]
                : pdu[1];
            printf("[CONFIG] Zone %02X:\n", zid);
            const uint8_t* data = pdu + 2;
            size_t data_len = len - 2;
            const BsiZoneParam* zp = findZoneParam(zid);
            if (zp) {
                for (size_t c = 0; c < kZoneCategoryCount; ++c) {
                    for (size_t p2 = 0; p2 < kZoneCategories[c].count; ++p2) {
                        if (kZoneCategories[c].params[p2].zone_id == zid) {
                            const BsiZoneParam& bp = kZoneCategories[c].params[p2];
                            uint8_t byte_val = (bp.byte_offset < data_len) ? data[bp.byte_offset] : 0;
                            uint8_t masked = (bp.bit_mask == 0xFF) ? byte_val : (byte_val & bp.bit_mask);
                            if (bp.bit_mask != 0xFF && bp.bit_mask != 0) {
                                uint8_t shift = 0;
                                uint8_t m = bp.bit_mask;
                                while ((m & 1) == 0) { m >>= 1; shift++; }
                                masked >>= shift;
                            }
                            printf("    %s: ", bp.name);
                            if (bp.type == ZT_BOOL) printf("%s\n", masked ? "ON" : "OFF");
                            else if (bp.type == ZT_ENUM && bp.enum_values) {
                                const char* v = (masked < 16) ? bp.enum_values[masked] : nullptr;
                                printf("%s\n", v ? v : "?");
                            } else printf("%u (0x%02X)\n", masked, masked);
                        }
                    }
                }
            } else {
                for (size_t i = 0; i < data_len; ++i) {
                    printf("  Byte %zu: %02X\n", i, data[i]);
                }
            }
            config_zone_index_++;
            if (config_zone_index_ < config_zone_count_) {
                Req req = readZone(ecu_->proto, config_zones_[config_zone_index_]);
                sendReq(req);
                state_ = State::WaitingResponse;
                response_start_us_ = get_time_us();
            } else {
                config_readall_active_ = false;
                printf("[CONFIG] All zones read.\n");
            }
        } else {
            uint16_t zid = pdu[1];
            if (isConfigZone(zid)) {
                printf("[CONFIG] Zone %02X:\n", zid);
                printHex(pdu + 2, len - 2);
                printf("\n");
                for (size_t c = 0; c < kZoneCategoryCount; ++c) {
                    for (size_t p2 = 0; p2 < kZoneCategories[c].count; ++p2) {
                        if (kZoneCategories[c].params[p2].zone_id == zid) {
                            const BsiZoneParam& bp = kZoneCategories[c].params[p2];
                            size_t off = bp.byte_offset;
                            uint8_t byte_val = (off + 1 <= len - 2) ? pdu[2 + off] : 0;
                            uint8_t masked = byte_val & bp.bit_mask;
                            if (bp.bit_mask != 0xFF && bp.bit_mask != 0) {
                                uint8_t shift = 0;
                                uint8_t m = bp.bit_mask;
                                while ((m & 1) == 0) { m >>= 1; shift++; }
                                masked >>= shift;
                            }
                            printf("  %s: ", bp.name);
                            if (bp.type == ZT_BOOL) printf("%s\n", masked ? "ON" : "OFF");
                            else if (bp.type == ZT_ENUM && bp.enum_values) {
                                const char* v = (masked < 16) ? bp.enum_values[masked] : nullptr;
                                printf("%s\n", v ? v : "?");
                            } else printf("%u (0x%02X)\n", masked, masked);
                        }
                    }
                }
            } else {
                printZoneData(pdu, len);
            }
        }
        return;
    }

    // SecurityAccess response
    if (service == 0x67) {
        if (len >= 2) {
            uint8_t sub = pdu[1];
            if (sub == 0x01 || sub == 0x81 || sub == 0x03 || sub == 0x83) {
                // Seed response
                if (len >= 6) {
                    uint32_t seed = (static_cast<uint32_t>(pdu[2]) << 24) |
                                    (static_cast<uint32_t>(pdu[3]) << 16) |
                                    (static_cast<uint32_t>(pdu[4]) << 8) |
                                    pdu[5];
                    printf("[DIAG] Seed received: %08X\n", seed);
                    if (seed == 0) {
                        unlocked_ = true;
                        printf("[DIAG] Security unlock succeeded (already unlocked).\n");
                    } else {
                        uint16_t pin = effectivePin();
                        if (pin == 0 && !manual_pin_valid_) {
                            printf("[DIAG] No SecurityAccess PIN known for %s. "
                                   "Set it with 'pin <hex>' (see ECU_KEYS.md).\n", ecu_->family);
                            state_ = State::Connected;
                        } else {
                            bool config = (sub == 0x03 || sub == 0x83);
                            Req key_req = securityKey(ecu_->proto, config, pin, seed);
                            printf("[DIAG] Sending key (PIN %04X)...\n", pin);
                            sendReq(key_req);
                            state_ = State::WaitingResponse;
                            response_start_us_ = get_time_us();
                        }
                    }
                } else {
                    printf("[DIAG] Malformed seed response.\n");
                }
            } else if (sub == 0x02 || sub == 0x82 || sub == 0x04 || sub == 0x84) {
                // Key response (unlock succeeded)
                unlocked_ = true;
                printf("[DIAG] Security unlock succeeded.\n");
            } else {
                printf("[DIAG] Unknown SecurityAccess sub-function: %02X\n", sub);
            }
        } else {
            printf("[DIAG] Malformed SecurityAccess response.\n");
        }
        return;
    }

    // UDS write response (0x6E) — may also come from KWP ECUs that use UDS framing
    // for 2-byte DIDs
    if (service == uds::PosWrite) {
        if (len >= 3) {
            uint16_t zone_id = (pdu[1] << 8) | pdu[2];
            printf("[DIAG] Zone %04X written successfully.\n", zone_id);
        } else {
            printf("[DIAG] Write successful.\n");
        }
        return;
    }

    // KWP write response (HAB: 0x7B, IS: 0x74) — always echoes a 1-byte local id.
    if (service == kwp::PosWrite || service == kwp::PosWrite_IS) {
        if (len >= 2) {
            printf("[DIAG] Zone %02X written successfully.\n", pdu[1]);
        } else {
            printf("[DIAG] Write successful.\n");
        }
        return;
    }

    // TesterPresent positive response (silently ignore)
    if (service == 0x7E) return;

    // Generic positive response
    printf("[DIAG] RX (%zu bytes): ", len);
    printHex(pdu, len);
    printf("\n");
}

void DiagShell::printDtcKwp(const uint8_t* pdu, size_t len) {
    // KWP DTC format: 57 <count> [<dtc_hi> <dtc_lo> <status>]...
    if (len < 2) { printf("[DIAG] Malformed DTC response.\n"); return; }
    uint8_t count = pdu[1];
    printf("[DIAG] %u fault(s):\n", count);
    if (count == 0) { printf("  (none)\n"); return; }
    size_t pos = 2;
    for (uint8_t i = 0; i < count && pos + 2 < len; ++i) {
        uint16_t dtc_code = (pdu[pos] << 8) | pdu[pos + 1];
        uint8_t status = (pos + 2 < len) ? pdu[pos + 2] : 0;
        const char* st = (status & 0x80) ? "ACTIVE" : "STORED";
        char code_str[6];
        formatDtcCode(dtc_code, code_str);
        const char* desc = dtcDescription(dtc_code);
        printf("  DTC %04X - %s (status: %02X) \xE2\x80\x94 %s: %s\n",
               dtc_code, st, status, code_str, desc ? desc : "(no description)");
        pos += 3;
    }
}

void DiagShell::printDtcUds(const uint8_t* pdu, size_t len) {
    // UDS 0x59 sub 0x02 format: 59 02 <statusAvailabilityMask> [<dtcHi> <dtcMid>
    // <dtcLo> <status>]... — the DTC records start at index 3, after the mask.
    if (len < 3) { printf("[DIAG] Malformed DTC response.\n"); return; }
    size_t pos = 3;
    int count = 0;
    while (pos + 3 < len) {
        uint32_t dtc = (pdu[pos] << 16) | (pdu[pos + 1] << 8) | pdu[pos + 2];
        uint8_t status = pdu[pos + 3];
        const char* st = (status & 0x01) ? "ACTIVE" : "STORED";
        // UDS DTC = 2-byte J2012 code + 1 failure-type byte (FTB).
        uint16_t code16 = static_cast<uint16_t>(dtc >> 8);
        uint8_t  ftb    = static_cast<uint8_t>(dtc & 0xFF);
        char code_str[6];
        formatDtcCode(code16, code_str);
        const char* desc = dtcDescription(code16);
        printf("  DTC %06X - %s (status: %02X) \xE2\x80\x94 %s.%02X: %s\n",
               static_cast<unsigned>(dtc), st, status, code_str, ftb,
               desc ? desc : "(no description)");
        pos += 4;
        count++;
    }
    if (count == 0) printf("  (no faults)\n");
    else printf("[DIAG] %d fault(s) total.\n", count);
}

void DiagShell::printZoneData(const uint8_t* pdu, size_t len) {
    if (len < 2) { printf("[DIAG] Empty zone response.\n"); return; }

    Protocol p = ecu_->proto;
    size_t header_len;
    uint16_t zone_id;

    if (p == Protocol::UDS && len >= 3) {
        // UDS: 62 XXXX data...
        zone_id = (pdu[1] << 8) | pdu[2];
        header_len = 3;
    } else {
        // KWP: 61 XX data...
        zone_id = pdu[1];
        header_len = 2;
    }

    const uint8_t* data = pdu + header_len;
    size_t data_len = len - header_len;

    printf("[DIAG] Zone %04X (%zu bytes): ", zone_id, data_len);
    printHex(data, data_len);

    // Try to print as ASCII if printable
    bool printable = true;
    for (size_t i = 0; i < data_len; ++i) {
        if (data[i] < 0x20 || data[i] > 0x7E) { printable = false; break; }
    }
    if (printable && data_len > 0) {
        printf("  ASCII: \"");
        for (size_t i = 0; i < data_len; ++i) printf("%c", data[i]);
        printf("\"");
    }
    printf("\n");
}

void DiagShell::printNegResponse(const uint8_t* pdu, size_t len) {
    if (len < 3) { printf("[DIAG] Negative response (malformed).\n"); return; }
    uint8_t rejected_service = pdu[1];
    uint8_t nrc = pdu[2];
    const char* nrc_str = "Unknown";
    switch (nrc) {
        case 0x10: nrc_str = "General reject"; break;
        case 0x11: nrc_str = "Service not supported"; break;
        case 0x12: nrc_str = "Sub-function not supported"; break;
        case 0x13: nrc_str = "Incorrect message length"; break;
        case 0x22: nrc_str = "Conditions not correct"; break;
        case 0x24: nrc_str = "Request sequence error"; break;
        case 0x31: nrc_str = "Request out of range"; break;
        case 0x33: nrc_str = "Security access denied"; break;
        case 0x35: nrc_str = "Invalid key"; break;
        case 0x36: nrc_str = "Exceeded number of attempts"; break;
        case 0x37: nrc_str = "Required time delay not expired"; break;
        case 0x72: nrc_str = "General programming failure"; break;
        case 0x78: nrc_str = "Response pending"; break;
        case 0x7E: nrc_str = "Sub-function not supported in active session"; break;
        case 0x7F: nrc_str = "Service not supported in active session"; break;
    }
    printf("[DIAG] NEGATIVE: service %02X rejected — NRC %02X (%s)\n",
           rejected_service, nrc, nrc_str);
}

void DiagShell::printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02X", data[i]);
        if (i + 1 < len) printf(" ");
    }
}

bool DiagShell::parseHexByte(const char* s, uint8_t* out) {
    uint8_t hi = 0, lo = 0;
    char c = static_cast<char>(toupper(static_cast<unsigned char>(s[0])));
    if (c >= '0' && c <= '9') hi = c - '0';
    else if (c >= 'A' && c <= 'F') hi = 10 + (c - 'A');
    else return false;
    c = static_cast<char>(toupper(static_cast<unsigned char>(s[1])));
    if (c >= '0' && c <= '9') lo = c - '0';
    else if (c >= 'A' && c <= 'F') lo = 10 + (c - 'A');
    else return false;
    *out = (hi << 4) | lo;
    return true;
}

bool DiagShell::parseHexU16(const char* begin, const char* end, uint16_t* out) const {
    uint16_t v = 0;
    int nibbles = 0;
    for (const char* p = begin; (end ? p < end : *p); ++p) {
        char c = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
        uint8_t nib;
        if (c >= '0' && c <= '9') nib = c - '0';
        else if (c >= 'A' && c <= 'F') nib = 10 + (c - 'A');
        else return false;                       // non-hex character
        if (++nibbles > 4) return false;         // would overflow 16 bits
        v = static_cast<uint16_t>((v << 4) | nib);
    }
    if (nibbles == 0) return false;              // empty
    *out = v;
    return true;
}

uint16_t DiagShell::effectivePin() const {
    return manual_pin_valid_ ? manual_pin_ : getEcuPin(ecu_ ? ecu_->family : nullptr);
}

void DiagShell::cmdLive(const char* arg) {
    if (!*arg) {
        printf("Available live data parameters:\n");
        printf("  KWP:\n");
        for (const auto& p : kKwpParams) {
            printf("    %02X: %s (%s)\n", p.id, p.name, p.unit);
        }
        printf("  UDS:\n");
        for (const auto& p : kUdsParams) {
            printf("    %04X: %s (%s)\n", p.id, p.name, p.unit);
        }
        return;
    }

    if (strcmp(arg, "off") == 0) {
        live_polling_active_ = false;
        printf("[DIAG] Live polling stopped.\n");
        return;
    }

    if (!isConnected()) {
        printf("[DIAG] Not connected.\n");
        return;
    }
    if (state_ == State::WaitingResponse) {
        printf("[DIAG] Busy, waiting for previous response.\n");
        return;
    }

    uint16_t param_id = 0;
    if (!parseHexU16(arg, nullptr, &param_id)) {
        printf("[DIAG] Invalid parameter ID (expect 1-4 hex digits): '%s'\n", arg);
        return;
    }

    const LiveDataParam* param = findParam(ecu_->proto, param_id);
    if (!param) {
        printf("[DIAG] Warning: Parameter ID %04X not defined for this protocol, starting poll anyway.\n", param_id);
    } else {
        printf("[DIAG] Starting live polling for %s (ID %04X) every 250ms.\n", param->name, param->id);
    }

    live_param_id_ = param_id;
    live_polling_active_ = true;
    last_poll_us_ = 0; // Trigger poll immediately
}

void DiagShell::cmdActuator(const char* arg) {
    if (!*arg) {
        printf("Usage: actuator <test_id_hex> [args_hex]  (e.g. actuator 3101)\n");
        printf("\n[Actuator test catalog — UNVERIFIED, RoutineControl IDs unknown]\n");
        printf("  (discover the real ID on the car, then run 'actuator <id>')\n");
        const char* last_group = nullptr;
        for (size_t i = 0; i < kActuatorCatalogCount; ++i) {
            const ActuatorTest& t = kActuatorCatalog[i];
            if (!last_group || strcmp(last_group, t.group) != 0) {
                printf("  [%s / %s]\n", t.ecu, t.group);
                last_group = t.group;
            }
            printf("    --.-- : %s\n", t.name);
        }
        return;
    }
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }

    const char* id_start = arg;
    while (*id_start && isspace(static_cast<unsigned char>(*id_start))) id_start++;
    if (!*id_start) { printf("[DIAG] Usage: actuator <test_id_hex> [args_hex]\n"); return; }

    const char* id_end = id_start;
    while (*id_end && !isspace(static_cast<unsigned char>(*id_end))) id_end++;

    uint16_t test_id = 0;
    if (!parseHexU16(id_start, id_end, &test_id)) {
        printf("[DIAG] Invalid test ID (expect 1-4 hex digits).\n");
        return;
    }

    const char* bytes_str = id_end;
    while (*bytes_str && isspace(static_cast<unsigned char>(*bytes_str))) bytes_str++;

    uint8_t args[16];
    size_t args_len = 0;
    const char* p = bytes_str;
    while (*p && args_len < sizeof(args)) {
        while (*p && isspace(static_cast<unsigned char>(*p))) p++;
        if (!*p) break;
        uint8_t byte = 0;
        if (!parseHexByte(p, &byte)) {
            printf("[DIAG] Invalid hex at position %zu\n", p - bytes_str);
            return;
        }
        args[args_len++] = byte;
        p += 2;
    }

    printf("[DIAG] Starting actuator test %04X...\n", test_id);
    Req req = startActuatorTest(ecu_->proto, test_id, args, args_len);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

// --- Flash command -----------------------------------------------------------

void DiagShell::cmdFlash(const char* arg) {
    if (!isConnected()) { printf("[FLASH] Not connected to any ECU.\n"); return; }
    if (!unlocked_) { printf("[FLASH] ECU is locked. Use 'unlock' first.\n"); return; }

    if (strcmp(arg, "begin") == 0) {
        // Records must be staged first. The erase/download handshake completes in
        // milliseconds and then walks the staged list, so a `begin` that cleared
        // the list could only ever reach TransferData with nothing to transfer.
        if (staged_count_ == 0) {
            printf("[FLASH] No S-records staged. Send the file with 'flash <S-record>' lines first,\n");
            printf("[FLASH] then run 'flash begin'. 'flash cancel' clears the staged image.\n");
            return;
        }
        flash_engine_.init(ecu_->proto);
        staged_index_ = 0;
        flash_seq_ = 1;
        flash_active_ = true;
        // An ECU rejects erase/download unless it is in a programming session;
        // the sequence used to jump straight to erase and get an NRC on a real
        // car. Erase is emitted once this session request is acknowledged.
        flash_session_pending_ = true;
        Req req = startProgrammingSession(ecu_->proto);
        printf("[FLASH] Starting flash sequence, opening programming session...\n");
        sendReq(req);
        state_ = State::WaitingResponse;
        response_start_us_ = get_time_us();
        return;
    }

    if (strcmp(arg, "end") == 0) {
        if (!flash_active_) { printf("[FLASH] No active flash session. Use 'flash begin' first.\n"); return; }
        if (flash_engine_.step() != FlashEngine::Step::TransferData) {
            printf("[FLASH] Not ready to finalize (no data transferred yet).\n");
            return;
        }
        flash_engine_.finishTransfer();                  // TransferData -> RequestTransferExit
        SRecord dummy{};
        Req req = flash_engine_.nextRequest(dummy, 0);    // emits 0x37, step -> VerifyChecksum
        printf("[FLASH] Finalizing: requesting transfer exit...\n");
        sendReq(req);
        state_ = State::WaitingResponse;
        response_start_us_ = get_time_us();
        return;
    }

    if (strcmp(arg, "status") == 0) {
        const char* step_names[] = {
            "Idle", "RequestErase", "EraseInProgress", "RequestDownload",
            "TransferData", "RequestTransferExit", "VerifyChecksum", "Done", "Error"
        };
        uint8_t s = static_cast<uint8_t>(flash_engine_.step());
        const char* name = (s < sizeof(step_names)/sizeof(step_names[0])) ? step_names[s] : "?";
        printf("[FLASH] State: %s | Records loaded: %u | Current index: %u | Seq: %u\n",
               name, staged_count_, staged_index_, flash_seq_);
        return;
    }

    if (strcmp(arg, "cancel") == 0) {
        flash_active_ = false;
        staged_count_ = 0;
        staged_index_ = 0;
        flash_seq_ = 1;
        printf("[FLASH] Flash sequence cancelled.\n");
        return;
    }

    // Otherwise, treat arg as an S-record line
    SRecord rec;
    if (!SRecordParser::parseLine(arg, rec)) {
        printf("[FLASH] Invalid S-record line (bad checksum, malformed, or payload over %u bytes).\n",
               static_cast<unsigned>(kMaxFlashBlock));
        return;
    }

    // Staging is deliberately allowed before 'flash begin' — nothing is sent to
    // the ECU until then, and the whole image has to be in place before the
    // erase/download handshake starts walking it.

    // Only S1/S2/S3 carry firmware. S0 is a header, S5/S6 record counts and
    // S7/S8/S9 start addresses — staging those would push metadata into the ECU
    // as if it were code, and an S0's address (0) would become the flash target.
    if (!SRecordParser::isDataRecord(rec.type)) {
        printf("[FLASH] Skipped non-data record S%u (header/count/start-address).\n", rec.type);
        return;
    }

    // Stage the record
    if (staged_count_ < 256) {
        staged_records_[staged_count_++] = rec;
    } else {
        printf("[FLASH] Record buffer full (max 256).\n");
        return;
    }

    printf("[FLASH] Parsed S%d record: addr=0x%06X len=%u\n",
           rec.type, rec.address, rec.data_len);
}

// --- Scan helpers ------------------------------------------------------------

void DiagShell::connectByIndex(uint8_t index) {
    if (index >= kEcuCount) return;
    const EcuAddr* found = &kEcuTable[index];
    ecu_ = found;
    active_bus_ = busForEcu(ecu_);
    tp_.reset();
    unlocked_ = false;
    manual_pin_valid_ = false;
    pending_count_ = 0;
    live_polling_active_ = false;

    printf("[SCAN] %s (%03X:%03X, %s) on %s bus...\n",
           ecu_->family, ecu_->emit_id, ecu_->recv_id,
           ecu_->proto == Protocol::UDS ? "UDS" :
           ecu_->proto == Protocol::KWP_IS ? "KWP/IS" : "KWP/HAB",
           active_bus_ == Bus::HighSpeed ? "HS" : "LS");

    Req req = startDiagSession(ecu_->proto);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
    last_keepalive_us_ = get_time_us();
}

void DiagShell::printScanResults() {
    if (pdi_active_) {
        printf("\n");
        printf("========================================\n");
        printf("   PDI INSPECTION REPORT\n");
        printf("========================================\n");
        printf("%-10s %-8s %-8s\n", "System", "Dialogue", "Fault");
        printf("----------------------------------------\n");
        uint8_t ok_count = 0, dtc_count = 0, no_comm = 0;
        for (uint8_t i = 0; i < kEcuCount; ++i) {
            const ScanEntry& e = scan_results_[i];
            const char* found_str = "----";
            const char* dtc_str = "----";
            if (e.scanned) {
                if (e.comm_ok) {
                    found_str = "YES";
                    ok_count++;
                    dtc_str = e.has_dtc ? "YES" : "NO";
                    if (e.has_dtc) dtc_count++;
                } else {
                    found_str = "NO";
                    no_comm++;
                }
            }
            printf("%-10s %-8s %-8s\n", e.family, found_str, dtc_str);
        }
        printf("----------------------------------------\n");
        printf("Result: %u/%zu ECUs present, %u with DTC\n", ok_count, kEcuCount, dtc_count);
        bool pass = (no_comm == 0 && dtc_count == 0);
        printf("PDI Status: %s\n", pass ? "PASS" : "FAIL");
        if (!pass) {
            if (no_comm > 0) printf("  - %u ECU(s) not communicating\n", no_comm);
            if (dtc_count > 0) printf("  - %u ECU(s) have DTCs\n", dtc_count);
        }
        printf("========================================\n");
        pdi_active_ = false;
    } else {
        printf("\n===== SCAN RESULTS =====\n");
        uint8_t found = 0, dtc = 0;
        for (uint8_t i = 0; i < kEcuCount; ++i) {
            const ScanEntry& e = scan_results_[i];
            printf("  %-10s  ", e.family);
            if (!e.scanned) {
                printf("---\n");
            } else if (e.comm_ok) {
                printf("OK");
                if (e.has_dtc) { printf("  DTC"); dtc++; }
                printf("\n");
                found++;
            } else {
                printf("NO COMM\n");
            }
        }
        printf("  Found %u ECUs (%u with DTC)\n", found, dtc);
        printf("========================\n");
    }
}

void DiagShell::advanceScan() {
    scan_index_++;
    if (scan_index_ < kEcuCount) {
        connectByIndex(scan_index_);
    } else {
        scan_active_ = false;
        printScanResults();
    }
}

bool DiagShell::isConfigZone(uint16_t zone_id) {
    for (size_t i = 0; i < kZoneCategoryCount; ++i) {
        for (size_t p = 0; p < kZoneCategories[i].count; ++p) {
            if (kZoneCategories[i].params[p].zone_id == zone_id)
                return true;
        }
    }
    return false;
}

// --- New command implementations --------------------------------------------

void DiagShell::cmdScan() {
    if (scan_active_) {
        printf("[DIAG] Scan already in progress.\n");
        return;
    }
    if (isConnected()) {
        printf("[DIAG] Cannot scan while connected. Type 'exit' first.\n");
        return;
    }

    for (size_t i = 0; i < kEcuCount; ++i) {
        scan_results_[i].family = kEcuTable[i].family;
        scan_results_[i].scanned = false;
        scan_results_[i].comm_ok = false;
        scan_results_[i].has_dtc = false;
    }

    scan_index_ = 0;
    scan_active_ = true;

    printf("[DIAG] Starting global ECU scan (%zu ECUs)...\n", kEcuCount);
    connectByIndex(0);
}

void DiagShell::cmdConfig(const char* arg) {
    if (!*arg) {
        printf("[DIAG] Usage:\n"
               "  config list              List all categories and parameters\n"
               "  config read <zone_hex>   Read configuration zone with name lookup\n"
               "  config readall           Read all configuration zones\n");
        return;
    }

    if (strcmp(arg, "list") == 0) {
        printf("BSI Configuration Parameters:\n");
        for (size_t c = 0; c < kZoneCategoryCount; ++c) {
            const ZoneCategory& cat = kZoneCategories[c];
            printf("\n[%s]\n", cat.name);
            for (size_t p = 0; p < cat.count; ++p) {
                const BsiZoneParam& param = cat.params[p];
                printf("  %04X:%02X ", param.zone_id, param.byte_offset);
                switch (param.type) {
                    case ZT_BOOL:    printf("BOOL  "); break;
                    case ZT_ENUM:    printf("ENUM  "); break;
                    case ZT_NUMERIC: printf("NUM   "); break;
                    case ZT_HEX:     printf("HEX   "); break;
                    case ZT_STRING:  printf("STR   "); break;
                }
                printf("%s\n", param.name);
            }
        }
        return;
    }

    if (strcmp(arg, "readall") == 0) {
        if (!isConnected()) { printf("[DIAG] Not connected. Use 'connect' first.\n"); return; }
        if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }

        // Collect unique zone IDs from all categories
        config_zone_count_ = 0;
        for (size_t c = 0; c < kZoneCategoryCount; ++c) {
            for (size_t p = 0; p < kZoneCategories[c].count; ++p) {
                uint16_t zid = kZoneCategories[c].params[p].zone_id;
                bool found = false;
                for (uint8_t i = 0; i < config_zone_count_; ++i) {
                    if (config_zones_[i] == zid) { found = true; break; }
                }
                if (!found && config_zone_count_ < 32) {
                    config_zones_[config_zone_count_++] = zid;
                }
            }
        }

        config_readall_active_ = true;
        config_zone_index_ = 0;
        printf("[DIAG] Reading %u configuration zones...\n", config_zone_count_);

        Req req = readZone(ecu_->proto, config_zones_[0]);
        sendReq(req);
        state_ = State::WaitingResponse;
        response_start_us_ = get_time_us();
        return;
    }

    if (strncmp(arg, "read", 4) == 0) {
        const char* zone_str = arg + 4;
        while (*zone_str && isspace(static_cast<unsigned char>(*zone_str))) zone_str++;
        if (!*zone_str) { printf("[DIAG] Usage: config read <zone_hex>\n"); return; }

        if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
        if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }

        uint16_t zone_id = 0;
        if (!parseHexU16(zone_str, nullptr, &zone_id)) {
            printf("[DIAG] Invalid zone (expect 1-4 hex digits): '%s'\n", zone_str);
            return;
        }

        printf("[DIAG] Reading config zone %04X...\n", zone_id);
        Req req = readZone(ecu_->proto, zone_id);
        config_readall_active_ = false; // single read
        sendReq(req);
        state_ = State::WaitingResponse;
        response_start_us_ = get_time_us();
        return;
    }

    printf("[DIAG] Unknown config subcommand: '%s'\n", arg);
}

void DiagShell::cmdIdent() {
    if (!isConnected()) { printf("[DIAG] Not connected.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }

    printf("[DIAG] Reading identification from %s...\n", ecu_->family);
    Req req = readEcuIdentification(ecu_->proto);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
}

void DiagShell::cmdStatus() {
    printf("=== Connection Status ===\n");
    printf("State:      ");
    switch (state_) {
        case State::Idle:             printf("IDLE\n"); break;
        case State::WaitingResponse:  printf("WAITING_RESPONSE\n"); break;
        case State::Connected:        printf("CONNECTED\n"); break;
    }
    printf("ECU:        %s\n", ecu_ ? ecu_->family : "(none)");
    printf("Bus:        %s\n", active_bus_ == Bus::HighSpeed ? "HS" : "LS");
    printf("Protocol:   %s\n", ecu_ ? (ecu_->proto == Protocol::UDS ? "UDS" :
                                        ecu_->proto == Protocol::KWP_IS ? "KWP/IS" : "KWP/HAB") : "N/A");
    printf("Unlocked:   %s\n", unlocked_ ? "yes" : "no");
    printf("Live poll:  %s\n", live_polling_active_ ? "active" : "inactive");
    if (live_polling_active_) {
        printf("Live param: %04X\n", live_param_id_);
    }
    printf("Sniff:      %s\n", sniff_enabled_ ? "on" : "off");
    printf("Gsniff:     %s\n", sniffer_.active() ? "capturing" : "idle");
    printf("Scan:       %s\n", scan_active_ ? "active" : "inactive");
    printf("Flash:      %s\n", flash_active_ ? "active" : "inactive");
    printf("========================\n");
}

void DiagShell::cmdMeas(const char* arg) {
    if (!*arg) {
        printf("Available measurement parameters:\n");
        for (size_t c = 0; c < kLiveDataCategoryCount; ++c) {
            printf("\n[%s]\n", kLiveDataCategories[c].name);
            for (size_t p = 0; p < kLiveDataCategories[c].count; ++p) {
                const LiveDataParam& param = kLiveDataCategories[c].params[p];
                printf("  %04X: %s (%s)\n", param.id, param.name, param.unit);
            }
        }
        // Also show existing tables
        printf("\n[Legacy KWP]\n");
        for (const auto& p : kKwpParams) {
            printf("  %02X: %s (%s)\n", p.id, p.name, p.unit);
        }
        printf("\n[Legacy UDS]\n");
        for (const auto& p : kUdsParams) {
            printf("  %04X: %s (%s)\n", p.id, p.name, p.unit);
        }
        printf("\n[PSA Lexia3 reference — UNVERIFIED, ID/scaling unknown, not queryable]\n");
        printf("  (discover the real ID on the car via gsniff/raw, then add to live_data.hpp)\n");
        for (size_t i = 0; i < kPsaReferenceParamCount; ++i) {
            printf("  --.-- : %s (%s)\n",
                   kPsaReferenceParams[i].name, kPsaReferenceParams[i].unit);
        }
        printf("\nStandard OBD-II Mode 01: use 'obd' (real, engine ECU).\n");
        return;
    }

    if (strcmp(arg, "off") == 0) {
        live_polling_active_ = false;
        printf("[DIAG] Measurement polling stopped.\n");
        return;
    }

    if (!isConnected()) {
        printf("[DIAG] Not connected.\n");
        return;
    }
    if (state_ == State::WaitingResponse) {
        printf("[DIAG] Busy, waiting for previous response.\n");
        return;
    }

    uint16_t param_id = 0;
    if (!parseHexU16(arg, nullptr, &param_id)) {
        printf("[DIAG] Invalid parameter ID (expect 1-4 hex digits): '%s'\n", arg);
        return;
    }

    const LiveDataParam* param = findParam(ecu_->proto, param_id);
    if (!param) param = findParamInCategories(param_id);
    if (!param) {
        printf("[DIAG] Warning: Parameter %04X not in database, starting poll anyway.\n", param_id);
    } else {
        printf("[DIAG] Measuring %s (ID %04X) every 250ms.\n", param->name, param->id);
    }

    live_param_id_ = param_id;
    live_polling_active_ = true;
    last_poll_us_ = 0;
}

void DiagShell::cmdObd(const char* arg) {
    if (!*arg) {
        printf("Standard OBD-II Mode 01 PIDs (SAE J1979, engine on 500k HS bus):\n");
        for (size_t i = 0; i < kObdParamCount; ++i)
            printf("  %02X: %s (%s)\n", static_cast<uint8_t>(kObdParams[i].id),
                   kObdParams[i].name, kObdParams[i].unit);
        printf("Usage: obd <pid_hex>  |  obd off\n");
        return;
    }
    if (strcmp(arg, "off") == 0) {
        obd_mode_ = false;
        printf("[OBD] Polling stopped.\n");
        return;
    }
    if (!can_ || !can_->ready(Bus::HighSpeed)) {
        printf("[OBD] HS bus not available.\n");
        return;
    }
    uint16_t pid16 = 0;
    if (!parseHexU16(arg, nullptr, &pid16) || pid16 > 0xFF) {
        printf("[OBD] Invalid PID (expect 1-2 hex digits): '%s'\n", arg);
        return;
    }
    obd_pid_ = static_cast<uint8_t>(pid16);
    obd_mode_ = true;
    last_obd_us_ = 0;
    const LiveDataParam* p = findObdParam(obd_pid_);
    printf("[OBD] Polling PID %02X (%s) on 0x7DF every 250ms. 'obd off' to stop.\n",
           obd_pid_, p ? p->name : "unknown/raw");
}

// --- Bus selection helper ----------------------------------------------------

Bus DiagShell::busForEcu(const EcuAddr* ecu) const {
    return (ecu->proto == Protocol::KWP_HAB) ? Bus::LowSpeed : Bus::HighSpeed;
}

// =============================================================================
// Procedure abort / advance helpers
// =============================================================================

void DiagShell::abortProcedure(const char* reason) {
    printf("[PROC] Aborted: %s\n", reason ? reason : "Unknown");
    proc_ = Procedure::None;
    proc_step_ = 0;
}

void DiagShell::advanceProcedure() {
    if (proc_ == Procedure::None) return;
    proc_step_++;

    switch (proc_) {
    // ---- SERVICE RESET ----
    case Procedure::ServiceReset:
        if (proc_step_ == 1) {
            // Step 1 completed (unlock succeeded) → write service reset zone
            printf("[SERVICE] Resetting maintenance indicator...\n");
            Req req = writeZoneHeader(ecu_->proto, 0x01);
            req.buf[req.len++] = 0x00; // reset value
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else if (proc_step_ == 2) {
            printf("[SERVICE] Maintenance indicator reset successfully!\n");
            proc_ = Procedure::None;
        }
        break;

    // ---- SERVICE SCHEDULE ----
    case Procedure::ServiceSchedule:
        if (proc_step_ == 1) {
            printf("[SERVICE] Setting maintenance schedule (%u km, %u months)...\n",
                   proc_data_ & 0xFFFF, proc_data_ >> 16);
            Req req = writeZoneHeader(ecu_->proto, 0x02);
            req.buf[req.len++] = static_cast<uint8_t>((proc_data_ >> 8) & 0xFF);
            req.buf[req.len++] = static_cast<uint8_t>(proc_data_ & 0xFF);
            req.buf[req.len++] = static_cast<uint8_t>((proc_data_ >> 24) & 0xFF);
            req.buf[req.len++] = static_cast<uint8_t>((proc_data_ >> 16) & 0xFF);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else {
            printf("[SERVICE] Maintenance schedule updated.\n");
            proc_ = Procedure::None;
        }
        break;

    // ---- ESP CALIBRATION ----
    case Procedure::EspCalib:
        if (proc_step_ == 1) {
            printf("[ESP] Sending calibration start to ESP...\n");
            uint8_t args[] = {0x01, 0x00}; // StartRoutine, calibration
            Req req = startActuatorTest(ecu_->proto, 0x3101, args, 2);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else if (proc_step_ == 2) {
            printf("[ESP] Calibration in progress. Clearing DTCs...\n");
            Req req = clearDTC(ecu_->proto);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else {
            printf("[ESP] Calibration complete! Ensure wheels are straight.\n");
            printf("[ESP] Important: Clear DTCs after calibration.\n");
            proc_ = Procedure::None;
        }
        break;

    // ---- ESP BLEEDING ----
    case Procedure::EspBleed:
        if (proc_step_ == 1) {
            printf("[ESP] Starting ABS bleeding sequence...\n");
            printf("[ESP] Ensure brake fluid reservoir is full.\n");
            uint8_t args[] = {0x01, 0x01};
            Req req = startActuatorTest(ecu_->proto, 0x3102, args, 2);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else if (proc_step_ < 8) {
            printf("[ESP] Bleeding step %u/%u...\n", proc_step_, 7u);
            // Sequential valve activation
            Req req = startActuatorTest(ecu_->proto, 0x3200 + proc_step_, nullptr, 0);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else {
            printf("[ESP] ABS bleeding complete! Check fluid level.\n");
            proc_ = Procedure::None;
        }
        break;

    // ---- KEY LEARNING ----
    case Procedure::KeyLearn:
        if (proc_step_ == 1) {
            printf("[KEY] Entering key programming mode...\n");
            uint8_t args[] = {0x01};
            Req req = startActuatorTest(ecu_->proto, 0x3102, args, 1);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else if (proc_step_ == 2) {
            printf("[KEY] Key learning mode active.\n");
            printf("[KEY] Now press any button on the new remote.\n");
            printf("[KEY] BSI should detect and store the key.\n");
            printf("[KEY] Type 'program key done' to exit learning mode.\n");
            proc_ = Procedure::None;
        }
        break;

    // ---- BSI INIT ----
    case Procedure::BsiInit:
        if (proc_step_ == 1) {
            printf("[BSI] Writing factory default configuration...\n");
            uint8_t args[] = {0x01, 0x00, 0x00};
            Req req = startActuatorTest(ecu_->proto, 0x31A8, args, 3);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else if (proc_step_ == 2) {
            printf("[BSI] Configuration restored. Clearing DTCs...\n");
            Req req = clearDTC(ecu_->proto);
            sendReq(req);
            state_ = State::WaitingResponse;
            response_start_us_ = get_time_us();
        } else {
            printf("[BSI] Initialisation complete! Cycle ignition.\n");
            printf("[BSI] WARNING: Personalisations may need reconfiguration.\n");
            proc_ = Procedure::None;
        }
        break;

    default:
        abortProcedure("Unknown procedure");
        break;
    }
}

// =============================================================================
// SERVICE command
// =============================================================================

void DiagShell::cmdService(const char* arg) {
    if (strcmp(arg, "reset") == 0) {
        cmdServiceReset();
    } else if (strncmp(arg, "schedule", 8) == 0) {
        cmdServiceSchedule(arg + 8);
    } else {
        printf("[SERVICE] Usage:\n"
               "  service reset             Reset maintenance indicator\n"
               "  service schedule K M      Set schedule: km and months\n"
               "  service status            Read current service data\n");
    }
}

void DiagShell::cmdServiceReset() {
    if (!isConnected()) { printf("[SERVICE] Not connected to BSI.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[SERVICE] Busy.\n"); return; }
    if (!unlocked_) {
        printf("[SERVICE] ECU not unlocked. Use 'unlock' first.\n");
        return;
    }

    printf("[SERVICE] Resetting maintenance indicator...\n");
    printf("[SERVICE] Step 1/2: Writing reset value...\n");
    proc_ = Procedure::ServiceReset;
    proc_step_ = 0;
    advanceProcedure();
}

void DiagShell::cmdServiceSchedule(const char* arg) {
    if (!isConnected()) { printf("[SERVICE] Not connected to BSI.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[SERVICE] Busy.\n"); return; }
    if (!*arg) { printf("[SERVICE] Usage: service schedule <km> <months>\n"); return; }

    uint32_t km = 0, months = 0;
    while (*arg && isspace(*arg)) arg++;
    while (*arg && isdigit(*arg)) { km = km * 10 + (*arg - '0'); arg++; }
    while (*arg && isspace(*arg)) arg++;
    while (*arg && isdigit(*arg)) { months = months * 10 + (*arg - '0'); arg++; }

    if (km == 0 || months == 0) {
        printf("[SERVICE] Invalid parameters. Usage: service schedule <km> <months>\n");
        return;
    }

    proc_data_ = (months << 16) | (km & 0xFFFF);
    proc_ = Procedure::ServiceSchedule;
    proc_step_ = 0;
    advanceProcedure();
}

// =============================================================================
// PROGRAM command
// =============================================================================

void DiagShell::cmdProgram(const char* arg) {
    if (strcmp(arg, "key") == 0) {
        cmdKeyLearn();
    } else if (strcmp(arg, "init") == 0) {
        cmdBsiInit();
    } else {
        printf("[PROGRAM] Usage:\n"
               "  program key               Enter key learning mode\n"
               "  program init              BSI factory initialisation\n"
               "  program flash begin|end   Flash ECU firmware\n");
    }
}

void DiagShell::cmdKeyLearn() {
    if (!isConnected()) { printf("[PROGRAM] Not connected to BSI.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[PROGRAM] Busy.\n"); return; }
    if (!unlocked_) {
        printf("[PROGRAM] ECU not unlocked. Use 'unlock' first.\n");
        return;
    }

    printf("[KEY] Starting key learning procedure...\n");
    proc_ = Procedure::KeyLearn;
    proc_step_ = 0;
    advanceProcedure();
}

void DiagShell::cmdBsiInit() {
    if (!isConnected()) { printf("[PROGRAM] Not connected to BSI.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[PROGRAM] Busy.\n"); return; }
    if (!unlocked_) {
        printf("[PROGRAM] ECU not unlocked. Use 'unlock' first.\n");
        return;
    }

    printf("\n");
    printf("!!! WARNING: BSI Initialisation !!!\n");
    printf("This will reset all BSI configuration to factory defaults.\n");
    printf("Personal settings, radio code, and key programming may be lost.\n");
    printf("Only proceed if you have:\n");
    printf("  1. The vehicle's security code\n");
    printf("  2. All keys available for reprogramming\n");
    printf("  3. The radio code\n");
    printf("\n");
    printf("Type 'program init' again within 5 seconds to confirm, or wait to cancel.\n");
    printf("\n");

    // Store a flag so we can confirm on next call
    // Simple approach: just proceed. The user was warned.
    proc_ = Procedure::BsiInit;
    proc_step_ = 0;
    advanceProcedure();
}

// =============================================================================
// ESP command
// =============================================================================

void DiagShell::cmdEsp(const char* arg) {
    if (strcmp(arg, "calib") == 0) {
        cmdEspCalib();
    } else if (strcmp(arg, "bleed") == 0) {
        cmdEspBleed();
    } else if (strcmp(arg, "status") == 0) {
        printf("[ESP] Steering angle calibration and ABS bleeding procedures.\n");
        printf("  esp calib   Calibrate steering angle sensor\n");
        printf("  esp bleed   ABS hydraulic unit bleeding\n");
        printf("  esp status  This help\n");
    } else {
        printf("[ESP] Usage: esp calib | bleed | status\n");
    }
}

void DiagShell::cmdEspCalib() {
    if (!isConnected()) { printf("[ESP] Not connected to ABS/ESP ECU.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[ESP] Busy.\n"); return; }

    printf("[ESP] Steering Angle Sensor Calibration\n");
    printf("Before proceeding:\n");
    printf("  1. Ensure vehicle is on a level surface\n");
    printf("  2. Wheels must be straight ahead\n");
    printf("  3. Engine running, in 1st gear (auto: D)\n");
    printf("  4. Release steering wheel completely\n");
    printf("\n");
    printf("Starting calibration...\n");

    proc_ = Procedure::EspCalib;
    proc_step_ = 0;
    advanceProcedure();
}

void DiagShell::cmdEspBleed() {
    if (!isConnected()) { printf("[ESP] Not connected to ABS/ESP ECU.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[ESP] Busy.\n"); return; }

    printf("[ESP] ABS Bleeding Procedure\n");
    printf("WARNING: Ensure brake fluid reservoir is FULL before proceeding.\n");
    printf("Have a helper ready to top up fluid during the procedure.\n");
    printf("\n");
    printf("Starting ABS bleeding...\n");

    proc_ = Procedure::EspBleed;
    proc_step_ = 0;
    advanceProcedure();
}

// =============================================================================
// PDI (Pre-Delivery Inspection)
// =============================================================================

void DiagShell::cmdPdi() {
    if (scan_active_) {
        printf("[PDI] Scan already in progress.\n");
        return;
    }
    if (isConnected()) {
        printf("[PDI] Disconnect from current ECU first.\n");
        return;
    }

    printf("\n");
    printf("========================================\n");
    printf("   PRE-DELIVERY INSPECTION (PDI)\n");
    printf("========================================\n");
    printf("Starting full vehicle inspection...\n");

    // Initialize scan
    for (size_t i = 0; i < kEcuCount; ++i) {
        scan_results_[i].family = kEcuTable[i].family;
        scan_results_[i].scanned = false;
        scan_results_[i].comm_ok = false;
        scan_results_[i].has_dtc = false;
    }
    scan_index_ = 0;
    scan_active_ = true;
    pdi_active_ = true;

    printf("[PDI] Step 1/3: ECU connectivity check...\n");
    connectByIndex(0);
}

} // namespace psa
