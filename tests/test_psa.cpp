// Host self-check for the pure protocol logic. No Pico SDK, no hardware.
// Compile and run:
//   g++ -std=c++17 -Iinclude -DHOST_TEST tests/test_psa.cpp src/isotp.cpp -o test_psa && ./test_psa
//   (clang++ works too). Returns 0 on success, non-zero on the first failed assert.
//
// Covers the two pieces of non-trivial logic in the project:
//   1. PSA seed/key algorithm  (determinism + a known pair)
//   2. ISO-15765-2 transport   (single + multi-frame round-trip)
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
#include "psa/flash_engine.hpp"
#include <vector>

namespace psa {
std::vector<CanFrame> g_sent_frames;

McpError CanManager::send(Bus b, const CanFrame& f) {
    (void)b;
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

    // 4. Trace command
    psa::g_sent_frames.clear();
    shell.feedCommandLine("trace");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(psa::g_sent_frames.size() == 2);
    assert(psa::g_sent_frames[0].data[0] == 0x10);
    assert(psa::g_sent_frames[0].data[1] == 12);
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

    // 3. Write command
    psa::g_sent_frames.clear();
    shell.feedCommandLine("write 2901 FD 00 00 00 01 01 01");
    assert(shell.state() == psa::DiagShell::State::WaitingResponse);
    assert(psa::g_sent_frames.size() == 2);
    assert(psa::g_sent_frames[0].data[0] == 0x10); // First Frame PCI
    assert(psa::g_sent_frames[0].data[1] == 10);   // PDU length 10
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

    // Flash begin should send erase request
    psa::g_sent_frames.clear();
    shell.feedCommandLine("flash begin");
    assert(!psa::g_sent_frames.empty());
    // KWP erase: 31 81 81 F0 5A
    assert(psa::g_sent_frames.back().data[1] == 0x31);
    assert(psa::g_sent_frames.back().data[2] == 0x81);
    printf("  flash_shell: begin sends erase request OK\n");
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

    psa::g_sent_frames.clear();
    shell.feedCommandLine("flash begin");
    // Feed erase response -> auto-advances to RequestDownload, sends 0x34
    resp.data[0] = 1;
    resp.data[1] = 0x70;
    shell.feedDiagFrame(resp);
    assert(!psa::g_sent_frames.empty());
    assert(psa::g_sent_frames.back().data[1] == 0x34);
    // Feed download positive response -> auto-advances to TransferData
    resp.data[0] = 1;
    resp.data[1] = 0x76;
    psa::g_sent_frames.clear();
    shell.feedDiagFrame(resp);
    // Now in TransferData, auto-advance sends nothing (no staged records yet)
    assert(psa::g_sent_frames.empty());

    // Stage a valid S3 record
    shell.feedCommandLine("flash S30A001000001122334455E6");
    printf("  flash_shell: S-record staging OK\n");
}

int main() {
    printf("psa self-check\n");
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
    test_srecord_parser();
    test_flash_checksum();
    test_flash_engine_uds();
    test_isotp_out_of_order_rejected();
    test_isotp_malformed_frames_rejected();
    test_flash_shell_begin();
    test_flash_shell_srecord_staging();
    test_pin_override_flow();
    printf("all checks passed\n");
    return 0;
}
