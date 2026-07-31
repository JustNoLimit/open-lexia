// DTC text dictionary — turns a raw fault code into "P0140" + a description.
// Two layers:
//   1. formatDtcCode(): algorithmic SAE J2012 / ISO 15031-6 decode of the 2-byte
//      code into its P/C/B/U + 4-digit form. No table, correct for EVERY code.
//   2. dtcDescription(): lookup of common *standardised generic* OBD-II codes
//      plus a curated set of *PSA manufacturer-specific* P1xxx/P2xxx codes
//      (Diagbox meanings for HDi EDC16/DCM ECUs — see the caveat by that block).
//      Codes not in the table still get their P-code from layer 1 and simply
//      show no description (honest — not guessed).
// Pure, host-compilable, no hardware deps.
#pragma once
#include <cstdint>

namespace psa {

// Decode the 2-byte J2012 code (first two DTC bytes; the UDS 3rd byte is the
// failure-type byte, passed separately) into out[] as e.g. "P0140".
// out must hold at least 6 bytes. Returns out.
inline const char* formatDtcCode(uint16_t code, char* out) {
    static const char kCat[4]   = {'P', 'C', 'B', 'U'};
    static const char kHex[17]  = "0123456789ABCDEF";
    out[0] = kCat[(code >> 14) & 0x3];   // bits 15-14: category letter
    out[1] = static_cast<char>('0' + ((code >> 12) & 0x3)); // bits 13-12: 0..3
    out[2] = kHex[(code >> 8) & 0xF];    // bits 11-8
    out[3] = kHex[(code >> 4) & 0xF];    // bits 7-4
    out[4] = kHex[code & 0xF];           // bits 3-0
    out[5] = '\0';
    return out;
}

struct DtcText { uint16_t code; const char* desc; };

// Standardised generic OBD-II definitions (SAE J2012). Real, not fabricated.
// Weighted toward what a C5 Mk1 HDi/petrol actually reports. Extend as codes
// are observed on the real vehicle — unknown codes degrade gracefully.
inline constexpr DtcText kDtcText[] = {
    // Fuel & air metering
    {0x0100, "Mass air flow (MAF) circuit"},
    {0x0101, "MAF circuit range/performance"},
    {0x0102, "MAF circuit low input"},
    {0x0103, "MAF circuit high input"},
    {0x0105, "MAP/barometric pressure circuit"},
    {0x0106, "MAP circuit range/performance"},
    {0x0107, "MAP circuit low input"},
    {0x0108, "MAP circuit high input"},
    {0x0110, "Intake air temp (IAT) circuit"},
    {0x0111, "IAT circuit range/performance"},
    {0x0112, "IAT circuit low input"},
    {0x0113, "IAT circuit high input"},
    {0x0115, "Coolant temp (ECT) circuit"},
    {0x0116, "ECT circuit range/performance"},
    {0x0117, "ECT circuit low input"},
    {0x0118, "ECT circuit high input"},
    {0x0120, "Throttle/pedal position sensor A"},
    {0x0121, "Throttle/pedal A range/performance"},
    {0x0122, "Throttle/pedal A low input"},
    {0x0123, "Throttle/pedal A high input"},
    {0x0128, "Coolant thermostat below regulating temp"},
    // Oxygen sensors
    {0x0130, "O2 sensor circuit B1S1"},
    {0x0131, "O2 sensor low voltage B1S1"},
    {0x0132, "O2 sensor high voltage B1S1"},
    {0x0133, "O2 sensor slow response B1S1"},
    {0x0134, "O2 sensor no activity B1S1"},
    {0x0135, "O2 sensor heater circuit B1S1"},
    {0x0136, "O2 sensor circuit B1S2"},
    {0x0140, "O2 sensor no activity B1S2"},
    {0x0141, "O2 sensor heater circuit B1S2"},
    // Fuel system / rail
    {0x0170, "Fuel trim malfunction bank 1"},
    {0x0171, "System too lean bank 1"},
    {0x0172, "System too rich bank 1"},
    {0x0180, "Fuel temp sensor A circuit"},
    {0x0182, "Fuel temp sensor A low input"},
    {0x0183, "Fuel temp sensor A high input"},
    {0x0190, "Fuel rail pressure sensor circuit"},
    {0x0191, "Fuel rail pressure range/performance"},
    {0x0192, "Fuel rail pressure low input"},
    {0x0193, "Fuel rail pressure high input"},
    // Injectors
    {0x0200, "Injector circuit open"},
    {0x0201, "Injector circuit cylinder 1"},
    {0x0202, "Injector circuit cylinder 2"},
    {0x0203, "Injector circuit cylinder 3"},
    {0x0204, "Injector circuit cylinder 4"},
    // Turbo / boost
    {0x0234, "Turbocharger overboost"},
    {0x0235, "Turbo boost sensor A circuit"},
    {0x0243, "Turbo wastegate solenoid A"},
    {0x0299, "Turbocharger underboost"},
    // Misfire & ignition
    {0x0300, "Random/multiple cylinder misfire"},
    {0x0301, "Cylinder 1 misfire"},
    {0x0302, "Cylinder 2 misfire"},
    {0x0303, "Cylinder 3 misfire"},
    {0x0304, "Cylinder 4 misfire"},
    {0x0335, "Crankshaft position sensor A circuit"},
    {0x0336, "Crankshaft position A range/performance"},
    {0x0340, "Camshaft position sensor A circuit"},
    {0x0341, "Camshaft position A range/performance"},
    // Glow plugs (diesel)
    {0x0380, "Glow plug/heater circuit A"},
    {0x0381, "Glow plug indicator circuit"},
    {0x0670, "Glow plug control module circuit"},
    // EGR & emissions
    {0x0401, "EGR flow insufficient"},
    {0x0402, "EGR flow excessive"},
    {0x0403, "EGR control circuit"},
    {0x0404, "EGR range/performance"},
    {0x0405, "EGR sensor A low"},
    {0x0420, "Catalyst efficiency below threshold B1"},
    {0x0463, "Fuel level sensor circuit high input"},
    {0x0470, "Exhaust pressure sensor circuit"},
    {0x0471, "Exhaust pressure sensor range/performance"},
    {0x0480, "Cooling fan 1 control circuit"},
    // DPF (HDi)
    {0x2002, "DPF efficiency below threshold B1"},
    {0x2463, "DPF restriction - soot accumulation"},
    // Vehicle speed / idle / voltage
    {0x0500, "Vehicle speed sensor A"},
    {0x0501, "Vehicle speed sensor A range/performance"},
    {0x0505, "Idle air control system"},
    {0x0560, "System voltage malfunction"},
    {0x0562, "System voltage low"},
    {0x0563, "System voltage high"},
    {0x0571, "Cruise control/brake switch A circuit"},
    // Control module
    {0x0600, "Serial communication link"},
    {0x0601, "Internal control module checksum error"},
    {0x0605, "Internal control module ROM error"},
    {0x0606, "Control module processor fault"},
    // Transmission
    {0x0700, "Transmission control system"},
    {0x0704, "Clutch switch input circuit"},
    {0x0705, "Transmission range sensor circuit"},
    {0x0715, "Input/turbine speed sensor circuit"},
    {0x0720, "Output speed sensor circuit"},
    // Chassis (C-codes) — PSA-specific
    {0x5301, "Brake pedal switch / ABS pressure sensor coherence"}, // C1301
    // Network (U-codes)
    {0xC001, "High-speed CAN communication bus"},   // U0001
    {0xC100, "Lost communication with ECM/PCM A"},  // U0100
    {0xC101, "Lost communication with TCM"},        // U0101
    {0xC121, "Lost communication with ABS module"}, // U0121
    {0xC126, "Lost communication with steering angle sensor"}, // U0126
    {0xC155, "Lost communication with instrument cluster"},    // U0155
    {0xC415, "Invalid data from ABS control module"},          // U0415

    // --- PSA manufacturer-specific (P1xxx/P2xxx) ------------------------------
    // Diagbox meanings for the common EDC16/DCM HDi ECUs. IMPORTANT: PSA remaps
    // P1xxx/P2xxx per engine-ECU generation — the SAME code means different
    // things on EDC15 vs EDC16 vs DCM3.5. Treat these as a strong hint, not
    // gospel; confirm against the ECU. Where two sources disagreed (P1193-P1198)
    // the more granular wording is used. P-code hex == the 4-digit number.
    // Diesel injection / rail pressure
    {0x1101, "Atmospheric pressure sensor circuit"},
    {0x1102, "Needle lift sensor circuit"},
    {0x1103, "Slider position sensor circuit"},
    {0x1104, "Variable-geometry aero solenoid valve circuit"},
    {0x1105, "Injector ventilation solenoid valve circuit"},
    {0x1113, "Rail pressure too low"},
    {0x1114, "Injection cut-off test on engine stop (fuel cut)"},
    {0x1163, "Injector latency-time adjustment problem"},
    {0x1164, "Fuel pressure drift from rail pressure before start"},
    {0x1165, "Pressure differential too low"},
    {0x1166, "Rail pressure too high"},
    {0x1167, "Fuel pressure regulation: value incorrect"},
    {0x1169, "Injector voltage converter: value incorrect"},
    {0x1193, "Injector control: flow too low"},
    {0x1194, "Injector control: injector jammed closed"},
    {0x1195, "Injector control: injector stuck open"},
    {0x1197, "Injector control: harness/injector/ECU power stage"},
    {0x1198, "Fuel flow regulation electrovalve: flow too low"},
    {0x11A2, "Injector initialisation: programming not done"},
    {0x1210, "Fuel pressure regulation electrovalve: open circuit"},
    {0x1282, "Gas pressure fault"},
    {0x1283, "Solenoid valve circuit"},
    {0x1302, "Needle lift / engine speed correlation fault"},
    // EGR / exhaust / turbo
    {0x1420, "Exhaust temperature sensor 2: intermittent circuit"},
    {0x1429, "Differential exhaust pressure: reaction time too long"},
    {0x1454, "EGR control high"},
    {0x1455, "EGR control low"},
    {0x1459, "EGR valve position controller: performance"},
    {0x1471, "EGR throttle control electrovalve: throttle open"},
    {0x1491, "EGR strategy: correction too large"},
    {0x1492, "EGR strategy: correction too small"},
    {0x1493, "EGR strategy: correction outside range"},
    // Particulate filter (FAP / Eolys additive)
    {0x1445, "Max additive threshold in FAP reached"},
    {0x1446, "FAP additive system: insufficient additive in tank"},
    {0x1447, "Particulate filter clogged or pierced"},
    {0x1490, "FAP regeneration driving conditions not met"},
    // Pre/post heating
    {0x1403, "Additional heating circuit 1 fault"},
    {0x1404, "Additional heating circuit 2 fault"},
    // Network / ECU internal / config
    {0x1500, "CAN: BSI info, gearbox locked in reverse"},
    {0x1510, "Auto gearbox ECU: diagnostic LED request"},
    {0x1536, "Brake switch signal: coherence"},
    {0x1607, "CAN: vehicle speed limiter error"},
    {0x1613, "Configuration fault: ECU not configured"},
    {0x1614, "Accelerator pedal resistance-point sensor: no signal"},
    {0x1641, "Injection ECU: power stage, injector control"},
    {0x1693, "Controlled start/stop: requests absent or invalid"},
    {0x1704, "Brake switch: consistency between 2 signals"},
    // P2xxx PSA-specific
    {0x2031, "Cat downstream temp signal: implausible"},
    {0x2032, "Cat downstream temp signal: short to positive"},
    {0x2033, "Cat downstream temp signal: short to earth"},
    {0x2084, "Cat downstream temp signal: coherence in evolution"},
    {0x2137, "Accelerator pedal: coherence with other pedal signal"},
    {0x2144, "Electric EGR valve control: short to earth/positive"},
    {0x2199, "Intake air temp signal: plausibility"},
    {0x2299, "Accelerator pedal signal: coherence"},
    {0x2408, "Additive system: fuel tank cap sensor signal"},
    {0x2670, "Sensor 5V supply fault"},
    {0x2671, "Sensor 5V supply fault (2)"},
};

inline constexpr size_t kDtcTextCount = sizeof(kDtcText) / sizeof(kDtcText[0]);

// Description for a 2-byte J2012 code, or nullptr if not in the generic table.
inline const char* dtcDescription(uint16_t code) {
    for (size_t i = 0; i < kDtcTextCount; ++i)
        if (kDtcText[i].code == code) return kDtcText[i].desc;
    return nullptr;
}

} // namespace psa
