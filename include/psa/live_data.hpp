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

} // namespace psa
