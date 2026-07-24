// Live Data Parameters and Decoders.
// Reference: Phase 3 of PSA CAN Interface Roadmap.
#pragma once
#include <cstdint>
#include <cstddef>
#include "psa/psa_protocol.hpp"

namespace psa {

struct LiveDataParam {
    uint16_t id;
    const char* name;
    const char* unit;
    float (*decode)(const uint8_t* data, size_t len);
};

// Decoder functions
inline float decodeEngineRpm(const uint8_t* data, size_t len) {
    if (len < 2) return 0.0f;
    return static_cast<float>((data[0] << 8) | data[1]) * 0.125f;
}

inline float decodeCoolantTemp(const uint8_t* data, size_t len) {
    if (len < 1) return 0.0f;
    return static_cast<float>(data[0]) - 40.0f;
}

inline float decodeBatteryVoltage(const uint8_t* data, size_t len) {
    if (len < 1) return 0.0f;
    return static_cast<float>(data[0]) * 0.1f + 7.0f;
}

inline float decodeThrottlePosition(const uint8_t* data, size_t len) {
    if (len < 1) return 0.0f;
    return static_cast<float>(data[0]) * 0.5f;
}

// UDS Live Data DIDs
inline constexpr LiveDataParam kUdsParams[] = {
    {0x100A, "Engine RPM", "rpm", decodeEngineRpm},
    {0x100B, "Coolant Temp", "°C", decodeCoolantTemp},
    {0x100C, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x100D, "Throttle Position", "%", decodeThrottlePosition}
};

// KWP Live Data Local IDs
inline constexpr LiveDataParam kKwpParams[] = {
    {0x01, "Engine RPM", "rpm", decodeEngineRpm},
    {0x02, "Coolant Temp", "°C", decodeCoolantTemp},
    {0x03, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x04, "Throttle Position", "%", decodeThrottlePosition}
};

inline const LiveDataParam* findParam(Protocol p, uint16_t id) {
    if (p == Protocol::UDS) {
        for (const auto& param : kUdsParams) {
            if (param.id == id) return &param;
        }
    } else { // KWP_HAB or KWP_IS
        for (const auto& param : kKwpParams) {
            if (param.id == id) return &param;
        }
    }
    return nullptr;
}

// --- Additional decoder functions -------------------------------------------

inline float decodeRawByte(const uint8_t* data, size_t len) {
    if (len < 1) return 0.0f;
    return static_cast<float>(data[0]);
}

inline float decodeFuelLevel(const uint8_t* data, size_t len) {
    if (len < 1) return 0.0f;
    return static_cast<float>(data[0]) * 100.0f / 255.0f;
}

inline float decodeOilTemp(const uint8_t* data, size_t len) {
    if (len < 1) return 0.0f;
    return static_cast<float>(data[0]) - 40.0f;
}

// Odometer / mileage: a plain big-endian counter (km), not an RPM-scaled value.
// ponytail: 24-bit read is the common PSA layout; widen if a 4-byte zone shows up.
inline float decodeOdometer(const uint8_t* data, size_t len) {
    if (len < 3) return (len >= 1) ? static_cast<float>(data[0]) : 0.0f;
    return static_cast<float>((data[0] << 16) | (data[1] << 8) | data[2]);
}

// --- BSI Supply / Voltage measurement parameters (KWP local IDs) ------------

inline constexpr LiveDataParam kSupplyParams[] = {
    {0x0C, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x0D, "Economy Mode", "", decodeRawByte},
    {0x0E, "BSI Operating Mode", "", decodeRawByte},
    {0x0F, "Ignition Status", "", decodeRawByte},
    {0x10, "Alternator Status", "", decodeRawByte},
    {0x11, "Vehicle Electrical Status", "", decodeRawByte},
};

// --- AC / Climate parameters (KWP local IDs) --------------------------------

inline constexpr LiveDataParam kACParams[] = {
    {0x20, "AC Compressor Status", "", decodeRawByte},
    {0x21, "AC Pressure", "bar", decodeRawByte},
    {0x22, "Evaporator Temperature", "°C", decodeCoolantTemp},
    {0x23, "Outside Temperature", "°C", decodeCoolantTemp},
    {0x24, "Blower Speed", "", decodeRawByte},
};

// --- Driving parameters (KWP local IDs) -------------------------------------

inline constexpr LiveDataParam kDrivingParams[] = {
    {0x01, "Engine Speed", "rpm", decodeEngineRpm},
    {0x02, "Coolant Temperature", "°C", decodeCoolantTemp},
    {0x03, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x04, "Throttle Position", "%", decodeThrottlePosition},
    {0x05, "Fuel Level", "%", decodeFuelLevel},
    {0x06, "Oil Temperature", "°C", decodeOilTemp},
    {0x07, "Vehicle Speed", "km/h", decodeRawByte},
};

// --- Engine parameters (UDS DIDs) -------------------------------------------

inline constexpr LiveDataParam kEngineParams[] = {
    {0x100A, "Engine RPM", "rpm", decodeEngineRpm},
    {0x100B, "Coolant Temp", "°C", decodeCoolantTemp},
    {0x100C, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x100D, "Throttle Position", "%", decodeThrottlePosition},
    {0x100E, "Fuel Level", "%", decodeFuelLevel},
    {0x100F, "Oil Temperature", "°C", decodeOilTemp},
};

// --- Body / Comfort parameters (UDS DIDs) -----------------------------------

inline constexpr LiveDataParam kBodyParams[] = {
    {0x1010, "Interior Temperature", "°C", decodeCoolantTemp},
    {0x1011, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x1012, "Odometer", "km", decodeOdometer},
};

// --- Parameter database category grouping -----------------------------------

struct LiveDataCategory {
    const char* name;
    const LiveDataParam* params;
    size_t count;
};

inline constexpr LiveDataCategory kLiveDataCategories[] = {
    {"BSI Supply / Voltage", kSupplyParams, sizeof(kSupplyParams) / sizeof(kSupplyParams[0])},
    {"AC / Climate",         kACParams, sizeof(kACParams) / sizeof(kACParams[0])},
    {"Driving",              kDrivingParams, sizeof(kDrivingParams) / sizeof(kDrivingParams[0])},
    {"Engine",               kEngineParams, sizeof(kEngineParams) / sizeof(kEngineParams[0])},
    {"Body / Comfort",       kBodyParams, sizeof(kBodyParams) / sizeof(kBodyParams[0])},
};

inline constexpr size_t kLiveDataCategoryCount = sizeof(kLiveDataCategories) / sizeof(kLiveDataCategories[0]);

inline const LiveDataParam* findParamInCategories(uint16_t id) {
    for (size_t c = 0; c < kLiveDataCategoryCount; ++c) {
        for (size_t p = 0; p < kLiveDataCategories[c].count; ++p) {
            if (kLiveDataCategories[c].params[p].id == id)
                return &kLiveDataCategories[c].params[p];
        }
    }
    return nullptr;
}

// =============================================================================
// Standard OBD-II Mode 01 (SAE J1979) — REAL, documented PIDs and scalings.
// A 2004+ EOBD engine ECU answers these on the 500k HS bus at 0x7DF/0x7E8 via
// service 0x01. Unlike the PSA-proprietary reads above, every scaling here is
// from the public J1979 spec, so the numbers are correct once wired to a car.
// Queried by the `obd` shell command (single-frame, no session), NOT `meas`.
// =============================================================================
inline float obdLoad(const uint8_t* d, size_t n)     { return n < 1 ? 0.f : d[0] * 100.0f / 255.0f; }
inline float obdTempA40(const uint8_t* d, size_t n)  { return n < 1 ? 0.f : static_cast<float>(d[0]) - 40.0f; }
inline float obdFuelTrim(const uint8_t* d, size_t n) { return n < 1 ? 0.f : (static_cast<float>(d[0]) - 128.0f) * 100.0f / 128.0f; }
inline float obdFuelPress(const uint8_t* d, size_t n){ return n < 1 ? 0.f : d[0] * 3.0f; }               // kPa
inline float obdMap(const uint8_t* d, size_t n)      { return n < 1 ? 0.f : static_cast<float>(d[0]); }  // kPa
inline float obdRpm(const uint8_t* d, size_t n)      { return n < 2 ? 0.f : ((d[0] << 8) | d[1]) / 4.0f; }
inline float obdSpeed(const uint8_t* d, size_t n)    { return n < 1 ? 0.f : static_cast<float>(d[0]); }  // km/h
inline float obdTiming(const uint8_t* d, size_t n)   { return n < 1 ? 0.f : d[0] / 2.0f - 64.0f; }       // deg
inline float obdMaf(const uint8_t* d, size_t n)      { return n < 2 ? 0.f : ((d[0] << 8) | d[1]) / 100.0f; } // g/s
inline float obdU16(const uint8_t* d, size_t n)      { return n < 2 ? 0.f : static_cast<float>((d[0] << 8) | d[1]); }
inline float obdRailRel(const uint8_t* d, size_t n)  { return n < 2 ? 0.f : ((d[0] << 8) | d[1]) * 0.079f; }  // kPa
inline float obdRailGauge(const uint8_t* d, size_t n){ return n < 2 ? 0.f : ((d[0] << 8) | d[1]) * 10.0f; }   // kPa
inline float obdModVolt(const uint8_t* d, size_t n)  { return n < 2 ? 0.f : ((d[0] << 8) | d[1]) / 1000.0f; } // V
inline float obdFuelRate(const uint8_t* d, size_t n) { return n < 2 ? 0.f : ((d[0] << 8) | d[1]) / 20.0f; }   // L/h

// id here is the 1-byte OBD-II PID.
inline constexpr LiveDataParam kObdParams[] = {
    {0x04, "Calculated engine load",  "%",    obdLoad},
    {0x05, "Coolant temperature",     "°C",   obdTempA40},
    {0x06, "Short-term fuel trim B1", "%",    obdFuelTrim},
    {0x07, "Long-term fuel trim B1",  "%",    obdFuelTrim},
    {0x0A, "Fuel pressure",           "kPa",  obdFuelPress},
    {0x0B, "Intake MAP",              "kPa",  obdMap},
    {0x0C, "Engine RPM",              "rpm",  obdRpm},
    {0x0D, "Vehicle speed",           "km/h", obdSpeed},
    {0x0E, "Timing advance",          "°",    obdTiming},
    {0x0F, "Intake air temperature",  "°C",   obdTempA40},
    {0x10, "MAF air flow rate",       "g/s",  obdMaf},
    {0x11, "Throttle position",       "%",    obdLoad},
    {0x1F, "Run time since start",    "s",    obdU16},
    {0x21, "Distance with MIL on",    "km",   obdU16},
    {0x22, "Fuel rail pressure (rel)","kPa",  obdRailRel},
    {0x23, "Fuel rail gauge pressure","kPa",  obdRailGauge},
    {0x2C, "Commanded EGR",           "%",    obdLoad},
    {0x2D, "EGR error",               "%",    obdFuelTrim},
    {0x2F, "Fuel level",              "%",    obdLoad},
    {0x31, "Distance since DTC clear","km",   obdU16},
    {0x33, "Barometric pressure",     "kPa",  obdMap},
    {0x42, "Control module voltage",  "V",    obdModVolt},
    {0x46, "Ambient air temperature", "°C",   obdTempA40},
    {0x5C, "Engine oil temperature",  "°C",   obdTempA40},
    {0x5E, "Engine fuel rate",        "L/h",  obdFuelRate},
};
inline constexpr size_t kObdParamCount = sizeof(kObdParams) / sizeof(kObdParams[0]);

inline const LiveDataParam* findObdParam(uint8_t pid) {
    for (size_t i = 0; i < kObdParamCount; ++i)
        if (static_cast<uint8_t>(kObdParams[i].id) == pid) return &kObdParams[i];
    return nullptr;
}

// =============================================================================
// PSA Lexia3 measurement names — UNVERIFIED reference checklist.
// These are the parameter NAMES Diagbox/Lexia3 shows (docs/lexia3_menu_reference
// .md §4.1.9). The wire ID and scaling are PSA-proprietary and NOT known here,
// so decode is null and id is 0: this list is printed by `meas` as a discovery
// checklist only — it is never queried and never fabricates a value. Fill an
// entry in (real id + decoder) once it is confirmed on the car via gsniff/raw.
// =============================================================================
struct PsaRefParam { const char* name; const char* unit; };
inline constexpr PsaRefParam kPsaReferenceParams[] = {
    {"Presence of +IGN (ignition on)", "bool"},
    {"Ignition key in cranking position", "bool"},
    {"Engine operating status", "enum"},
    {"Alternator excitation voltage", "V"},
    {"Water in diesel", "bool"},
    {"Fuel sender impedance", "ohm"},
    {"Displayed fuel level", "L"},
    {"Gross fuel level", "L"},
    {"Oil pressure warning", "bool"},
    {"Measured oil level", "L"},
    {"Oil temperature", "°C"},
    {"Load shedding level", "enum"},
};
inline constexpr size_t kPsaReferenceParamCount =
    sizeof(kPsaReferenceParams) / sizeof(kPsaReferenceParams[0]);

} // namespace psa
