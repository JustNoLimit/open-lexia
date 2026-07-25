#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include <cstdio>
#include "psa/can_manager.hpp"
#include "psa/isotp.hpp"
#include "psa/psa_protocol.hpp"
#include "psa/diag_shell.hpp"
#include "psa/wifi_server.hpp"

namespace {

// Frames pulled from one bus per main-loop pass. Enough to empty the MCP2515's
// two receive buffers plus a burst, without letting a busy bus starve Wi-Fi/USB.
constexpr int kMaxRxPerPass = 8;

void decode_sniffed(psa::Bus bus, const psa::CanFrame& f) {
    // CAN2004 decoder for Citroen C5 Mk1 FL.
    // CAN IDs and byte layouts from cross-ref: ludwig-v/arduino-psa-comfort-can-adapter,
    // prototux/PSA-CAN-RE, Melnik-Alex/PSA_CAN, autowp, JustNoLimit/citroen-can.
    // IDs marked (unverified) have only one source and need gsniff confirmation.
    (void)bus;
    switch (f.id) {
    // ===== CAN-HS (500 kbps) =====
    case 0x0B6: if (f.dlc >= 4) {
        uint16_t rpm   = (f.data[0] << 8) | f.data[1];
        uint16_t speed = (f.data[2] << 8) | f.data[3];
        printf("[HS] 0B6 rpm=%.1f speed=%.2fkmh\n", rpm * 0.125f, speed * 0.01f);
    } break;
    case 0x0E6: if (f.dlc >= 7) {
        bool abs_fault  = (f.data[0] >> 6) & 1;
        bool abs_active = (f.data[0] >> 5) & 1;
        bool low_fluid  = (f.data[0] >> 1) & 1;
        float voltage   = f.data[5] + 7.0f;
        bool stt        = (f.data[6] >> 7) & 1;
        bool slope      = (f.data[6] >> 6) & 1;
        bool emerg_brake= (f.data[6] >> 5) & 1;
        printf("[HS] 0E6 abs_fault=%d active=%d fluid_low=%d stt=%d slope=%d emerg_brake=%d batt_abs=%.1fV\n",
               abs_fault, abs_active, low_fluid, stt, slope, emerg_brake, voltage);
    } break;
    case 0x120: if (f.dlc >= 2) {
        printf("[HS] 120 alerts oil=%02X temp=%02X brake=%02X esp=%02X\n",
               f.data[0], f.data[1], f.data[2], f.data[3]);
    } break;
    case 0x128: if (f.dlc >= 7) {
        static const char* gears[] = {"P","R","N","D","6","5","4","3","2","1"};
        uint8_t gi = (f.data[6] >> 4) & 0x0F;
        const char* gear = (gi <= 9) ? gears[gi] : "?";
        bool auto_mode = ((f.data[7] >> 4) & 0x07) != 0;
        printf("[HS] 128 gear=%s auto=%d handbrake=%d\n", gear, auto_mode, (f.data[0] >> 5) & 1);
    } break;
    case 0x168: if (f.dlc >= 6) {
        printf("[HS] 168 panel: check_eng=%d stop=%d oil=%d batt=%d handbrake=%d abs=%d\n",
               (f.data[0] >> 7) & 1, (f.data[0] >> 6) & 1, (f.data[0] >> 5) & 1,
               (f.data[0] >> 4) & 1, (f.data[0] >> 3) & 1, (f.data[0] >> 2) & 1);
    } break;
    case 0x208: if (f.dlc >= 3) {
        printf("[HS] 208 gas_pedal=%.1f%% torque=%uNm\n",
               f.data[0] * 0.392f, (unsigned)((f.data[1] << 8) | f.data[2]));
    } break;
    case 0x305: if (f.dlc >= 2) {
        printf("[HS] 305 steering_angle=%d\n",
               static_cast<int>(static_cast<int16_t>((f.data[0] << 8) | f.data[1])));
    } break;
    case 0x348: if (f.dlc >= 2) {
        printf("[HS] 348 boost=%u man_temp=%dC\n", f.data[0], static_cast<int>(f.data[1]) - 40);
    } break;
    case 0x44D: if (f.dlc >= 8) {
        float fl = ((f.data[0] << 8) | f.data[1]) * 0.01f;
        float fr = ((f.data[2] << 8) | f.data[3]) * 0.01f;
        float rl = ((f.data[4] << 8) | f.data[5]) * 0.01f;
        float rr = ((f.data[6] << 8) | f.data[7]) * 0.01f;
        printf("[HS] 44D wheel_spd fl=%.1f fr=%.1f rl=%.1f rr=%.1fkmh\n", fl, fr, rl, rr);
    } break;
    case 0x468: if (f.dlc >= 1) {
        printf("[HS] 468 oil_temp=%dC\n", static_cast<int>(f.data[0]) - 40);
    } break;
    case 0x488: if (f.dlc >= 2) {
        printf("[HS] 488 coolant=%dC oil=%dC\n",
               static_cast<int>(f.data[0]) - 40, static_cast<int>(f.data[1]) - 40);
    } break;
    case 0x108: printf("[HS] 108 ecu_hb\n"); break;
    case 0x34D: printf("[HS] 34D esp_off=%d asr=%d\n", (f.data[0] >> 7) & 1, (f.data[0] >> 3) & 1); break;
    case 0x38D: if (f.dlc >= 2) {
        printf("[HS] 38D yaw_rate=%d\n",
               static_cast<int16_t>((f.data[0] << 8) | f.data[1]));
    } break;
    case 0x072: printf("[HS] 072 immo_query\n"); break;
    case 0x0A8: printf("[HS] 0A8 immo_response\n"); break;
    case 0x217: printf("[HS] 217 cluster_btn=%02X\n", f.data[0]); break;

    // ===== CAN-LS (125 kbps) =====
    case 0x036: if (f.dlc >= 4) {
        printf("[LS] 036 econ=%d bright=%u\n", (f.data[2] >> 7) & 1, f.data[3]);
    } break;
    case 0x0F6: if (f.dlc >= 7) {
        bool ign   = f.data[0] > 128;
        int coolant = static_cast<int>(f.data[1]) - 40;
        uint32_t odo = (static_cast<uint32_t>(f.data[2]) << 16) | (f.data[3] << 8) | f.data[4];
        int ext_temp = static_cast<int>(f.data[6] >> 1) - 40;
        printf("[LS] 0F6 ign=%d coolant=%dC odo=%ukm ext_temp=%dC\n", ign, coolant, odo, ext_temp);
    } break;
    case 0x1E1: if (f.dlc >= 2) {
        static const char* ign_st[] = {"off","ACC","on","cranking"};
        uint8_t k = f.data[0] & 0x0F;
        k = (k == 0x00) ? 0 : (k == 0x01) ? 1 : (k == 0x04) ? 2 : (k == 0x08) ? 3 : 0;
        printf("[LS] 1E1 ignition=%s econ=%d\n", ign_st[k], f.data[1] != 0);
    } break;
    case 0x1D0: if (f.dlc >= 7) {
        printf("[LS] 1D0 climate mode=%02X fan=%s pos=%02X demist=%d recycle=%d tempL=%u tempR=%u%s\n",
               f.data[0], (f.data[2] == 0x0F ? "off" : "on"), f.data[3],
               f.data[4] == 0x10, f.data[4] == 0x30,
               f.data[5], f.data[6], (f.data[5] == f.data[6]) ? " MONO" : "");
    } break;
    case 0x21F: if (f.dlc >= 1) {
        printf("[LS] 21F wheel_btn src=%d vol=%d seek=%d\n",
               (f.data[0] >> 4) & 1, (f.data[0] >> 2) & 1, (f.data[0] >> 6) & 1);
    } break;
    case 0x0A2: if (f.dlc >= 2) {
        printf("[LS] 0A2 steering_wheel menu=%d mode=%d esc=%d ok=%d\n",
               (f.data[1] >> 3) & 1, (f.data[1] >> 2) & 1, (f.data[1] >> 4) & 1, (f.data[1] >> 5) & 1);
    } break;
    case 0x1A1: if (f.dlc >= 2) {
        printf("[LS] 1A1 doors(fl/fr/rl/rr/trunk/bonnet)=%d/%d/%d/%d/%d/%d"
               " lights(park/low/high/turnL/turnR)=%d/%d/%d/%d/%d\n",
               f.data[0] & 1, (f.data[0] >> 1) & 1, (f.data[0] >> 2) & 1,
               (f.data[0] >> 3) & 1, (f.data[0] >> 4) & 1, (f.data[0] >> 5) & 1,
               f.data[1] & 1, (f.data[1] >> 1) & 1, (f.data[1] >> 2) & 1,
               (f.data[1] >> 3) & 1, (f.data[1] >> 4) & 1);
    } break;
    case 0x221: if (f.dlc >= 5) {
        printf("[LS] 221 range=%ukm\n", (unsigned)((f.data[3] << 8) | f.data[4]));
    } break;
    case 0x228: if (f.dlc >= 2) {
        printf("[LS] 228 clock=%02u:%02u\n", f.data[0] & 0x1F, f.data[1] & 0x3F);
    } break;
    case 0x336: if (f.dlc >= 3) {
        printf("[LS] 336 vin1=%.3s\n", reinterpret_cast<const char*>(f.data));
    } break;
    case 0x3B6: if (f.dlc >= 6) {
        printf("[LS] 3B6 vin2=%.6s\n", reinterpret_cast<const char*>(f.data));
    } break;
    case 0x2B6: if (f.dlc >= 8) {
        printf("[LS] 2B6 vin3=%.8s\n", reinterpret_cast<const char*>(f.data));
    } break;
    case 0x3A7: if (f.dlc >= 7) {
        printf("[LS] 3A7 maint_km=%ukm maint_days=%udays\n",
               (unsigned)((f.data[3] << 8) | f.data[4]) * 20,
               (unsigned)((f.data[5] << 8) | f.data[6]));
    } break;
    case 0x3E5: printf("[LS] 3E5 emf_menu=%02X\n", f.data[0]); break;
    case 0x2A5: if (f.dlc >= 8) printf("[LS] 2A5 rds=%.8s\n", reinterpret_cast<const char*>(f.data)); break;
    case 0x225: printf("[LS] 225 tuner=%02X\n", f.data[0]); break;
    case 0x125: printf("[LS] 125 cd=%.4s\n", reinterpret_cast<const char*>(f.data)); break;
    case 0x1A8: if (f.dlc >= 2) printf("[LS] 1A8 cruise_set=%u\n", f.data[1]); break;
    case 0x0D6: printf("[LS] 0D6 gearbox_pos=%02X\n", f.data[0]); break;
    case 0x220: printf("[LS] 220 doors_raw=%02X\n", f.data[0]); break;
    case 0x227: printf("[LS] 227 leds=%02X\n", f.data[0]); break;
    case 0x5E5: printf("[LS] 5E5 display_ver=%02X%02X\n", f.data[0], f.data[1]); break;
    case 0x1A5: printf("[LS] 1A5 volume=%u\n", f.data[0]); break;
    case 0x0A4: printf("[LS] 0A4 radio_status=%02X\n", f.data[0]); break;
    case 0x165: printf("[LS] 165 sound=%02X\n", f.data[0]); break;
    case 0x161: if (f.dlc >= 7) {
        printf("[LS] 161 (unverified) oil_temp=%dC fuel=%u%% oil_lvl=%u\n",
               static_cast<int>(f.data[2]) - 40, f.data[3] * 100 / 255, f.data[6]);
    } break;
    case 0x0E8: if (f.dlc >= 2) {
        printf("[LS] 0E8 (unverified) doors(fl/fr/rl/rr/b/t)=%d/%d/%d/%d/%d/%d handbrake=%d\n",
               (f.data[0] >> 3) & 1, (f.data[0] >> 4) & 1,
               (f.data[0] >> 1) & 1, (f.data[0] >> 2) & 1,
               f.data[0] & 1, (f.data[1] >> 7) & 1, (f.data[1] >> 1) & 1);
    } break;
    case 0x2E1: printf("[LS] 2E1 (unverified) val=%02X\n", f.data[0]); break;
    case 0x361: printf("[LS] 361 (unverified) config=%02X\n", f.data[0]); break;
    default: break;
    }
}

} // namespace

// DiagShell alone is ~26 KB (staged flash records, ISO-TP buffers, the sniffer
// table). Core 0's stack is 2 KB — __StackTop 0x20082000, __StackBottom
// 0x20081800 — so holding these as locals made main() open with
// `sub sp, #74752` and run tens of kilobytes below the stack it was given.
// It happened not to corrupt anything only because that SRAM was unused.
// They belong in .bss.
psa::Mcp2515::Pins g_pins{spi0, 2, 3, 4, 5, 6};
psa::CanManager      g_can;
psa::DiagShell       g_shell;

int main() {
    stdio_init_all();

    psa::Mcp2515::Pins& pins = g_pins;
    psa::CanManager& can = g_can;
    bool can_ok = can.init(pins, /*listen_only=*/false);
    if (!can_ok) {
        printf("CAN init failed (no MCP2515 response) - continuing without CAN.\n");
    } else {
        printf("Citroen C5 interface up. MCP2515 on spi0 GP2-6 (default 500k). Use 'gsniff rate ls' for 125k.\n");
    }

    psa::DiagShell& shell = g_shell;
    shell.init(&can);

    // Bring up Wi-Fi Access Point and HTTP/SSE server.
    // SSID: "Citroen-Diag"  Password: "12345678"  → open http://192.168.4.1
    psa::WifiServer& wifi = psa::WifiServer::instance();
    if (wifi.init(&shell)) {
        // The shell logs with printf (330+ call sites), not an explicit sink, so
        // capture stdout centrally and stream every line to the SSE dashboard.
        wifi.initStdioCapture();
        printf("[WIFI] Dashboard: connect to Wi-Fi 'Citroen-Diag' then open http://192.168.4.1\n");
    }

    // Everything that can block is now bounded, so arm the watchdog. 8 s is well
    // clear of the 5 s ISO-TP response timeout, so only a genuine wedge trips it.
    // Armed after Wi-Fi is up: cyw43 firmware upload alone can take seconds.
    if (watchdog_caused_reboot()) printf("[SYS] Recovered from a watchdog reset.\n");
    watchdog_enable(8000, /*pause_on_debug=*/true);

    psa::CanFrame f;
    while (true) {
        watchdog_update();
        tud_task();  // service USB CDC every loop; the SDK's background task can
                     // starve under cyw43 threadsafe_background, stalling shell input
        shell.poll();
        wifi.poll();

        // Drain frames from the single MCP2515. The controller holds two
        // receive buffers; draining one per pass meant a single slow lwIP/printf
        // call dropped consecutive frames of a long ECU reply and failed the
        // whole reassembly. Bounded so a saturated bus never starves Wi-Fi/USB.
        // All frames are tagged with the current bus (HS/LS) — whichever rate
        // the MCP2515 is configured for via `gsniff rate`.
        for (int i = 0; i < kMaxRxPerPass && can.hasRx(psa::Bus::HighSpeed); ++i) {
            if (can.read(psa::Bus::HighSpeed, f) != psa::McpError::Ok) break;
            if (!shell.feedDiagFrame(f)) {
                if (shell.gsniffActive()) shell.feedCaptureFrame(psa::Bus::HighSpeed, f);
                else if (shell.sniffEnabled()) decode_sniffed(psa::Bus::HighSpeed, f);
            }
        }
        tight_loop_contents();
    }
}
