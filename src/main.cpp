#include "pico/stdlib.h"
#include <cstdio>
#include "psa/can_manager.hpp"
#include "psa/isotp.hpp"
#include "psa/psa_protocol.hpp"
#include "psa/diag_shell.hpp"
#include "psa/wifi_server.hpp"

namespace {

void decode_sniffed(psa::Bus bus, const psa::CanFrame& f) {
    // Minimal CAN2004 decoder — the known IDs from docs section 2.
    switch (f.id) {
    case 0x0B6: if (f.dlc >= 4) {  // HS: RPM + speed
        uint16_t rpm   = ((f.data[0] << 8) | f.data[1]);
        uint16_t speed = ((f.data[2] << 8) | f.data[3]);
        printf("[HS] 0B6 rpm=%.1f speed=%.2fkmh\n", rpm * 0.125f, speed * 0.01f);
    } break;
    case 0x208: if (f.dlc >= 5) {  // HS: Engine RPM + throttle + brake status
        uint16_t rpm = ((f.data[0] << 8) | f.data[1]);
        float throttle = f.data[3] * 0.5f;
        bool brake = (f.data[4] >> 1) & 1;
        printf("[HS] 208 rpm=%.1f throttle=%.1f%% brake=%d\n", rpm * 0.125f, throttle, brake);
    } break;
    case 0x488: if (f.dlc >= 8) {  // HS: Coolant, oil, intake temps
        int coolant_temp = (int8_t)f.data[0] - 40;
        int oil_temp = (int8_t)f.data[5] - 40;
        int intake_temp = (int8_t)f.data[7] - 40;
        printf("[HS] 488 coolant=%dC oil=%dC intake=%dC\n", coolant_temp, oil_temp, intake_temp);
    } break;
    case 0x0E6: if (f.dlc >= 6) {  // HS: ABS status + battery voltage
        bool abs_fault = (f.data[0] >> 6) & 1;
        bool abs_active = (f.data[0] >> 5) & 1;
        bool low_fluid = (f.data[0] >> 1) & 1;
        float voltage = f.data[5] + 7.0f;
        printf("[HS] 0E6 abs_fault=%d abs_active=%d fluid_low=%d batt_abs=%.1fV\n", abs_fault, abs_active, low_fluid, voltage);
    } break;
    case 0x0F6: if (f.dlc >= 6) {  // LS: ignition + ext temp
        bool ign = f.data[0] > 128;
        int temp = (f.data[5] >> 1) - 40;
        printf("[LS] 0F6 ign=%d ext_temp=%dC\n", ign, temp);
    } break;
    case 0x161: if (f.dlc >= 7) {  // LS: Gauges (oil temp, fuel level, oil level)
        int oil_temp = (int8_t)f.data[2] - 40;
        uint8_t fuel = f.data[3];
        uint8_t oil_level = f.data[6];
        printf("[LS] 161 oil_temp=%dC fuel=%u%% oil_lvl=%u%%\n", oil_temp, fuel, oil_level);
    } break;
    case 0x0E8: if (f.dlc >= 2) {  // LS: Doors + parking brake
        bool fl = (f.data[0] >> 3) & 1;
        bool fr = (f.data[0] >> 4) & 1;
        bool rl = (f.data[0] >> 1) & 1;
        bool rr = (f.data[0] >> 2) & 1;
        bool bonnet = f.data[0] & 1;
        bool trunk = (f.data[1] >> 7) & 1;
        bool handbrake = (f.data[1] >> 1) & 1;
        printf("[LS] 0E8 doors(fl/fr/rl/rr/b/t)=%d/%d/%d/%d/%d/%d handbrake=%d\n", fl, fr, rl, rr, bonnet, trunk, handbrake);
    } break;
    case 0x3A7: if (f.dlc >= 6) {  // LS: Maintenance status
        uint16_t km = ((f.data[2] << 8) | f.data[3]) * 20;
        uint16_t days = ((f.data[4] << 8) | f.data[5]);
        printf("[LS] 3A7 maint_km=%ukm maint_days=%udays\n", km, days);
    } break;
    case 0x0A2: if (f.dlc >= 2) {  // LS: Steering wheel buttons (C4/C5 Mk1 FL layout)
        bool menu = (f.data[1] >> 3) & 1;
        bool mode = (f.data[1] >> 2) & 1;
        bool esc  = (f.data[1] >> 4) & 1;
        bool ok   = (f.data[1] >> 5) & 1;
        printf("[LS] 0A2 steering_wheel buttons: menu=%d mode=%d esc=%d ok=%d\n", menu, mode, esc, ok);
    } break;
    case 0x036: if (f.dlc >= 4) {  // LS: economy mode + brightness
        printf("[LS] 036 econ=%d bright=%u\n", (f.data[2] >> 7) & 1, f.data[3]);
    } break;
    case 0x168: printf("[HS] 168 instrument-panel/alerts\n"); break;
    case 0x1D0: printf("[LS] 1D0 climate\n"); break;
    case 0x21F: printf("[LS] 21F wheel-btn 0x%02X\n", f.data[0]); break;
    default: break;  // sniffer: unknown frames silently ignored in MVP
    }
}

} // namespace

int main() {
    stdio_init_all();

    psa::DualCanPins pins;
    psa::CanManager can;
    // Set listen_only to false to enable transmission for active diagnostics
    bool can_ok = can.init(pins, /*listen_only=*/false);
    if (!can_ok) {
        // Don't halt: returning from main() hits the SDK's default _exit(),
        // which spins on __breakpoint() with no debugger attached — that wedges
        // the chip badly enough to also kill USB enumeration. A diagnostic tool
        // with dead CAN hardware should still boot Wi-Fi/USB so the failure is
        // visible and the ECU functions remain reachable once wiring is fixed.
        printf("CAN init failed (no MCP2515 response) - continuing without CAN.\n");
    } else {
        printf("Citroen C5 interface up. HS=500k spi0, LS=125k spi1.\n");
    }

    psa::DiagShell shell;
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

    psa::CanFrame f;
    while (true) {
        shell.poll();
        wifi.poll();

        if (can.hasRx(psa::Bus::HighSpeed)) {
            if (can.read(psa::Bus::HighSpeed, f) == psa::McpError::Ok) {
                if (!shell.feedDiagFrame(f)) {
                    if (shell.sniffEnabled()) decode_sniffed(psa::Bus::HighSpeed, f);
                }
            }
        }
        if (can.hasRx(psa::Bus::LowSpeed)) {
            if (can.read(psa::Bus::LowSpeed, f) == psa::McpError::Ok) {
                if (!shell.feedDiagFrame(f)) {
                    if (shell.sniffEnabled()) decode_sniffed(psa::Bus::LowSpeed, f);
                }
            }
        }
        tight_loop_contents();
    }
}
