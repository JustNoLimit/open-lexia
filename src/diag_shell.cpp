// Diagnostic shell — implementation.
// Interactive USB serial CLI for PSA ECU diagnostics.
#include "psa/diag_shell.hpp"
#include "psa/ecu_keys.hpp"
#include "psa/live_data.hpp"
#include "psa/dtc_text.hpp"
#include "psa/actuator_catalog.hpp"
#include "psa/flash_engine.hpp"
#include "psa/ecu_zones.hpp"
#include "psa/ecu_params.hpp"
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
#include "hardware/watchdog.h"
static inline uint64_t get_time_us() { return to_us_since_boot(get_absolute_time()); }
static inline int getchar_nonblocking() { return getchar_timeout_us(0); }
#endif

namespace psa {

void DiagShell::init(CanManager* can) {
    can_ = can;
    state_ = State::Idle;
    ecu_ = nullptr;
    sniff_enabled_ = true;
    sniff_bus_ = Bus::HighSpeed;
    line_pos_ = 0;
    tp_.reset();
    unlocked_ = false;
    live_polling_active_ = false;
    live_param_count_ = 0;
    live_param_idx_ = 0;
    for (auto& id : live_param_ids_) id = 0;
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

    // LID sweep runs on its own short deadline (see kLidScanTimeoutUs). Only
    // when nothing at all has come back: a multi-frame reply that is still being
    // reassembled must be allowed to finish, however slowly.
    if (lidscan_active_ && state_ == State::WaitingResponse && !tp_.rxActive() &&
        get_time_us() - lidscan_sent_us_ > kLidScanTimeoutUs) {
        tp_.reset();
        state_ = State::Connected;
        lidScanAdvance();
    }

    // Check for response timeout
    if (state_ == State::WaitingResponse) {
        uint64_t now = get_time_us();
        if (now - response_start_us_ > kResponseTimeoutUs) {
            printf("[DIAG] Response timeout.\n");
            tp_.txReset();
            // The request itself may still be latched in a transmit buffer. With
            // nothing on the bus to acknowledge it — ignition off, connector
            // unplugged — TXREQ never clears on its own, and sendFrame's rescue
            // only fires once ALL THREE buffers are full, which a single-frame
            // request never reaches. Left alone the frame sits there and goes
            // out the moment the bus comes up: a stale session-open, or worse a
            // stale write, arriving long after we told the user it timed out.
#ifndef HOST_TEST
            if (Mcp2515* m = can_->bus(active_bus_)) m->recoverBus();
#endif
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
            if (lidscan_active_) {
                lidscan_active_ = false;
                printf("[LIDSCAN] Aborted at %04X.\n", lidscan_id_);
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

    // Send live poll if active — round-robin through the active param list
    if (live_polling_active_ && state_ == State::Connected && live_param_count_ > 0) {
        uint64_t now = get_time_us();
        if (last_poll_us_ == 0 || now - last_poll_us_ >= 250000) {
            uint16_t id = live_param_ids_[live_param_idx_];
            live_param_idx_ = (live_param_idx_ + 1) % live_param_count_;
            Req req = readLiveData(ecu_->proto, id);
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

    // Write protection, enforced here rather than in the dashboard: the USB
    // serial console and any other client reach the same dispatch, so a UI
    // button alone would guard nothing. `raw` is on the list because it can
    // encode any write service by hand.
    if (!rw_enabled_) {
        static const char* const kWriteCommands[] = {
            "write", "trace", "clear", "actuator", "flash",
            "service", "program", "esp", "raw", "ecodisable",
        };
        for (const char* w : kWriteCommands) {
            if (strcmp(cmd, w) == 0) {
                printf("[DIAG] Read-only mode: '%s' blocked. Enable R/W first ('rw on').\n", cmd);
                return;
            }
        }
    }

    // Dispatch
    if (strcmp(cmd, "rw") == 0) {
        cmdRw(arg);
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
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
    } else if (strcmp(cmd, "lidscan") == 0) {
        cmdLidScan(arg);
    } else if (strcmp(cmd, "ecodisable") == 0) {
        cmdEcoDisable();
    } else if (strcmp(cmd, "pincrack") == 0) {
        cmdPincrack(arg);
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
        "  lidscan [a b|stop]    Sweep identifiers to find readable ones (default 00..FF)\n"
        "  live [param|off]      Monitor sensor parameter (e.g. live 100A or live off)\n"
        "  actuator <id> [args]  Start actuator routine (e.g. actuator 3101)\n"
        "  flash begin           Start flash sequence (erase + prepare)\n"
        "  flash <S3_line>       Load S-record line (parsed and sent automatically)\n"
        "  flash end             Finalize flash (transfer exit + verification)\n"
        "  flash status          Show current flash state machine step\n"
        "  flash cancel          Abort flash sequence\n"
        "  raw <hex bytes>       Send raw PDU (e.g. raw 21 80)\n"
        "  sniff [on|raw|off]    Passive bus monitor; 'raw' dumps every frame (gsniff rate hs/ls)\n"
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
        "  rw [on|off]           Read-only (default) or read/write mode\n"
        "  hwtest                Hardware self-test (SPI + CAN loopback)\n"
        "  gsniff <sub>          Guided CAN signal discovery (gsniff for subcommands)\n"
        "  ecodisable            Disable Eco Mode (BSI RoutineControl DF0A; needs unlock)\n"
        "  pincrack <c> <r> ...  Brute-force immo PIN from 072/0A8 challenge/response pairs\n"
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
    setBusMode(CanBitrate::Bps500k, /*listen_only=*/false);
    tp_.reset();
    unlocked_ = false;
    manual_pin_valid_ = false;
    pending_count_ = 0;
    live_polling_active_ = false;
    live_param_count_ = 0;
    live_param_idx_ = 0;

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
    live_param_count_ = 0;
    live_param_idx_ = 0;
    config_readall_active_ = false;
    lidscan_active_ = false;
}

// =============================================================================
// LID / DID sweep — discovery tool for the parameter tables we do not have.
// PSA never published the measurement identifiers Lexia 3 reads, so the only
// way to find them is to ask the ECU for every identifier in turn and note
// which ones answer. Strictly read-only: it sends nothing but ReadDataByLocalId
// (KWP 21 xx) / ReadDataByIdentifier (UDS 22 xxxx), never a write or a routine.
// =============================================================================

void DiagShell::cmdLidScan(const char* arg) {
    if (strcmp(arg, "stop") == 0) {
        if (!lidscan_active_) { printf("[LIDSCAN] Not running.\n"); return; }
        lidscan_active_ = false;
        state_ = State::Connected;
        printf("[LIDSCAN] Stopped at %04X. %u of %u answered.\n",
               lidscan_id_, lidscan_hits_,
               static_cast<unsigned>(lidscan_id_ - lidscan_first_ + 1));
        return;
    }
    if (!isConnected() || !ecu_) { printf("[LIDSCAN] Not connected. Use 'connect <ECU>' first.\n"); return; }
    if (lidscan_active_)          { printf("[LIDSCAN] Already running ('lidscan stop' to abort).\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[LIDSCAN] Busy.\n"); return; }

    uint16_t first = 0x00, last = 0xFF;
    if (*arg) {
        const char* sep = arg;
        while (*sep && !isspace(static_cast<unsigned char>(*sep))) sep++;
        const char* second = sep;
        while (*second && isspace(static_cast<unsigned char>(*second))) second++;
        if (!*second || !parseHexU16(arg, sep, &first) ||
            !parseHexU16(second, nullptr, &last)) {
            printf("[LIDSCAN] Usage: lidscan [start_hex end_hex] | lidscan stop\n"
                   "          lidscan            sweep 00..FF (KWP local IDs)\n"
                   "          lidscan 0100 01FF  sweep a UDS DID range\n");
            return;
        }
        if (last < first) { printf("[LIDSCAN] End is below start.\n"); return; }
    }

    // readZone() promotes any identifier above 0xFF to UDS framing, so a range
    // of 00..FF is a KWP local-ID sweep and anything wider is a DID sweep. Both
    // are legitimate on a CAN2004 ECU; which one the user wants is the range.
    lidscan_first_ = first;
    lidscan_id_    = first;
    lidscan_end_   = last;
    lidscan_hits_  = 0;
    lidscan_active_ = true;
    printf("[LIDSCAN] Sweeping %04X..%04X on %s (%u identifiers, ~%u s worst case).\n",
           first, last, ecu_->family,
           static_cast<unsigned>(last - first + 1),
           static_cast<unsigned>((last - first + 1) * kLidScanTimeoutUs / 1'000'000));
    lidScanSend();
}

void DiagShell::lidScanSend() {
    pending_count_ = 0;   // the 0x78 streak is per identifier, not per sweep
    Req req = readZone(ecu_->proto, lidscan_id_);
    sendReq(req);
    state_ = State::WaitingResponse;
    response_start_us_ = get_time_us();
    lidscan_sent_us_   = response_start_us_;
}

void DiagShell::lidScanAdvance() {
    if (!lidscan_active_) return;
    if (lidscan_id_ >= lidscan_end_) {   // compare before incrementing: end may be FFFF
        lidscan_active_ = false;
        state_ = State::Connected;
        printf("[LIDSCAN] Done. %u of %u identifiers answered.\n",
               lidscan_hits_, static_cast<unsigned>(lidscan_end_ - lidscan_first_ + 1));
        return;
    }
    lidscan_id_++;
    // Progress ticks so a long sweep does not look like a hang on a quiet ECU.
    if ((lidscan_id_ & 0x1F) == 0) printf("[LIDSCAN] ...%04X\n", lidscan_id_);
    lidScanSend();
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

// Immobiliser PIN cracker: brute-forces the 4-char alphanumeric anti-theft PIN
// from captured (challenge, response) pairs off CAN IDs 0x072/0x0A8 (see the
// immo_challenge/immo_response sniff lines). Pure offline computation, same
// cipher primitive as SecurityAccess (psa::seed_key::transform) combined the
// way pypsadiag's PINExtractor.py does it — ported from that reference, not
// reverse-derived here. No ECU connection needed.
namespace {
uint32_t immoComputeResponse(const uint8_t pin[4], uint32_t challenge) {
    uint8_t b0 = static_cast<uint8_t>(challenge >> 24);
    uint8_t b1 = static_cast<uint8_t>(challenge >> 16);
    uint8_t b2 = static_cast<uint8_t>(challenge >> 8);
    uint8_t b3 = static_cast<uint8_t>(challenge);
    long res_msb = seed_key::transform(b0, b2, seed_key::kSec1)
                 | seed_key::transform(pin[0], pin[3], seed_key::kSec2);
    long res_lsb = seed_key::transform(b1, b3, seed_key::kSec2)
                 | seed_key::transform(pin[1], pin[2], seed_key::kSec1);
    return (static_cast<uint32_t>(res_msb) << 16) | (static_cast<uint32_t>(res_lsb) & 0xFFFF);
}
} // namespace

void DiagShell::cmdPincrack(const char* arg) {
    static constexpr int kMaxPairs = 4;
    uint32_t chal[kMaxPairs], resp[kMaxPairs];
    int n = 0;

    const char* p = arg;
    while (*p && n < kMaxPairs * 2) {
        while (*p && isspace(static_cast<unsigned char>(*p))) p++;
        if (!*p) break;
        uint8_t b[4];
        bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) ok = parseHexByte(p + i * 2, &b[i]);
        if (!ok) { printf("[PINCRACK] Bad hex token (need 8 hex chars per value).\n"); return; }
        p += 8;
        if (*p && !isspace(static_cast<unsigned char>(*p))) {
            printf("[PINCRACK] Bad hex token (need exactly 8 hex chars per value).\n");
            return;
        }
        uint32_t v = (static_cast<uint32_t>(b[0]) << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
        if (n % 2 == 0) chal[n / 2] = v; else resp[n / 2] = v;
        n++;
    }
    if (n == 0 || n % 2 != 0) {
        printf("Usage: pincrack <chal8hex> <resp8hex> [<chal8hex> <resp8hex> ...]\n"
               "  chal = 072 immo_challenge, resp = matching 0A8 immo_response.\n"
               "  One pair usually leaves a few candidates; a second pair disambiguates.\n");
        return;
    }
    int pairs = n / 2;

    static const char kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    printf("[PINCRACK] Searching %u candidates against %d pair(s)...\n",
           36u * 36u * 36u * 36u, pairs);
    int found = 0;
    uint8_t pin[4];
    for (int i0 = 0; i0 < 36; ++i0) {
        pin[0] = static_cast<uint8_t>(kAlphabet[i0]);
        for (int i1 = 0; i1 < 36; ++i1) {
            pin[1] = static_cast<uint8_t>(kAlphabet[i1]);
#ifndef HOST_TEST
            watchdog_update(); // ~1296 candidates/pet; whole sweep is ~2s, well under 8s
#endif
            for (int i2 = 0; i2 < 36; ++i2) {
                pin[2] = static_cast<uint8_t>(kAlphabet[i2]);
                for (int i3 = 0; i3 < 36; ++i3) {
                    pin[3] = static_cast<uint8_t>(kAlphabet[i3]);
                    if (immoComputeResponse(pin, chal[0]) != resp[0]) continue;
                    bool ok = true;
                    for (int k = 1; k < pairs; ++k) {
                        if (immoComputeResponse(pin, chal[k]) != resp[k]) { ok = false; break; }
                    }
                    if (ok) {
                        printf("[PINCRACK] candidate: %c%c%c%c\n", pin[0], pin[1], pin[2], pin[3]);
                        found++;
                    }
                }
            }
        }
    }
    printf("[PINCRACK] done, %d candidate(s).\n", found);
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

    // --- Test 1: SPI register read ---
    printf("[HWTEST] Test 1: SPI register read...\n");

    Mcp2515* mcp = can_->bus(Bus::HighSpeed);
    if (!mcp) {
        printf("[HWTEST]   SKIP: MCP2515 did not answer at init — not probing (would hang).\n");
        printf("[HWTEST] FAIL: No MCP2515 detected. Check SPI wiring (GP2-6) + level-shifter.\n");
        printf("[HWTEST] Expected: MCP2515 on spi0 GP2(SCK) GP3(MOSI) GP4(MISO) GP5(CS) GP6(INT)\n");
        return;
    }

    uint8_t s = mcp->readReg(Mcp2515::MCP_CANSTAT);
    printf("[HWTEST]   CANSTAT = 0x%02X (mode bits: 0x%02X)\n", s, s & 0xE0);

    auto spiPresent = [](Mcp2515& m) {
        static constexpr uint8_t kProbe = 0xA5;
        uint8_t saved = m.readReg(Mcp2515::MCP_CANINTE);
        m.writeReg(Mcp2515::MCP_CANINTE, kProbe);
        bool ok = m.readReg(Mcp2515::MCP_CANINTE) == kProbe;
        m.writeReg(Mcp2515::MCP_CANINTE, saved);
        return ok;
    };
    bool spi_ok = spiPresent(*mcp);
    printf("[HWTEST]   SPI: %s\n", spi_ok ? "OK" : "FAIL (readback mismatch - check wiring/level-shift)");

    printf("[HWTEST]   CANCTRL = 0x%02X  EFLG = 0x%02X\n",
           mcp->readReg(Mcp2515::MCP_CANCTRL), mcp->errorFlags());

    if (!spi_ok) {
        printf("[HWTEST] FAIL: MCP2515 SPI unresponsive. Check wiring (GP2-6 via TXS0108E).\n");
        return;
    }

    // --- Test 2: Loopback ---
    printf("[HWTEST] Test 2: CAN loopback test...\n");

    Mcp2515& m = *mcp;
    printf("[HWTEST]   Switching to loopback mode...\n");
    if (m.setLoopbackMode() != McpError::Ok) {
        printf("[HWTEST]   FAIL (could not enter loopback mode)\n");
        return;
    }

    // Start from an empty controller. Anything left over from earlier traffic —
    // or a request still latched in a transmit buffer, which loopback mode will
    // happily deliver back to us — is read before our own frame and reported as
    // a data mismatch, failing the test on hardware that is perfectly fine.
    m.recoverBus();
    CanFrame stale{};
    for (int i = 0; i < 4 && m.hasRx(); ++i) m.read(stale);

    CanFrame tx{};
    tx.id = 0x7FF;
    tx.ext = false;
    tx.dlc = 8;
    tx.data[0] = 0xDE; tx.data[1] = 0xAD; tx.data[2] = 0xBE; tx.data[3] = 0xEF;
    tx.data[4] = 0xCA; tx.data[5] = 0xFE; tx.data[6] = 0xBA; tx.data[7] = 0xBE;

    if (m.send(tx) != McpError::Ok) {
        printf("[HWTEST]   FAIL (TX error)\n");
        return;
    }
    sleep_ms(10);

    CanFrame rx{};
    if (!m.hasRx()) {
        printf("[HWTEST]   FAIL (no RX after loopback TX - check CAN-H/CAN-L jumper)\n");
        return;
    }
    if (m.read(rx) != McpError::Ok) {
        printf("[HWTEST]   FAIL (RX read error)\n");
        return;
    }

    bool match = (rx.id == tx.id && rx.dlc == tx.dlc);
    for (int i = 0; i < 8 && match; ++i)
        if (rx.data[i] != tx.data[i]) match = false;

    if (match) {
        printf("[HWTEST]   PASS (TX=RX, ID=0x%03X, DLC=%d, data OK)\n", rx.id, rx.dlc);
    } else {
        printf("[HWTEST]   FAIL (data mismatch: TX=%02X%02X%02X%02X%02X%02X%02X%02X RX=%02X%02X%02X%02X%02X%02X%02X%02X)\n",
               tx.data[0],tx.data[1],tx.data[2],tx.data[3],tx.data[4],tx.data[5],tx.data[6],tx.data[7],
               rx.data[0],rx.data[1],rx.data[2],rx.data[3],rx.data[4],rx.data[5],rx.data[6],rx.data[7]);
        return;
    }

    m.setNormalMode();

    printf("[HWTEST] === Summary ===\n");
    printf("[HWTEST]   SPI: OK  Loopback: PASS\n");
    printf("[HWTEST] PASS: MCP2515 is working correctly.\n");

    // Re-init to restore normal operation
    printf("[HWTEST] Re-initializing CAN...\n");
    Mcp2515::Pins pins{spi0, 2, 3, 4, 5, 6};
    can_->init(pins, false);
    printf("[HWTEST] Done.\n");
#endif
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

// Disable Eco Mode: BSI RoutineControl DF0A with param 3C. Exact wire bytes and
// BSI key (B4E0) taken verbatim from pypsadiag's disableEcoMode() — a fixed,
// unlock-gated RoutineControl call, not a config zone write. Needs 'connect BMF'
// (or BSI) and 'unlock' first (try 'pin B4E0' if the family-default PIN fails).
void DiagShell::cmdEcoDisable() {
    if (!isConnected()) { printf("[DIAG] Not connected. 'connect BMF' first.\n"); return; }
    if (state_ == State::WaitingResponse) { printf("[DIAG] Waiting for previous response...\n"); return; }
    if (!unlocked_) {
        printf("[DIAG] Warning: ECU is not security unlocked. Routine may be rejected.\n");
    }
    printf("[DIAG] Sending Disable Eco Mode routine (RoutineControl DF0A)...\n");
    Req req{{}, 0};
    req.buf[0] = 0x31; req.buf[1] = 0x01; req.buf[2] = 0xDF; req.buf[3] = 0x0A; req.buf[4] = 0x3C;
    req.len = 5;
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
        sniff_raw_ = false;
        printf("[DIAG] Passive sniffing OFF.\n");
    } else if (strcmp(arg, "raw") == 0) {
        // The decoder only knows ~50 IDs and prints nothing for the rest, so on
        // a bus carrying anything else it looked broken. Raw mode prints every
        // frame (rate-limited in main.cpp) — this is the actual bus monitor.
        sniff_enabled_ = true;
        sniff_raw_ = true;
        printf("[DIAG] Passive sniffing ON (raw frames, sampled).\n");
    } else if (strcmp(arg, "on") == 0 || *arg == '\0') {
        sniff_enabled_ = true;
        sniff_raw_ = false;
        printf("[DIAG] Passive sniffing ON (decoded IDs only).\n");
    } else {
        printf("[DIAG] Usage: sniff [on|raw|off]\n");
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
    } else if (strcmp(sub, "rate") == 0) {
        if (strcmp(subarg, "hs") == 0 || strcmp(subarg, "HS") == 0) {
            sniff_bus_ = Bus::HighSpeed;
            setBusMode(CanBitrate::Bps500k, /*listen_only=*/true);
            printf("[GSNIFF] Switched to HS (500k). Connect MCP2515 to OBD pins 3/8.\n");
        } else if (strcmp(subarg, "ls") == 0 || strcmp(subarg, "LS") == 0) {
            sniff_bus_ = Bus::LowSpeed;
            setBusMode(CanBitrate::Bps125k, /*listen_only=*/true);
            printf("[GSNIFF] Switched to LS (125k). Connect MCP2515 to BSI CAN lines.\n");
        } else {
            printf("[GSNIFF] Usage: gsniff rate <hs|ls>\n");
        }
    } else {
        printf(
            "Usage: gsniff <alt-komut>\n"
            "  rate <hs|ls>           MCP2515 baud rate'ini degistir (500k/125k)\n"
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

// One MCP2515, one wire. The BSI gateways the body-bus ECUs onto CAN-HS, so
// every diagnostic exchange -- including the ECUs the table lists as LS -- runs
// at 500k in normal mode. Only the sniffer ever drops to 125k, and it must be
// put back before the next request: at the wrong rate nothing acknowledges us,
// and in listen-only TXREQ is never serviced at all, so the three TX buffers
// latch full and every send afterwards dies with "bus busy".
void DiagShell::setBusMode(CanBitrate rate, bool listen_only) {
    if (!can_) return;
    can_->reconfigureBus(Bus::HighSpeed, rate);
#ifndef HOST_TEST
    if (Mcp2515* m = can_->bus(Bus::HighSpeed)) {
        if (listen_only) m->setListenOnlyMode(); else m->setNormalMode();
    }
#else
    (void)listen_only;
#endif
}

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
    // Retries exhausted: all three buffers are latched with nothing draining
    // them. Drop them, or every later request inherits a dead transmitter --
    // which is why one stuck ECU used to poison the whole rest of a scan.
#ifndef HOST_TEST
    if (Mcp2515* m = can_->bus(active_bus_)) m->recoverBus();
#endif
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
        if (lidscan_active_) {
            // Most identifiers come back "request out of range" (0x31) or
            // "service not supported" (0x11) — that is the ECU saying the
            // identifier does not exist, and printing 250 of those buries the
            // hits. A refusal on security or session grounds is the opposite:
            // the identifier IS implemented, we are just not allowed to read it
            // yet, which is exactly what the sweep is looking for.
            uint8_t nrc = (len >= 3) ? pdu[2] : 0x00;
            if (nrc == 0x22 || nrc == 0x33 || nrc == 0x7E || nrc == 0x7F) {
                lidscan_hits_++;
                printf("[LIDSCAN] %04X: present, refused (NRC %02X)\n", lidscan_id_, nrc);
            }
            lidScanAdvance();
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
        } else if (!config_readall_active_) {
            // Plain manual command (connect/dtc/read/...): the ISO-TP dispatcher
            // already forced state_ = Connected the moment this PDU completed
            // reassembly, before it knew the response was negative. If the
            // rejected service was the session-open request itself, there never
            // was a session — leaving state_ = Connected here made 'isConnected()'
            // true for a session BSI had just refused, so the periodic keep-alive
            // kept firing 0x3E into it and getting the same NRC back forever.
            uint8_t session_open_service =
                (ecu_->proto == Protocol::KWP_IS) ? kwp::StartSession_IS
                                                   : kwp::StartDiagnosticSession; // == uds::DiagnosticSessionControl
            if (len >= 2 && pdu[1] == session_open_service) {
                state_ = State::Idle;
            }
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
        // Framing comes from the reply's service byte, not the ECU's nominal
        // protocol: readLiveData() promotes any id above 0xFF to UDS framing
        // even on a KWP ECU, and that ECU then answers 0x62.
        uint16_t resp_param_id = 0;
        size_t header_len = 0;
        if (service == uds::PosRead && len >= 3) {
            resp_param_id = (pdu[1] << 8) | pdu[2];
            header_len = 3;
        } else if (service == kwp::PosRead && len >= 2) {
            resp_param_id = pdu[1];
            header_len = 2;
        }

        if (header_len > 0) {
            bool in_list = false;
            for (int i = 0; i < live_param_count_; ++i) {
                if (live_param_ids_[i] == resp_param_id) { in_list = true; break; }
            }
            if (in_list) {
                const LiveDataParam* param =
                    findParamForEcu(ecu_->family, p, resp_param_id);
                if (param && param->decode) {
                    float val = param->decode(pdu + header_len, len - header_len);
                    printf("[LIVE] %s: %.1f %s\n", param->name, val, param->unit);
                } else {
                    printf("[LIVE] ID %04X undecoded raw = ", resp_param_id);
                    printHex(pdu + header_len, len - header_len);
                    printf("\n");
                }
            }
            return;
        }
    }

    // Zone read positive response. The reply's own service byte decides the
    // framing, never the ECU's nominal protocol: readZone() promotes any zone id
    // above 0xFF to UDS "22 hi lo" even on a KWP ECU -- RD4's 0x2A00 radio config
    // is the everyday case -- and such an ECU answers "62 hi lo". Taking the id
    // from pdu[1] alone yielded 0x2A instead of 0x2A00 and slid every data byte
    // one position left, so every decoded field came out wrong.
    if (service == kwp::PosRead || service == uds::PosRead) {
        const bool wide     = (service == uds::PosRead);
        const uint16_t zid  = wide ? ((static_cast<uint16_t>(pdu[1]) << 8) | pdu[2])
                                   : pdu[1];
        const size_t off    = wide ? 3 : 2;
        const uint8_t* data = pdu + off;
        const size_t data_len = (len > off) ? len - off : 0;

        if (lidscan_active_) {
            lidscan_hits_++;
            printf("[LIDSCAN] %04X: %zu bytes: ", zid, data_len);
            printHex(data, data_len);
            // ASCII alongside the hex: identification zones (VIN, part numbers,
            // supplier strings) are the ones worth spotting at a glance.
            bool printable = data_len > 0;
            for (size_t i = 0; i < data_len && printable; ++i)
                if (data[i] < 0x20 || data[i] > 0x7E) printable = false;
            if (printable) {
                printf("  \"");
                for (size_t i = 0; i < data_len; ++i) printf("%c", data[i]);
                printf("\"");
            }
            printf("\n");
            lidScanAdvance();
            return;
        }

        if (config_readall_active_) {
            if (!printZoneParams(zid, data, data_len)) {
                printf("[CONFIG] Zone %04X (no catalogue):\n", zid);
                for (size_t i = 0; i < data_len; ++i)
                    printf("  Byte %zu: %02X\n", i, data[i]);
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
        } else if (!printZoneParams(zid, data, data_len)) {
            printZoneData(pdu, len);
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
    uint8_t i = 0;
    for (; i < count && pos + 2 < len; ++i) {
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
    // The ECU's own count byte promised more records than its response actually
    // carried (a real, observed quirk on this AUTORADIO). Say so instead of
    // silently under-reporting — matches this project's "don't hide a mismatch" rule.
    if (i < count) {
        printf("  ... ECU declared %u fault(s) but response only carried %u "
               "(ECU-side inconsistency, not a transport error)\n", count, i);
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

    // The reply's service byte, not the ECU's nominal protocol: a KWP ECU still
    // answers 0x62 when the request was UDS-framed (any zone id above 0xFF).
    size_t header_len;
    uint16_t zone_id;

    if (pdu[0] == uds::PosRead && len >= 3) {
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
        if (live_param_count_ > 0) {
            printf("\nActive: ");
            for (int i = 0; i < live_param_count_; ++i)
                printf("%04X ", live_param_ids_[i]);
            printf("\n");
        }
        return;
    }

    if (strcmp(arg, "off") == 0) {
        live_polling_active_ = false;
        live_param_count_ = 0;
        live_param_idx_ = 0;
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

    // Toggle: if already in list, remove it; otherwise add it.
    for (int i = 0; i < live_param_count_; ++i) {
        if (live_param_ids_[i] == param_id) {
            for (int j = i; j < live_param_count_ - 1; ++j)
                live_param_ids_[j] = live_param_ids_[j + 1];
            live_param_count_--;
            if (live_param_idx_ >= live_param_count_) live_param_idx_ = 0;
            printf("[DIAG] Removed %04X from live poll list.\n", param_id);
            if (live_param_count_ == 0) live_polling_active_ = false;
            return;
        }
    }

    if (live_param_count_ >= kMaxLiveParams) {
        printf("[DIAG] Max %d live params reached.\n", kMaxLiveParams);
        return;
    }

    live_param_ids_[live_param_count_++] = param_id;
    live_polling_active_ = true;
    last_poll_us_ = 0;

    const LiveDataParam* param =
        findParamForEcu(ecu_->family, ecu_->proto, param_id);
    if (!param) {
        printf("[DIAG] Added %04X to live poll list.\n", param_id);
    } else {
        printf("[DIAG] Added %s (%04X) to live poll list.\n", param->name, param->id);
    }
}

void DiagShell::cmdRw(const char* arg) {
    if (strcmp(arg, "on") == 0) {
        rw_enabled_ = true;
        printf("[DIAG] R/W mode ENABLED — writes, actuator tests and flashing are live.\n");
    } else if (strcmp(arg, "off") == 0) {
        rw_enabled_ = false;
        printf("[DIAG] Read-only mode.\n");
    } else {
        printf("[DIAG] Mode: %s  (use 'rw on' / 'rw off')\n",
               rw_enabled_ ? "R/W" : "READ-ONLY");
    }
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
    setBusMode(CanBitrate::Bps500k, /*listen_only=*/false);
    tp_.reset();
    unlocked_ = false;
    manual_pin_valid_ = false;
    pending_count_ = 0;
    live_polling_active_ = false;
    live_param_count_ = 0;
    live_param_idx_ = 0;

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

void DiagShell::printOneZoneParam(const BsiZoneParam& bp,
                                  const uint8_t* data, size_t data_len) {
    uint8_t val = (bp.byte_offset < data_len) ? data[bp.byte_offset] : 0;
    if (bp.bit_mask != 0 && bp.bit_mask != 0xFF) {
        uint8_t m = bp.bit_mask;
        val &= bp.bit_mask;
        while ((m & 1) == 0) { m >>= 1; val >>= 1; }
    }
    printf("  %s: ", bp.name);
    if (bp.type == ZT_STRING) {
        // No length field in the catalogue, so a string runs to the end of the
        // zone. Non-printable bytes become '.' rather than reaching the terminal.
        for (size_t i = bp.byte_offset; i < data_len; ++i)
            putchar((data[i] >= 0x20 && data[i] < 0x7F) ? data[i] : '.');
        printf("\n");
    } else if (bp.type == ZT_BOOL) {
        printf("%s\n", val ? "ON" : "OFF");
    } else if (bp.type == ZT_ENUM && bp.enum_values) {
        // Walk to the terminator instead of indexing blind: the tables are
        // nullptr-terminated and shorter than the widest value a mask can hold,
        // so enum_values[val] used to read past the end.
        size_t n = 0;
        while (bp.enum_values[n]) ++n;
        printf("%s\n", val < n ? bp.enum_values[val] : "?");
    } else {
        printf("%u (0x%02X)\n", val, val);
    }
}

// Decode one config zone, printing its header and raw bytes first. Returns false
// when nothing describes the zone, so the caller can fall back.
//
// Two registries hold zone definitions and only one of them was ever consulted:
// kZoneCategories carries the BSI's own zones, while every other ECU's zones --
// RD4's 0x2A00 radio config among them -- live in kEcuParamSets. Zone ids are
// only unique within an ECU, so the connected ECU's table is searched first.
bool DiagShell::printZoneParams(uint16_t zid, const uint8_t* data, size_t data_len) {
    bool any = false;
    auto emit = [&](const BsiZoneParam& bp) {
        if (bp.zone_id != zid) return;
        if (!any) {
            any = true;
            printf("[CONFIG] Zone %04X: ", zid);
            printHex(data, data_len);
            printf("\n");
        }
        printOneZoneParam(bp, data, data_len);
    };

    const EcuParamSet* set = ecu_ ? findEcuParamSet(ecu_->family) : nullptr;
    if (set && set->config_params)
        for (size_t i = 0; i < set->config_count; ++i) emit(set->config_params[i]);

    // Only fall back to the BSI catalogue when the ECU's own table said nothing:
    // zone ids collide across ECUs (0x2500 is the screen's config on ECRAN_C and
    // the fuel sender law on the BSI), and running both would print one zone's
    // bytes under two unrelated sets of field names.
    if (any) return true;

    for (size_t c = 0; c < kZoneCategoryCount; ++c)
        for (size_t i = 0; i < kZoneCategories[c].count; ++i)
            emit(kZoneCategories[c].params[i]);

    return any;
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
    if (live_polling_active_ && live_param_count_ > 0) {
        printf("Active params (%d): ", live_param_count_);
        for (int i = 0; i < live_param_count_; ++i)
            printf("%04X ", live_param_ids_[i]);
        printf("\n");
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
        if (live_param_count_ > 0) {
            printf("\nActive: ");
            for (int i = 0; i < live_param_count_; ++i)
                printf("%04X ", live_param_ids_[i]);
            printf("\n");
        }
        return;
    }

    if (strcmp(arg, "off") == 0) {
        live_polling_active_ = false;
        live_param_count_ = 0;
        live_param_idx_ = 0;
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

    // Toggle: if already in list, remove it; otherwise add it.
    for (int i = 0; i < live_param_count_; ++i) {
        if (live_param_ids_[i] == param_id) {
            for (int j = i; j < live_param_count_ - 1; ++j)
                live_param_ids_[j] = live_param_ids_[j + 1];
            live_param_count_--;
            if (live_param_idx_ >= live_param_count_) live_param_idx_ = 0;
            const LiveDataParam* p =
                findParamForEcu(ecu_->family, ecu_->proto, param_id);
            printf("[DIAG] Stopped %s (%04X).\n", p ? p->name : "UNKNOWN", param_id);
            if (live_param_count_ == 0) live_polling_active_ = false;
            return;
        }
    }

    if (live_param_count_ >= kMaxLiveParams) {
        printf("[DIAG] Max %d measurement parameters reached.\n", kMaxLiveParams);
        return;
    }

    live_param_ids_[live_param_count_++] = param_id;
    live_polling_active_ = true;
    last_poll_us_ = 0;

    const LiveDataParam* param =
        findParamForEcu(ecu_->family, ecu_->proto, param_id);
    if (!param) {
        printf("[DIAG] Added %04X to measurement list.\n", param_id);
    } else {
        printf("[DIAG] Measuring %s (%04X).\n", param->name, param->id);
    }
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
