// Host self-check for the pure protocol logic. No Pico SDK, no hardware.
// Compile and run:
//   cmake -B build_host -DHOST_TEST=ON && cmake --build build_host && ./build_host/test_psa
//   or directly:
//   g++ -std=c++17 -Iinclude -DHOST_TEST tests/test_psa.cpp src/isotp.cpp
//       src/diag_shell.cpp src/flash_engine.cpp src/can_sniffer.cpp
//       src/wifi_server.cpp -o /tmp/test_psa && /tmp/test_psa
//       (all six .cpp on one line; they are wrapped here only for width)
//   (clang++ works too). Returns 0 on success, non-zero on the first failed assert.
//
// Covers the non-trivial logic in the project:
//   1. PSA seed/key algorithm  (determinism + a known pair)
//   2. ISO-15765-2 transport   (single + multi-frame, and the sender's flow
//      control: FC.CTS/WAIT/OVERFLOW, block size, STmin, N_Bs timeout)
//   3. Flash path              (S-record bounds, record-type filtering,
//      download extent, programming-session ordering)
//   4. Shell state machine     (unlock flow, transmit-failure reporting)
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "psa/psa_protocol.hpp"
#include "psa/isotp.hpp"
#include "psa/can_manager.hpp"
#include "psa/diag_shell.hpp"
#include "psa/ecu_keys.hpp"
#include "psa/live_data.hpp"
#include "psa/dtc_text.hpp"
#include "psa/flash_engine.hpp"
#include "psa/can_sniffer.hpp"
#include <vector>

// The shell's host-test clock. Tests advance it to exercise the flow-control
// and response timeouts.
extern uint64_t g_fake_time_us;

namespace psa {
std::vector<CanFrame> g_sent_frames;
// Tests flip this to make the controller refuse, so the failure path is covered
// rather than assumed. The real driver returns AllTxBusy whenever all three
// hardware transmit buffers are still latched.
McpError g_send_result = McpError::Ok;

McpError CanManager::send(Bus b, const CanFrame& f) {
    (void)b;
    if (g_send_result != McpError::Ok) return g_send_result;
    g_sent_frames.push_back(f);
    return McpError::Ok;
}
}

static void test_diag_shell_state() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    assert(shell.state() == psa::DiagShell::State::Idle);
    assert(!shell.isConnected());
    assert(shell.sniffEnabled());

    printf("  diag_shell: state initialization OK\n");
}


static void test_seedkey_determinism() {
    // Same (pin, seed) must always produce the same key.
    uint32_t k1 = psa::seed_key::compute(0xD91C, 0x12345678);
    uint32_t k2 = psa::seed_key::compute(0xD91C, 0x12345678);
    assert(k1 == k2);
    // Different seeds must (overwhelmingly) produce different keys.
    uint32_t k3 = psa::seed_key::compute(0xD91C, 0x12345679);
    assert(k3 != k1);
    // Pin is only 16 bits; key is 32 bits -> different pins diverge.
    uint32_t k4 = psa::seed_key::compute(0xA7D8, 0x12345678);
    assert(k4 != k1);
    printf("  seed/key: pin=D91C seed=12345678 -> key=%08lX (deterministic)\n",
           static_cast<unsigned long>(k1));
}

static void test_seedkey_known_vector() {
    // Regression sentinel: pin=D91C, seed=0 -> fixed expected output.
    // Hand-traced from ludwig-v/psa-seedkey-algorithm:
    //   transform(D9,1C,SEC1)=0x46E3  transform(0,0,SEC2)=0 -> r_msb=0x46E3
    //   transform(0,0,SEC1)=0  transform(46,E3,SEC2)=0x3D53 -> r_lsb=0x3D53
    //   -> key = 0x46E33D53
    // If this ever changes, somebody broke the algorithm.
    uint32_t k = psa::seed_key::compute(0xD91C, 0x00000000);
    assert(k == 0x46E33D53);
    printf("  seed/key: known vector (D91C,00000000)=%08lX OK\n",
           static_cast<unsigned long>(k));
}

static void test_isotp_single_frame() {
    psa::IsoTp tp;
    uint8_t pdu[3] = {0x22, 0xF1, 0x90};
    psa::CanFrame fr[4];
    size_t n = psa::IsoTp::encode(0x6A8, pdu, 3, fr, 4);
    assert(n == 1);
    assert(fr[0].id == 0x6A8);
    assert(fr[0].dlc == 4);
    assert(fr[0].data[0] == 3);                 // PCI = length
    assert(fr[0].data[1] == 0x22);
    assert(tp.feed(fr[0]) == psa::IsoTpStatus::Done);
    assert(tp.pdu_len() == 3);
    assert(std::memcmp(tp.pdu(), pdu, 3) == 0);
    printf("  iso-tp: single-frame (3B) round-trip OK\n");
}

static void test_isotp_multi_frame() {
    psa::IsoTp tp;
    uint8_t pdu[27];
    for (int i = 0; i < 27; ++i) pdu[i] = static_cast<uint8_t>(i);
    psa::CanFrame fr[8];
    size_t n = psa::IsoTp::encode(0x752, pdu, 27, fr, 8);
    assert(n == 4);                              // 1 first + 3 consecutive (6 + 7 + 7 + 7)
    assert(fr[0].data[0] == 0x10);               // first-frame PCI hi nibble
    assert(fr[0].data[1] == 27);                 // total length
    assert(fr[1].data[0] == 0x21);
    assert(fr[2].data[0] == 0x22);
    assert(fr[3].data[0] == 0x23);

    assert(tp.feed(fr[0]) == psa::IsoTpStatus::NeedFlowControl);
    psa::CanFrame fc = tp.flowControl(0x752);
    assert(fc.data[0] == 0x30);                  // Clear To Send
    assert(tp.feed(fr[1]) == psa::IsoTpStatus::Continue);
    assert(tp.feed(fr[2]) == psa::IsoTpStatus::Continue);
    assert(tp.feed(fr[3]) == psa::IsoTpStatus::Done);
    assert(tp.pdu_len() == 27);
    assert(std::memcmp(tp.pdu(), pdu, 27) == 0);
    printf("  iso-tp: multi-frame (27B -> 1FF + 3CF) round-trip OK\n");
}

static void test_ecu_table() {
    const psa::EcuAddr* bsi = psa::findEcu(0x752);
    assert(bsi != nullptr);
    assert(bsi->recv_id == 0x652);
    const psa::EcuAddr* inj = psa::findEcu(0x6A8);
    assert(inj != nullptr && inj->recv_id == 0x688);
    assert(psa::findEcu(0x0000) == nullptr);
    printf("  ecu table: BSI=752:652 INJ=6A8:688 lookup OK (%zu ECUs)\n", psa::kEcuCount);
}

static void test_ecu_keys_lookup() {
    // Source-verified family defaults (ludwig-v ECU_KEYS.md) for the C5 Mk1 FL ECUs.
    assert(psa::getEcuPin("BMF") == 0xB2B2);   // BSI (VALEO)
    assert(psa::getEcuPin("BSI") == 0xB2B2);
    assert(psa::getEcuPin("INJ") == 0x475A);   // EDC16C3
    assert(psa::getEcuPin("TELEMAT") == 0xD91C); // NAC
    // Families with no source-verified default return 0 ("unknown", use `pin`).
    assert(psa::getEcuPin("COMBINE") == 0x0000);
    assert(psa::getEcuPin("CLIM") == 0x0000);
    assert(psa::getEcuPin("ADC") == 0x0000);
    assert(psa::getEcuPin(nullptr) == 0x0000);
    printf("  ecu keys: pin lookups OK\n");
}

static void test_diag_shell_unlock_flow() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    // 1. Connect to BMF (BSI)
    psa::g_sent_frames.clear();
    shell.feedCommandLine("connect BMF");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(shell.isConnected());
    assert(!shell.isUnlocked());

    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x81); // KWP_IS startSession_IS

    // Feed session open positive response from BMF (service 0xC1):
    psa::CanFrame resp;
    resp.id = 0x652; // BMF recv_id
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 1; // PCI len = 1
    resp.data[1] = 0xC1; // Positive response to 0x81
    bool consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);
    assert(!shell.isUnlocked());

    // 2. Request unlock
    psa::g_sent_frames.clear();
    shell.feedCommandLine("unlock");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(!shell.isUnlocked());
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x27);
    assert(psa::g_sent_frames.back().data[2] == 0x83);

    // Feed a seed response from BMF:
    resp.data[0] = 6;
    resp.data[1] = 0x67;
    resp.data[2] = 0x83;
    resp.data[3] = 0x12;
    resp.data[4] = 0x34;
    resp.data[5] = 0x56;
    resp.data[6] = 0x78;
    
    psa::g_sent_frames.clear();
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    uint32_t expected_key = psa::seed_key::compute(psa::getEcuPin("BMF"), 0x12345678);

    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(!shell.isUnlocked());
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[0] == 6);
    assert(psa::g_sent_frames.back().data[1] == 0x27);
    assert(psa::g_sent_frames.back().data[2] == 0x84);
    uint32_t sent_key = (static_cast<uint32_t>(psa::g_sent_frames.back().data[3]) << 24) |
                        (static_cast<uint32_t>(psa::g_sent_frames.back().data[4]) << 16) |
                        (static_cast<uint32_t>(psa::g_sent_frames.back().data[5]) << 8) |
                        psa::g_sent_frames.back().data[6];
    assert(sent_key == expected_key);

    // Feed key verification response
    resp.data[0] = 2;
    resp.data[1] = 0x67;
    resp.data[2] = 0x84;
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);
    assert(shell.isUnlocked());

    // 3. Write command
    psa::g_sent_frames.clear();
    shell.feedCommandLine("write 01 02 03");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[0] == 4);
    assert(psa::g_sent_frames.back().data[1] == 0x34); // KWP_IS short-form write (34 XX ..)
    assert(psa::g_sent_frames.back().data[2] == 0x01);
    assert(psa::g_sent_frames.back().data[3] == 0x02);
    assert(psa::g_sent_frames.back().data[4] == 0x03);

    // Feed write positive response (KWP_IS: 0x74 = 0x34 | 0x40)
    resp.data[0] = 2;
    resp.data[1] = 0x74;
    resp.data[2] = 0x01;
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // 4. Trace command — a 12-byte PDU, so ISO-TP segments it. Only the First
    //    Frame may go out; the consecutive frame is owed to the ECU's flow
    //    control. Sending both immediately (as this used to) makes a real ECU
    //    drop the tail of every multi-frame request.
    psa::g_sent_frames.clear();
    shell.feedCommandLine("trace");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(psa::g_sent_frames.size() == 1);
    assert(psa::g_sent_frames[0].data[0] == 0x10);
    assert(psa::g_sent_frames[0].data[1] == 12);

    // Nothing more may leave until flow control arrives, however long we poll.
    g_fake_time_us += 1000;
    shell.poll();
    assert(psa::g_sent_frames.size() == 1);

    // ECU clears us to send (FS=CTS, BS=0 unlimited, STmin=0).
    psa::CanFrame fc{};
    fc.id = resp.id; fc.dlc = 3;
    fc.data[0] = 0x30; fc.data[1] = 0x00; fc.data[2] = 0x00;
    consumed = shell.feedDiagFrame(fc);
    assert(consumed);
    g_fake_time_us += 1000;
    shell.poll();
    assert(psa::g_sent_frames.size() == 2);
    assert(psa::g_sent_frames[1].data[0] == 0x21);

    printf("  diag_shell: unlock and configuration write flow OK\n");
}

static void test_diag_shell_unlock_uds_flow() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    // 1. Connect to TELEMAT
    psa::g_sent_frames.clear();
    shell.feedCommandLine("connect TELEMAT");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(shell.isConnected());
    assert(!shell.isUnlocked());

    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x10);
    assert(psa::g_sent_frames.back().data[2] == 0x03);

    // Feed session open positive response (service 0x50, sub 0x03):
    psa::CanFrame resp;
    resp.id = 0x664; // TELEMAT recv_id
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 2; // PCI len = 2
    resp.data[1] = 0x50; // Positive response to 0x10
    resp.data[2] = 0x03;
    bool consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);
    assert(!shell.isUnlocked());

    // 2. Request unlock
    psa::g_sent_frames.clear();
    shell.feedCommandLine("unlock");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x27);
    assert(psa::g_sent_frames.back().data[2] == 0x03);

    // Feed a seed response from TELEMAT:
    resp.data[0] = 6;
    resp.data[1] = 0x67;
    resp.data[2] = 0x03;
    resp.data[3] = 0x98;
    resp.data[4] = 0x76;
    resp.data[5] = 0x54;
    resp.data[6] = 0x90;
    
    psa::g_sent_frames.clear();
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    uint32_t expected_key = psa::seed_key::compute(psa::getEcuPin("TELEMAT"), 0x98765490);
    
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[0] == 6);
    assert(psa::g_sent_frames.back().data[1] == 0x27);
    assert(psa::g_sent_frames.back().data[2] == 0x04);
    uint32_t sent_key = (static_cast<uint32_t>(psa::g_sent_frames.back().data[3]) << 24) |
                        (static_cast<uint32_t>(psa::g_sent_frames.back().data[4]) << 16) |
                        (static_cast<uint32_t>(psa::g_sent_frames.back().data[5]) << 8) |
                        psa::g_sent_frames.back().data[6];
    assert(sent_key == expected_key);

    // Feed key verification response
    resp.data[0] = 2;
    resp.data[1] = 0x67;
    resp.data[2] = 0x04;
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);
    assert(shell.isUnlocked());

    // 3. Write command — 10-byte PDU, so First Frame now, consecutive frame only
    //    once the ECU sends flow control.
    psa::g_sent_frames.clear();
    shell.feedCommandLine("write 2901 FD 00 00 00 01 01 01");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(psa::g_sent_frames.size() == 1);
    assert(psa::g_sent_frames[0].data[0] == 0x10); // First Frame PCI
    assert(psa::g_sent_frames[0].data[1] == 10);   // PDU length 10

    psa::CanFrame fc{};
    fc.id = resp.id; fc.dlc = 3;
    fc.data[0] = 0x30; fc.data[1] = 0x00; fc.data[2] = 0x00;
    assert(shell.feedDiagFrame(fc));
    g_fake_time_us += 1000;
    shell.poll();
    assert(psa::g_sent_frames.size() == 2);
    assert(psa::g_sent_frames[1].data[0] == 0x21); // Consecutive Frame PCI


    // Feed write positive response (0x6E 0x29 0x01)
    resp.data[0] = 3;
    resp.data[1] = 0x6E;
    resp.data[2] = 0x29;
    resp.data[3] = 0x01;
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);

    printf("  diag_shell: UDS unlock and write flow OK\n");
}

static void test_live_data_decoders() {
    // Engine RPM: (data[0] << 8 | data[1]) * 0.125f
    // e.g. 800 rpm -> 800 / 0.125 = 6400 -> 0x1900
    uint8_t rpm_data[2] = {0x19, 0x00};
    assert(psa::decodeEngineRpm(rpm_data, 2) == 800.0f);

    // Coolant Temp: (int8_t)data[0] - 40.0f
    // e.g. 90 °C -> 90 + 40 = 130 -> 0x82
    uint8_t temp_data[1] = {0x82};
    assert(psa::decodeCoolantTemp(temp_data, 1) == 90.0f);

    // Battery Voltage: data[0] * 0.1f + 7.0f
    // e.g. 12.5V -> (12.5 - 7.0) / 0.1 = 55 -> 0x37
    uint8_t volt_data[1] = {0x37};
    assert(psa::decodeBatteryVoltage(volt_data, 1) == 12.5f);

    // Throttle Position: data[0] * 0.5f
    // e.g. 50% -> 50 / 0.5 = 100 -> 0x64
    uint8_t throttle_data[1] = {0x64};
    assert(psa::decodeThrottlePosition(throttle_data, 1) == 50.0f);

    // Lookup tests
    const auto* p1 = psa::findParam(psa::Protocol::UDS, 0x100A);
    assert(p1 != nullptr && std::strcmp(p1->name, "Engine RPM") == 0);

    const auto* p2 = psa::findParam(psa::Protocol::KWP_IS, 0x01);
    assert(p2 != nullptr && std::strcmp(p2->name, "Engine RPM") == 0);

    printf("  live_data: parameter decoders and lookups OK\n");
}

static void test_actuator_req_builders() {
    // UDS Routine Control
    psa::Req r1 = psa::startActuatorTest(psa::Protocol::UDS, 0x1234);
    assert(r1.buf[0] == 0x31);
    assert(r1.buf[1] == 0x01);
    assert(r1.buf[2] == 0x12);
    assert(r1.buf[3] == 0x34);
    assert(r1.len == 4);

    // KWP StartRoutineByLocalId default to 0x31
    psa::Req r2 = psa::startActuatorTest(psa::Protocol::KWP_IS, 0x05);
    assert(r2.buf[0] == 0x31);
    assert(r2.buf[1] == 0x05);
    assert(r2.len == 2);

    // KWP StartRoutineByLocalId with service 0x30
    psa::Req r3 = psa::startActuatorTest(psa::Protocol::KWP_IS, 0x3005);
    assert(r3.buf[0] == 0x30);
    assert(r3.buf[1] == 0x05);
    assert(r3.len == 2);

    // Actuator test with args
    uint8_t args[2] = {0xAA, 0xBB};
    psa::Req r4 = psa::startActuatorTest(psa::Protocol::UDS, 0x1234, args, 2);
    assert(r4.len == 6);
    assert(r4.buf[4] == 0xAA);
    assert(r4.buf[5] == 0xBB);

    printf("  actuator: request formatting builders OK\n");
}

static void test_diag_shell_live_data_and_actuator() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    // Connect to BMF (KWP)
    shell.feedCommandLine("connect BMF");
    // Feed positive connection response
    psa::CanFrame resp;
    resp.id = 0x652;
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 1;
    resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // Start live polling of coolant temp (KWP ID 0x02)
    psa::g_sent_frames.clear();
    shell.feedCommandLine("live 02");
    // Should trigger an immediate readLiveData request in poll()
    shell.poll();

    assert(!psa::g_sent_frames.empty());
    // KWP readZone/readLiveData for 0x02: 21 02
    assert(psa::g_sent_frames.back().data[1] == 0x21);
    assert(psa::g_sent_frames.back().data[2] == 0x02);
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);

    // Feed response for coolant temp: 90 °C -> 0x82
    resp.data[0] = 3;
    resp.data[1] = 0x61; // Positive response to 0x21
    resp.data[2] = 0x02; // ID
    resp.data[3] = 0x82; // 90°C (130 - 40)
    
    psa::g_sent_frames.clear();
    bool consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // Test actuator command (KWP routine 0x05)
    psa::g_sent_frames.clear();
    shell.feedCommandLine("actuator 05 AA BB");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(!psa::g_sent_frames.empty());
    // KWP StartRoutineByLocalId: 31 05 AA BB
    assert(psa::g_sent_frames.back().data[0] == 4); // len
    assert(psa::g_sent_frames.back().data[1] == 0x31);
    assert(psa::g_sent_frames.back().data[2] == 0x05);
    assert(psa::g_sent_frames.back().data[3] == 0xAA);
    assert(psa::g_sent_frames.back().data[4] == 0xBB);

    // Feed actuator response
    resp.data[0] = 2;
    resp.data[1] = 0x71; // Positive routine control response
    resp.data[2] = 0x05;
    consumed = shell.feedDiagFrame(resp);
    assert(consumed);
    assert(shell.state() == psa::DiagShell::State::Connected);

    printf("  diag_shell: live monitoring & actuator commands OK\n");
}

static void test_srecord_parser() {
    psa::SRecord rec;
    bool ok = psa::SRecordParser::parseLine("S30A001000001122334455E6", rec);
    assert(ok);
    assert(rec.valid);
    assert(rec.type == 3);
    assert(rec.address == 0x00100000);
    assert(rec.data_len == 5);
    assert(rec.data[0] == 0x11);
    assert(rec.data[4] == 0x55);

    ok = psa::SRecordParser::parseLine("S30A001000001122334455E0", rec);
    assert(!ok);

    ok = psa::SRecordParser::parseLine("S106000011223393", rec);
    assert(ok);
    assert(rec.valid);
    assert(rec.type == 1);
    assert(rec.address == 0x0000);
    assert(rec.data_len == 3);
    assert(rec.data[0] == 0x11);

    printf("  flash_engine: S-record parser tests OK\n");
}

static void test_flash_checksum() {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = psa::FlashChecksum::crc16_x25(data, 4);
    assert(crc != 0);
    printf("  flash_engine: X-25 CRC-16 calculation OK (crc = %04X)\n", crc);
}

static void test_flash_engine_uds() {
    psa::FlashEngine eng;
    eng.init(psa::Protocol::UDS);
    assert(eng.step() == psa::FlashEngine::Step::Idle);

    psa::SRecord rec;
    rec.address = 0x00100000;
    rec.data_len = 4;
    std::memcpy(rec.data, "\x11\x22\x33\x44", 4);

    psa::Req req = eng.nextRequest(rec, 1);
    assert(eng.step() == psa::FlashEngine::Step::RequestErase);
    assert(req.buf[0] == 0x31);
    assert(req.buf[1] == 0x01);

    uint8_t resp[] = {0x71};
    eng.handleResponse(0x71, resp, 1);
    assert(eng.step() == psa::FlashEngine::Step::RequestDownload);

    req = eng.nextRequest(rec, 1);
    assert(eng.step() == psa::FlashEngine::Step::TransferData);
    assert(req.buf[0] == 0x34);

    req = eng.nextRequest(rec, 1);
    assert(eng.step() == psa::FlashEngine::Step::TransferData);
    assert(req.buf[0] == 0x36);
    assert(req.buf[1] == 1);
    assert(req.buf[2] == 0x11);

    // Finalize: finishTransfer() leaves TransferData, then nextRequest emits the
    // transfer-exit (0x37), and the following one emits the checksum routine (0x31).
    eng.finishTransfer();
    assert(eng.step() == psa::FlashEngine::Step::RequestTransferExit);
    req = eng.nextRequest(rec, 0);
    assert(req.buf[0] == 0x37);
    assert(eng.step() == psa::FlashEngine::Step::VerifyChecksum);
    req = eng.nextRequest(rec, 0);
    assert(req.buf[0] == 0x31);
    assert(eng.step() == psa::FlashEngine::Step::Done);

    printf("  flash_engine: UDS flashing steps state machine OK\n");
}

static void test_multi_param_live_polling() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    // Connect to BMF (KWP)
    shell.feedCommandLine("connect BMF");
    psa::CanFrame resp;
    resp.id = 0x652;
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 1;
    resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // Add first param (RPM 0x01)
    psa::g_sent_frames.clear();
    shell.feedCommandLine("live 01");
    g_fake_time_us = 1000;
    shell.poll();
    assert(!psa::g_sent_frames.empty());
    // Should request 21 01
    assert(psa::g_sent_frames.back().data[1] == 0x21);
    assert(psa::g_sent_frames.back().data[2] == 0x01);

    // Respond to 0x01
    resp.data[0] = 3;
    resp.data[1] = 0x61;
    resp.data[2] = 0x01;
    resp.data[3] = 0x0A; // RPM = 10 * 32 = 320 rpm (rough)
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // Add second param (Coolant 0x02)
    shell.feedCommandLine("live 02");
    // First poll should still send 0x01 (round-robin, idx was 0, now idx=1 after first add)
    shell.poll();
    if (psa::g_sent_frames.empty()) {
        // Time hasn't advanced: pump clock forward
        g_fake_time_us = 250001;
        psa::g_sent_frames.clear();
        shell.poll();
    }
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[2] == 0x01);

    // Respond to 0x01
    resp.data[2] = 0x01;
    resp.data[3] = 0x82;
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // Next poll sends 0x02 (idx=1 % 2 = 1)
    g_fake_time_us = 500001; // advance past 250ms
    psa::g_sent_frames.clear();
    shell.poll();
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[2] == 0x02);

    // Respond to 0x02
    resp.data[2] = 0x02;
    resp.data[3] = 0x82;
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp);
    assert(shell.state() == psa::DiagShell::State::Connected);

    // Remove 0x01 (toggle off)
    shell.feedCommandLine("live 01");

    // Poll should send 0x02 (only remaining)
    g_fake_time_us = 750001;
    psa::g_sent_frames.clear();
    shell.poll();
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[2] == 0x02);

    // Stop all
    shell.feedCommandLine("live off");
    psa::g_sent_frames.clear();
    shell.poll();
    // No new request should be sent
    assert(psa::g_sent_frames.empty());

    printf("  diag_shell: multi-param live polling OK\n");
}

static void test_pin_override_flow() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    // CLIM has no source-verified family PIN (getEcuPin -> 0), so unlock must NOT
    // send a key frame; it should ask for a manual PIN instead.
    shell.feedCommandLine("connect CLIM");
    psa::CanFrame resp;
    resp.id = 0x66D; // CLIM recv_id
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 2; resp.data[1] = 0x50; resp.data[2] = 0xC0; // KWP_HAB session open
    shell.feedDiagFrame(resp);
    assert(shell.state() == psa::DiagShell::State::Connected);

    shell.feedCommandLine("unlock");
    // Seed response with non-zero seed:
    resp.data[0] = 6; resp.data[1] = 0x67; resp.data[2] = 0x83;
    resp.data[3] = 0x11; resp.data[4] = 0x22; resp.data[5] = 0x33; resp.data[6] = 0x44;
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp);
    // No key sent (unknown PIN), shell returns to Connected.
    assert(psa::g_sent_frames.empty());
    assert(shell.state() == psa::DiagShell::State::Connected);
    assert(!shell.isUnlocked());

    // Provide the PIN manually, unlock again -> key frame is now sent.
    shell.feedCommandLine("pin B2B2");
    shell.feedCommandLine("unlock");
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp); // same seed response
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x27);
    assert(psa::g_sent_frames.back().data[2] == 0x84);
    uint32_t sent_key = (static_cast<uint32_t>(psa::g_sent_frames.back().data[3]) << 24) |
                        (static_cast<uint32_t>(psa::g_sent_frames.back().data[4]) << 16) |
                        (static_cast<uint32_t>(psa::g_sent_frames.back().data[5]) << 8) |
                        psa::g_sent_frames.back().data[6];
    assert(sent_key == psa::seed_key::compute(0xB2B2, 0x11223344));

    printf("  diag_shell: manual PIN override flow OK\n");
}

// The sender must obey the peer's flow control. Before this existed, every
// consecutive frame was dumped on the bus the instant the First Frame went out,
// which a real ECU drops — so multi-frame writes and flashing never worked.
// A refused transmission must not be swallowed. The shell used to discard every
// CanManager::send() result, so a request that never reached the bus looked
// identical to one the ECU ignored — the user just saw a response timeout.
static void test_shell_reports_transmit_failure() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    psa::g_sent_frames.clear();
    psa::g_send_result = psa::McpError::Ok;
    shell.feedCommandLine("connect BMF");
    psa::CanFrame resp{};
    resp.id = 0x652; resp.dlc = 8;
    resp.data[0] = 1; resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    assert(shell.isConnected());

    // Now the controller refuses everything.
    psa::g_send_result = psa::McpError::AllTxBusy;
    psa::g_sent_frames.clear();
    shell.feedCommandLine("dtc");
    assert(psa::g_sent_frames.empty());          // nothing reached the bus
    // and no transmit state is left dangling for poll() to trip over
    psa::g_send_result = psa::McpError::Ok;
    g_fake_time_us += 1000;
    shell.poll();
    assert(psa::g_sent_frames.empty());

    printf("  diag_shell: refused transmission is reported, not swallowed OK\n");
}

static void test_isotp_tx_flow_control() {
    using namespace psa;
    uint8_t pdu[30];
    for (size_t i = 0; i < sizeof(pdu); ++i) pdu[i] = static_cast<uint8_t>(i);

    IsoTp tp;
    CanFrame f{};
    uint64_t now = 1000;
    assert(tp.beginSend(0x752, pdu, sizeof(pdu), now, f));
    assert(f.data[0] == 0x10 && f.data[1] == 30);   // First Frame, 30 bytes
    assert(tp.txActive());

    // Nothing may go out before flow control, no matter how much time passes.
    CanFrame cf{};
    assert(!tp.nextTxFrame(now + 100000, cf));

    // FC.WAIT holds us; still nothing on the wire.
    CanFrame fc{};
    fc.dlc = 3; fc.data[0] = 0x31; fc.data[1] = 0x00; fc.data[2] = 0x00;
    assert(tp.feed(fc) == IsoTpStatus::TxWait);
    assert(!tp.nextTxFrame(now + 200000, cf));

    // FC.CTS with BS=2 and STmin=5 ms: exactly two frames, spaced 5 ms apart.
    fc.data[0] = 0x30; fc.data[1] = 0x02; fc.data[2] = 0x05;
    assert(tp.feed(fc) == IsoTpStatus::TxClearToSend);
    now += 200000;
    assert(tp.nextTxFrame(now, cf));
    assert(cf.data[0] == 0x21);
    assert(!tp.nextTxFrame(now, cf));           // STmin not elapsed
    now += 5000;
    assert(tp.nextTxFrame(now, cf));
    assert(cf.data[0] == 0x22);
    now += 5000;
    assert(!tp.nextTxFrame(now, cf));           // block exhausted, awaiting new FC

    // A fresh FC (BS=0 = unlimited, STmin=0) drains the rest.
    fc.data[1] = 0x00; fc.data[2] = 0x00;
    assert(tp.feed(fc) == IsoTpStatus::TxClearToSend);
    int emitted = 0;
    while (tp.nextTxFrame(now, cf)) ++emitted;
    assert(emitted == 2);                       // 30 bytes = 6 + 4x7 -> 4 CFs total
    assert(!tp.txActive());

    // FC.OVERFLOW means the ECU refused the transfer; we must stop, not keep
    // writing into an ECU that has explicitly said no.
    IsoTp tp2;
    assert(tp2.beginSend(0x752, pdu, sizeof(pdu), now, f));
    fc.data[0] = 0x32;
    assert(tp2.feed(fc) == IsoTpStatus::TxAbort);
    assert(!tp2.txActive());
    assert(!tp2.nextTxFrame(now + 100000, cf));

    // An ECU that never answers must not leave the transfer wedged forever.
    IsoTp tp3;
    assert(tp3.beginSend(0x752, pdu, sizeof(pdu), now, f));
    assert(!tp3.txTimedOut(now + 100000));
    assert(tp3.txTimedOut(now + 2000000));

    // A single frame owes nothing and leaves no transmit state behind.
    IsoTp tp4;
    uint8_t sf[3] = {0x22, 0xF1, 0x90};
    assert(tp4.beginSend(0x752, sf, sizeof(sf), now, f));
    assert(f.data[0] == 3);
    assert(!tp4.txActive());

    printf("  isotp: sender honours FC.CTS/WAIT/OVERFLOW, BS and STmin OK\n");
}

// The flow control we advertise as a receiver must not tell the ECU to stream
// without limit: the controller only buffers two frames.
static void test_isotp_rx_flow_control_params() {
    using namespace psa;
    IsoTp tp;
    CanFrame fc = tp.flowControl(0x752);
    assert(fc.data[0] == 0x30);                 // clear to send
    assert(fc.data[1] == IsoTp::kRxBlockSize);  // bounded block, not 0/unlimited
    assert(fc.data[1] != 0);
    assert(fc.data[2] == IsoTp::kRxStMinMs);    // a real gap, not 0
    assert(fc.data[2] != 0);
    printf("  isotp: receiver advertises a bounded BS and non-zero STmin OK\n");
}

static void test_isotp_out_of_order_rejected() {
    psa::IsoTp tp;
    uint8_t pdu[20];
    for (int i = 0; i < 20; ++i) pdu[i] = static_cast<uint8_t>(i);
    psa::CanFrame fr[8];
    size_t n = psa::IsoTp::encode(0x752, pdu, 20, fr, 8);
    assert(n >= 2);

    // Feed first frame -> NeedFlowControl
    assert(tp.feed(fr[0]) == psa::IsoTpStatus::NeedFlowControl);

    // Feed consecutive frame OUT of order (0x22 instead of 0x21)
    assert(tp.feed(fr[2]) == psa::IsoTpStatus::Error);
    printf("  iso-tp: out-of-order consecutive frame rejected OK\n");
}

static void test_isotp_malformed_frames_rejected() {
    psa::IsoTp tp;

    // Single frame claiming more bytes than the DLC carries must be rejected,
    // not silently truncated into a Done buffer of uninitialised bytes.
    psa::CanFrame sf{};
    sf.id = 0x652; sf.dlc = 3; sf.data[0] = 0x05; // claims 5 bytes, carries 2
    assert(tp.feed(sf) == psa::IsoTpStatus::Error);

    // First frame declaring <=7 total bytes is malformed; must be rejected before
    // it can underflow the consecutive-frame chunk clamp on the next frame.
    psa::IsoTp tp2;
    psa::CanFrame ff{};
    ff.id = 0x652; ff.dlc = 8; ff.data[0] = 0x10; ff.data[1] = 0x05; // FF, total len 5
    assert(tp2.feed(ff) == psa::IsoTpStatus::Error);

    // A valid FF (total len 8) followed by a runaway would have overflowed before
    // the fix; confirm the FF itself is accepted so we didn't over-reject.
    psa::IsoTp tp3;
    ff.data[1] = 0x08;
    assert(tp3.feed(ff) == psa::IsoTpStatus::NeedFlowControl);

    printf("  iso-tp: malformed single/first frames rejected OK\n");
}

static void test_can_sniffer_count_mode() {
    psa::CanSniffer sniff;
    sniff.init();
    psa::CanFrame f{};
    f.dlc = 8;

    // Baseline: 0x0B6 byte0 is a free-running counter (noisy, gets masked);
    // 0x1D0 sits quiet.
    sniff.beginBaseline(0, 1000);
    f.id = 0x0B6;
    for (int i = 0; i < 4; ++i) {
        for (int b = 0; b < 8; ++b) f.data[b] = 0;
        f.data[0] = static_cast<uint8_t>(i);
        sniff.feed(psa::Bus::HighSpeed, f);
    }
    f.id = 0x1D0;
    for (int b = 0; b < 8; ++b) f.data[b] = 0x10;
    sniff.feed(psa::Bus::LowSpeed, f);
    sniff.tick(1000);

    // Count phase: "press driver-temp-down 5 times" -> 0x1D0 byte5 changes 5x.
    sniff.beginCount(5);
    uint8_t v = 0x10;
    for (int i = 0; i < 5; ++i) {
        f.id = 0x1D0;
        for (int b = 0; b < 8; ++b) f.data[b] = 0x10;
        v--;
        f.data[5] = v;
        sniff.feed(psa::Bus::LowSpeed, f);
    }
    // Noise on the masked byte must not out-rank the real signal.
    f.id = 0x0B6;
    for (int b = 0; b < 8; ++b) f.data[b] = 0;
    f.data[0] = 0x55;
    sniff.feed(psa::Bus::HighSpeed, f);

    sniff.stop();

    psa::Bus bus; uint16_t id; uint8_t byte; uint16_t score;
    assert(sniff.bestCandidate(&bus, &id, &byte, &score));
    assert(bus == psa::Bus::LowSpeed);
    assert(id == 0x1D0);
    assert(byte == 5);
    assert(score == 0); // exact match: 5 observed vs 5 expected

    printf("  can_sniffer: count-mode baseline masking + exact match OK\n");
}

static void test_can_sniffer_hold_mode() {
    psa::CanSniffer sniff;
    sniff.init();
    psa::CanFrame f{};
    f.dlc = 8;

    // Baseline: RPM (0x0B6 bytes0-1) idles at 800rpm raw = 0x1900.
    sniff.beginBaseline(0, 1000);
    f.id = 0x0B6;
    f.data[0] = 0x19; f.data[1] = 0x00;
    for (int b = 2; b < 8; ++b) f.data[b] = 0;
    sniff.feed(psa::Bus::HighSpeed, f);
    sniff.tick(1000);

    // Hold: rev to ~2000rpm raw = 0x3E80 and keep it there.
    sniff.beginHold();
    f.data[0] = 0x3E; f.data[1] = 0x80;
    sniff.feed(psa::Bus::HighSpeed, f);
    sniff.feed(psa::Bus::HighSpeed, f); // repeats at the same value: held steady
    sniff.feed(psa::Bus::HighSpeed, f);
    sniff.stop();

    psa::Bus bus; uint16_t id; uint8_t byte; uint16_t score;
    assert(sniff.bestCandidate(&bus, &id, &byte, &score));
    assert(bus == psa::Bus::HighSpeed);
    assert(id == 0x0B6);
    assert(byte == 0); // first byte that left the baseline value wins the tie
    assert(score == 1); // one transition into the held value, then rock steady

    printf("  can_sniffer: hold-mode steady-value detection OK\n");
}

static void test_can_sniffer_sweep_mode() {
    psa::CanSniffer sniff;
    sniff.init();
    psa::CanFrame f{};
    f.dlc = 8;

    sniff.beginBaseline(0, 1000);
    f.id = 0x0B6;
    f.data[0] = 0x19; f.data[1] = 0x00;
    for (int b = 2; b < 8; ++b) f.data[b] = 0;
    sniff.feed(psa::Bus::HighSpeed, f);
    sniff.tick(1000);

    // Sweep: slowly raise the low byte of RPM, one direction only.
    sniff.beginSweep();
    uint8_t v = 0x00;
    for (int i = 0; i < 6; ++i) {
        v = static_cast<uint8_t>(v + 0x10);
        f.data[1] = v;
        sniff.feed(psa::Bus::HighSpeed, f);
    }
    sniff.stop();

    psa::Bus bus; uint16_t id; uint8_t byte; uint16_t score;
    assert(sniff.bestCandidate(&bus, &id, &byte, &score));
    assert(bus == psa::Bus::HighSpeed);
    assert(id == 0x0B6);
    assert(byte == 1);

    printf("  can_sniffer: sweep-mode monotonic detection OK\n");
}

static void test_can_sniffer_climate_scenario() {
    psa::CanSniffer sniff;
    sniff.init();
    psa::CanFrame f{};
    f.dlc = 8;
    f.id = 0x1D0;
    for (int b = 0; b < 8; ++b) f.data[b] = 0x10;

    uint64_t now = 0;
    sniff.beginRun(&psa::kClimateScenario, now);
    assert(sniff.scenarioActive());
    assert(sniff.mode() == psa::CanSniffer::Mode::Baseline);

    sniff.feed(psa::Bus::LowSpeed, f); // quiet baseline sample
    now += psa::CanSniffer::kDefaultBaselineUs;
    sniff.tick(now); // ends baseline, auto-starts step 1 (drv_temp_down, count=5)
    assert(sniff.mode() == psa::CanSniffer::Mode::Count);

    uint8_t v5 = 0x10;
    for (int i = 0; i < 5; ++i) { v5--; f.data[5] = v5; sniff.feed(psa::Bus::LowSpeed, f); }
    sniff.nextStep(now); // learns drv_temp_down, starts drv_temp_up

    for (int i = 0; i < 5; ++i) { v5++; f.data[5] = v5; sniff.feed(psa::Bus::LowSpeed, f); }
    sniff.nextStep(now); // learns drv_temp_up, starts pass_temp_down

    assert(sniff.learnedCount() == 2);
    assert(std::strcmp(sniff.learnedAt(0).label, "drv_temp_down") == 0);
    assert(sniff.learnedAt(0).bus == psa::Bus::LowSpeed);
    assert(sniff.learnedAt(0).id == 0x1D0);
    assert(sniff.learnedAt(0).byte == 5);
    assert(std::strcmp(sniff.learnedAt(1).label, "drv_temp_up") == 0);
    assert(sniff.learnedAt(1).byte == 5);

    // Drive the remaining steps to confirm the scenario terminates cleanly.
    while (sniff.scenarioActive()) {
        uint8_t expected = psa::kClimateScenario.steps[
            sniff.learnedCount() < psa::kClimateScenario.count ? sniff.learnedCount() : 0].expected;
        for (uint8_t i = 0; i < expected; ++i) {
            v5 = static_cast<uint8_t>(v5 + 1);
            f.data[5] = v5;
            sniff.feed(psa::Bus::LowSpeed, f);
        }
        sniff.nextStep(now);
    }
    assert(!sniff.scenarioActive());
    assert(sniff.mode() == psa::CanSniffer::Mode::Idle);
    assert(sniff.learnedCount() == psa::kClimateScenario.count);

    printf("  can_sniffer: climate scenario runner (%u steps) terminates cleanly OK\n",
           static_cast<unsigned>(psa::kClimateScenario.count));
}

static void test_flash_shell_begin() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);

    // Connect to BMF and unlock
    psa::g_sent_frames.clear();
    shell.feedCommandLine("connect BMF");
    psa::CanFrame resp;
    resp.id = 0x652;
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 1;
    resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    assert(shell.isConnected());

    shell.feedCommandLine("unlock");
    resp.data[0] = 6;
    resp.data[1] = 0x67;
    resp.data[2] = 0x83;
    resp.data[3] = 0x00;
    resp.data[4] = 0x00;
    resp.data[5] = 0x00;
    resp.data[6] = 0x00;
    shell.feedDiagFrame(resp);
    assert(shell.isUnlocked());

    // 'flash begin' with nothing staged must refuse: the erase/download
    // handshake completes in milliseconds and would reach TransferData empty.
    psa::g_sent_frames.clear();
    shell.feedCommandLine("flash begin");
    assert(psa::g_sent_frames.empty());

    // Stage an image, then begin. That must open a programming session first —
    // an ECU rejects erase and download outright from the diagnostic session.
    shell.feedCommandLine("flash S30A001000001122334455E6");
    psa::g_sent_frames.clear();
    shell.feedCommandLine("flash begin");
    assert(!psa::g_sent_frames.empty());
    // BMF speaks KWP_IS, whose session start is the single byte 0x81.
    assert(psa::g_sent_frames.back().data[0] == 1);
    assert(psa::g_sent_frames.back().data[1] == 0x81);

    // Only once that is acknowledged does the erase request go out.
    psa::g_sent_frames.clear();
    resp.data[0] = 1;
    resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    assert(!psa::g_sent_frames.empty());
    // KWP erase: 31 81 81 F0 5A
    assert(psa::g_sent_frames.back().data[1] == 0x31);
    assert(psa::g_sent_frames.back().data[2] == 0x81);

    printf("  flash_shell: begin opens programming session, then erases OK\n");
}

static void test_flash_shell_srecord_staging() {
    psa::CanManager can;
    psa::DiagShell shell;
    shell.init(&can);
    shell.feedCommandLine("connect BMF");
    psa::CanFrame resp;
    resp.id = 0x652;
    resp.dlc = 8;
    std::memset(resp.data, 0, 8);
    resp.data[0] = 1;
    resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    shell.feedCommandLine("unlock");
    resp.data[0] = 6;
    resp.data[1] = 0x67;
    resp.data[2] = 0x83;
    resp.data[3] = 0x00;
    resp.data[4] = 0x00;
    resp.data[5] = 0x00;
    resp.data[6] = 0x00;
    shell.feedDiagFrame(resp);

    // Stage a valid S3 record (6 payload bytes at 0x00100000), then begin.
    shell.feedCommandLine("flash S30A001000001122334455E6");
    // Non-data records must be refused, not pushed to the ECU as firmware.
    shell.feedCommandLine("flash S0030000FC");            // S0 header
    shell.feedCommandLine("flash S9030000FC");            // S9 start address

    psa::g_sent_frames.clear();
    shell.feedCommandLine("flash begin");
    // Programming session ack -> erase request
    resp.data[0] = 1;
    resp.data[1] = 0xC1;
    shell.feedDiagFrame(resp);
    // Feed erase response -> auto-advances to RequestDownload, sends 0x34
    psa::g_sent_frames.clear();
    resp.data[0] = 1;
    resp.data[1] = 0x70;
    shell.feedDiagFrame(resp);
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x34);
    // The download must announce the staged image's real extent, not a dummy
    // address of 0 with a hardcoded size. KWP form: 34 <addr 3B> <cmp> <size 2B>.
    assert(psa::g_sent_frames.back().data[2] == 0x10);   // addr 0x001000..
    assert(psa::g_sent_frames.back().data[3] == 0x00);
    assert(psa::g_sent_frames.back().data[4] == 0x00);
    assert(psa::g_sent_frames.back().data[6] == 0x00);   // size hi
    assert(psa::g_sent_frames.back().data[7] == 5);      // size lo = 5 payload bytes

    // Feed download positive response -> auto-advances to TransferData and
    // immediately ships the one staged block as 0x36.
    resp.data[0] = 1;
    resp.data[1] = 0x76;
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp);
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x36);
    printf("  flash_shell: staging, extent and non-data-record rejection OK\n");
}

// An ordinary 32-byte S-record used to memcpy 34 bytes into a 16-byte Req::buf
// in the caller's stack frame. The parser must now bound it, and TransferData
// must never write past buf whatever it is handed.
static void test_flash_srecord_bounds() {
    using namespace psa;
    SRecord rec{};
    // Build an S1 with a 32-byte payload rather than hand-typing it: byte_count
    // is 2 address + 32 data + 1 checksum, and the checksum is ~sum of all of
    // those plus the count itself.
    auto build = [](char* out, uint8_t type, uint16_t addr, const uint8_t* data, uint8_t n) {
        uint8_t count = static_cast<uint8_t>(2 + n + 1);
        uint8_t sum = count;
        sum += static_cast<uint8_t>(addr >> 8);
        sum += static_cast<uint8_t>(addr & 0xFF);
        for (uint8_t i = 0; i < n; ++i) sum += data[i];
        int p = std::sprintf(out, "S%u%02X%04X", type, count, addr);
        for (uint8_t i = 0; i < n; ++i) p += std::sprintf(out + p, "%02X", data[i]);
        std::sprintf(out + p, "%02X", static_cast<uint8_t>(~sum));
    };
    uint8_t payload[32];
    for (int i = 0; i < 32; ++i) payload[i] = static_cast<uint8_t>(i);
    char line[128];
    build(line, 1, 0x0010, payload, 32);
    bool ok = SRecordParser::parseLine(line, rec);
    assert(ok);
    assert(rec.data_len == 32);
    assert(rec.data_len <= sizeof(rec.data));

    // Whatever the record claims, the emitted request must stay inside buf.
    FlashEngine fe;
    fe.init(Protocol::UDS);
    fe.nextRequest(rec, 0);                 // Idle -> RequestErase
    const uint8_t erase_ok[] = {0x71, 0x01};
    fe.handleResponse(0x71, erase_ok, sizeof(erase_ok));   // -> RequestDownload
    Req dl = fe.nextRequest(rec, 0);        // -> TransferData
    assert(dl.len <= sizeof(dl.buf));
    Req td = fe.nextRequest(rec, 1);
    assert(td.buf[0] == 0x36);
    assert(td.len == 2 + 32);
    assert(td.len <= sizeof(td.buf));

    // A payload larger than one TransferData can carry is rejected outright
    // rather than silently truncated into a hole in the ECU image.
    SRecord big{};
    char huge[600];
    std::strcpy(huge, "S1FF0010");
    for (int i = 0; i < 252; ++i) std::strcat(huge, "AA");
    std::strcat(huge, "00");
    assert(!SRecordParser::parseLine(huge, big));

    // Record-type classification.
    assert(SRecordParser::isDataRecord(1));
    assert(SRecordParser::isDataRecord(2));
    assert(SRecordParser::isDataRecord(3));
    assert(!SRecordParser::isDataRecord(0));
    assert(!SRecordParser::isDataRecord(5));
    assert(!SRecordParser::isDataRecord(7));
    assert(!SRecordParser::isDataRecord(9));
    printf("  flash_engine: S-record bounds + data-record classification OK\n");
}

static void test_dtc_text() {
    using namespace psa;
    char b[6];
    // Algorithmic J2012 decode across all four category letters.
    assert(strcmp(formatDtcCode(0x0140, b), "P0140") == 0);
    assert(strcmp(formatDtcCode(0x0300, b), "P0300") == 0);
    assert(strcmp(formatDtcCode(0x1234, b), "P1234") == 0); // mfr-specific P
    assert(strcmp(formatDtcCode(0x4567, b), "C0567") == 0); // C: bits15-14=01
    assert(strcmp(formatDtcCode(0x8ABC, b), "B0ABC") == 0); // B: bits15-14=10
    assert(strcmp(formatDtcCode(0xC100, b), "U0100") == 0); // U: bits15-14=11
    // Descriptions: known generic codes resolve, unknown returns nullptr.
    assert(dtcDescription(0x0140) != nullptr);
    assert(strstr(dtcDescription(0x0300), "misfire") != nullptr);
    assert(dtcDescription(0xC101) != nullptr);          // U0101
    assert(dtcDescription(0x0ABC) == nullptr);          // not in table
    // PSA-specific: P1445 (FAP additive), P11A2 (injector init).
    assert(strstr(dtcDescription(0x1445), "FAP") != nullptr);
    assert(strcmp(formatDtcCode(0x11A2, b), "P11A2") == 0);
    assert(dtcDescription(0x11A2) != nullptr);
    printf("  dtc_text: J2012 decode + generic description lookup OK\n");
}

static void test_obd_mode01() {
    using namespace psa;
    // Real J1979 scalings on known raw values.
    const uint8_t rpm[] = {0x1A, 0xF8};        // (0x1AF8)/4 = 1726 rpm
    assert(findObdParam(0x0C)->decode(rpm, 2) > 1725.0f &&
           findObdParam(0x0C)->decode(rpm, 2) < 1727.0f);
    const uint8_t temp[] = {0x7B};             // 123 - 40 = 83 °C
    assert(findObdParam(0x05)->decode(temp, 1) == 83.0f);
    const uint8_t load[] = {0xFF};             // 255*100/255 = 100 %
    assert(findObdParam(0x04)->decode(load, 1) == 100.0f);
    const uint8_t trim[] = {0x80};             // (128-128)*100/128 = 0 %
    assert(findObdParam(0x06)->decode(trim, 1) == 0.0f);
    assert(findObdParam(0xAB) == nullptr);     // not a defined PID
    printf("  obd_mode01: J1979 PID decoders OK\n");
}

int main() {
    printf("psa self-check\n");
    test_dtc_text();
    test_obd_mode01();
    test_diag_shell_state();
    test_seedkey_determinism();
    test_seedkey_known_vector();
    test_isotp_single_frame();
    test_isotp_multi_frame();
    test_ecu_table();
    test_ecu_keys_lookup();
    test_diag_shell_unlock_flow();
    test_diag_shell_unlock_uds_flow();
    test_live_data_decoders();
    test_actuator_req_builders();
    test_diag_shell_live_data_and_actuator();
    test_multi_param_live_polling();
    test_srecord_parser();
    test_flash_checksum();
    test_flash_engine_uds();
    test_isotp_tx_flow_control();
    test_isotp_rx_flow_control_params();
    test_shell_reports_transmit_failure();
    test_isotp_out_of_order_rejected();
    test_isotp_malformed_frames_rejected();
    test_flash_shell_begin();
    test_flash_shell_srecord_staging();
    test_flash_srecord_bounds();
    test_pin_override_flow();
    test_can_sniffer_count_mode();
    test_can_sniffer_hold_mode();
    test_can_sniffer_sweep_mode();
    test_can_sniffer_climate_scenario();
    printf("all checks passed\n");
    return 0;
}
