// BSI configuration zone database -- PSA/Stellantis vehicle parameters
// Maps zone/DID identifiers to individual bit/byte parameters with
// human-readable names, categories, and enum value descriptions.
// Sources: Lexia 3 menu reference, PSA BSI diagnostic documentation.
#pragma once
#include <cstdint>
#include <cstddef>

namespace psa {

enum ZoneType : uint8_t {
    ZT_BOOL,
    ZT_ENUM,
    ZT_NUMERIC,
    ZT_HEX,
    ZT_STRING,
};

struct BsiZoneParam {
    uint16_t zone_id;
    uint16_t byte_offset;
    uint8_t bit_mask;
    const char* name;
    const char* category;
    ZoneType type;
    const char* const* enum_values;
};

struct ZoneCategory {
    const char* name;
    const BsiZoneParam* params;
    size_t count;
};

// =============================================================================
// Enum value tables
// =============================================================================

inline constexpr const char* kYesNo[] = {
    "0=No",
    "1=Yes",
    nullptr
};

inline constexpr const char* kPresentAbsent[] = {
    "0=Absent / Missing",
    "1=Present",
    nullptr
};

inline constexpr const char* kCruiseControlType[] = {
    "0=No cruise control",
    "1=Cruise control only",
    "2=Speed limiter only",
    "3=Cruise control and speed limitation",
    nullptr
};

inline constexpr const char* kParkingAssistType[] = {
    "0=No parking assistance",
    "1=Rear only (ultrasonic)",
    "2=Front and rear (ultrasonic)",
    "3=360 camera",
    nullptr
};

inline constexpr const char* kParkingAssistFR[] = {
    "0=Rear only",
    "1=Front and rear",
    "2=Front/rear/side",
    nullptr
};

inline constexpr const char* kFuelFillerCap[] = {
    "0=No detection",
    "1=Switch type",
    "2=Resistive type",
    nullptr
};

inline constexpr const char* kDieselAdditivePump[] = {
    "0=Not present",
    "1=By the particle filter",
    "2=By the injection ECU",
    nullptr
};

inline constexpr const char* kAlternatorType[] = {
    "0=Standard alternator",
    "1=Alternator with regulator",
    "2=Smart alternator (Li-ion)",
    "3=Alternator-starter hybrid",
    nullptr
};

inline constexpr const char* kTyreDeflationType[] = {
    "0=Not present",
    "1=Indirect (ABS-based) without display of pressures",
    "2=Direct (pressure sensors) with display",
    "3=Indirect with display",
    nullptr
};

inline constexpr const char* kDayRunningLightsType[] = {
    "0=No daytime lights",
    "1=Dipped beam DRL",
    "2=Dedicated DRL lamps",
    "3=LED DRL",
    "4=Position lamps DRL",
    nullptr
};

inline constexpr const char* kWaterInFuelOrigin[] = {
    "0=Not present",
    "1=Engine management ECU",
    "2=BSI",
    "3=Dedicated sensor",
    nullptr
};

inline constexpr const char* kOilTempSource[] = {
    "0=Not present",
    "1=Engine relay unit",
    "2=Engine management ECU",
    "3=Dedicated sensor",
    nullptr
};

inline constexpr const char* kSeatBeltMgmtType[] = {
    "0=Not present",
    "1=Wire",
    "2=Multiplexed",
    nullptr
};

inline constexpr const char* kFaultMemorizing[] = {
    "0=Not authorised",
    "1=Authorised",
    "2=Authorised with warning",
    nullptr
};

inline constexpr const char* kCustomMenuType[] = {
    "0=Standard user profile",
    "1=Unique user profile",
    "2=Customised user profile",
    nullptr
};

inline constexpr const char* kOilPressureSource[] = {
    "0=Not present",
    "1=Engine relay unit",
    "2=Engine management ECU",
    nullptr
};

inline constexpr const char* kOilLevelSource[] = {
    "0=Not present",
    "1=Engine relay unit",
    "2=Engine management ECU",
    "3=Dedicated sensor",
    nullptr
};

inline constexpr const char* kSunshineSensorType[] = {
    "0=Not present",
    "1=Single zone sunshine sensor",
    "2=Two zone sunshine sensor",
    nullptr
};

inline constexpr const char* kAirMixingType[] = {
    "0=Manual mixing",
    "1=Automatic mixing (single zone)",
    "2=Two zone",
    "3=Tri-zone",
    nullptr
};

inline constexpr const char* kAirDistributionType[] = {
    "0=Manual distribution",
    "1=Automatic distribution",
    "2=Two zone",
    nullptr
};

inline constexpr const char* kAdditionalHeatingType[] = {
    "0=Absent",
    "1=Electric PTC heater",
    "2=Fuel-burning heater",
    "3=Electric + fuel heater",
    nullptr
};

inline constexpr const char* kFrontLightingType[] = {
    "0=Halogen",
    "1=Xenon bulbs (HID)",
    "2=LED",
    "3=Matrix LED",
    "4=Laser LED",
    nullptr
};

inline constexpr const char* kLockingType[] = {
    "0=Central locking",
    "1=Selective unlocking (driver door first)",
    "2=Deadlocking",
    "3=Selective + deadlocking",
    nullptr
};

inline constexpr const char* kSunroofType[] = {
    "0=No sunroof",
    "1=Manual sunroof",
    "2=Electric sunroof",
    "3=Panoramic roof / sunroof",
    nullptr
};

inline constexpr const char* kChildSafetyType[] = {
    "0=Manual child locks",
    "1=Mechanical",
    "2=Electric child locks",
    nullptr
};

inline constexpr const char* kAlarmType[] = {
    "0=No alarm",
    "1=Basic alarm (perimeter)",
    "2=Standard alarm",
    "3=Full alarm (perimeter + volumetric)",
    "4=Alarm with tilt sensor",
    nullptr
};

inline constexpr const char* kKeyType[] = {
    "0=Standard key",
    "1=Plip key (infrared)",
    "2=Weak current key (RF remote)",
    "3=Hands-free keyless entry",
    nullptr
};

inline constexpr const char* kFuelType[] = {
    "0=Petrol (unleaded)",
    "1=Diesel",
    "2=LPG",
    "3=Petrol + LPG",
    "4=Electric",
    "5=Hybrid",
    nullptr
};

inline constexpr const char* kFuelSenderSelection[] = {
    "0=Not present",
    "1=Diesel engines",
    "2=Petrol engines",
    "3=All engines",
    nullptr
};

inline constexpr const char* kDipstickLaw[] = {
    "0=Not present",
    "1=petrol 1.8L(EW7)",
    "2=petrol 2.0L(EW10)",
    "3=petrol 3.0L(ES9)",
    "4=petrol 3.0L(V6)",
    "5=diesel 1.6L(DV6)",
    "6=diesel 2.0L(DW10)",
    "7=diesel 2.2L(DW12)",
    "8=diesel 2.7L(DT17)",
    "9=diesel 3.0L(DT20)",
    nullptr
};

inline constexpr const char* kOilLevelMeasureCond[] = {
    "0=Not measured",
    "1=Engine off",
    "2=Measured at ignition on",
    "3=Measured periodically",
    "4=Measured on request only",
    nullptr
};

inline constexpr const char* kInteriorLampSwitch[] = {
    "0=Standard switch",
    "1=One-touch switch",
    nullptr
};

// =============================================================================
// Vehicle Definition (zones 0x0100 - 0x0103) — combined composite array
// =============================================================================
inline constexpr BsiZoneParam kVehicleDefParams[] = {
    // Byte 0
    {0x0100, 0, 0x01, "Overspeed warning for the Arabian peninsula", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 0, 0x02, "Passenger seat position memory option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 0, 0x04, "Automatic gearbox option", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 0, 0x08, "RH drive vehicle", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 0, 0x10, "Dynamic stability control option (ESP)", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 0, 0x20, "Variable damping suspension option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 0, 0x40, "Driving school vehicle option", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 0, 0x80, "Three door vehicle", "Vehicle Definition", ZT_BOOL, kYesNo},
    // Byte 1
    {0x0100, 1, 0x01, "Oil temperature sensor option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 1, 0x02, "Coolant level sensor option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 1, 0x04, "Passenger airbag option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 1, 0x08, "Presence of Telematic unit (RT3/RT4)", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 1, 0x10, "Water in diesel sensor", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 1, 0x20, "Air pump presence", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 1, 0x40, "Multiplexed ABS option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 1, 0x80, "Estate vehicle", "Vehicle Definition", ZT_BOOL, kYesNo},
    // Byte 2
    {0x0100, 2, 0x01, "Presence of controlled manual gearbox", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 2, 0x02, "Driver seat memorisation option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 2, 0x04, "Presence of a trailer relay unit", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 2, 0x0C, "Presence and type of cruise control", "Vehicle Definition", ZT_ENUM, kCruiseControlType},
    {0x0100, 2, 0x30, "Type of parking assistance", "Vehicle Definition", ZT_ENUM, kParkingAssistType},
    {0x0100, 2, 0xC0, "Parking assistance front/rear", "Vehicle Definition", ZT_ENUM, kParkingAssistFR},
    // Byte 3
    {0x0100, 3, 0x01, "Type of fuel filler cap presence detection", "Vehicle Definition", ZT_ENUM, kFuelFillerCap},
    {0x0100, 3, 0x06, "Control of the diesel additive pump", "Vehicle Definition", ZT_ENUM, kDieselAdditivePump},
    {0x0100, 3, 0x18, "Type of alternator", "Vehicle Definition", ZT_ENUM, kAlternatorType},
    {0x0100, 3, 0x20, "Engine management ECU compatible with speed limiter", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0100, 3, 0x40, "Overtaking assistance option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0100, 3, 0x80, "Presence of a secondary electric brake", "Vehicle Definition", ZT_BOOL, kYesNo},
    // 0x0101
    {0x0101, 0, 0x01, "Origin of water in fuel information", "Vehicle Definition", ZT_ENUM, kWaterInFuelOrigin},
    {0x0101, 0, 0x06, "Source of oil temperature information", "Vehicle Definition", ZT_ENUM, kOilTempSource},
    {0x0101, 0, 0x18, "Type of seat belt fastening management unit", "Vehicle Definition", ZT_ENUM, kSeatBeltMgmtType},
    {0x0101, 0, 0x20, "Presence of parking assistance button", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 0, 0x40, "Presence of parking assistance warning", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 0, 0x80, "Presence of a RD4 audio system", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x01, "Parking assistance with visual information", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x02, "Parking assistance with audible information", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x04, "Presence of function log", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x08, "Presence of warning log", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x10, "Presence of a fuel pump", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x20, "Presence of front passenger detection area", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0101, 1, 0x40, "Presence of welcome function for the driver", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 1, 0x80, "Presence of faulty parking assistance warning", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x01, "Activation of seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x02, "Driver seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x04, "Front passenger seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x08, "Front middle passenger seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x10, "Rear middle passenger seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x20, "Rear LH passenger seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x40, "Rear RH passenger seat belt not fastened detection", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 2, 0x80, "Display of rear seat belt reminder on door open", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0101, 3, 0x01, "Origin of oil pressure information", "Vehicle Definition", ZT_ENUM, kOilPressureSource},
    {0x0101, 3, 0x06, "Origin of oil level information", "Vehicle Definition", ZT_ENUM, kOilLevelSource},
    {0x0101, 3, 0x08, "Customisation menu type", "Vehicle Definition", ZT_ENUM, kCustomMenuType},
    {0x0101, 3, 0x10, "Memorizing of faults", "Vehicle Definition", ZT_ENUM, kFaultMemorizing},
    {0x0101, 3, 0x20, "Lane departure warning system option", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0101, 3, 0x40, "Rear seat position memory unit", "Vehicle Definition", ZT_BOOL, kPresentAbsent},
    {0x0101, 3, 0x80, "Display fuel consumption without DPF regen extra", "Vehicle Definition", ZT_BOOL, kYesNo},
    // 0x0102
    {0x0102, 0, 0xFF, "Total period before maintenance (months)", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 1, 0xFF, "Revolutions before maintenance (millions)", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 2, 0xFF, "First maintenance limit (km) /100", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 3, 0xFF, "Maintenance limit (km) /100", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 4, 0xFF, "Distance limit for forcing customer mode (km)", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 5, 0xFF, "Distance limit for parc to customer mode switch (km)", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 6, 0xFF, "Tolerance on speed limitation/cruise setting (kph*10)", "Vehicle Definition", ZT_NUMERIC, nullptr},
    {0x0102, 7, 0xFF, "Response time from ECU to BSI for cruise (s*10)", "Vehicle Definition", ZT_NUMERIC, nullptr},
    // 0x0103
    {0x0103, 0, 0x01, "Red LED for Power Steering warning", "Vehicle Definition", ZT_BOOL, kYesNo},
    {0x0103, 0, 0x02, "Orange LED for Power Steering warning", "Vehicle Definition", ZT_BOOL, kYesNo},
};

// =============================================================================
// Customer Options (zone 0x2100)
// =============================================================================

inline constexpr BsiZoneParam kCustomerOptionsParams[] = {
    {0x2100, 0, 0x01, "Multiplexed electric door mirrors with fold in function", "Customer Options", ZT_BOOL, kYesNo},
    {0x2100, 0, 0x02, "Presence of rear wiping in reverse gear", "Customer Options", ZT_BOOL, kYesNo},
    {0x2100, 0, 0x04, "Close windows with high frequency remote control and key", "Customer Options", ZT_BOOL, kPresentAbsent},
    {0x2100, 0, 0x08, "Type of tyre deflation detection", "Customer Options", ZT_ENUM, kTyreDeflationType},
    {0x2100, 0, 0x30, "Type of day running lights", "Customer Options", ZT_ENUM, kDayRunningLightsType},
    {0x2100, 0, 0x40, "Driver seat belt not fastened detection", "Customer Options", ZT_BOOL, kYesNo},
};

// =============================================================================
// Heating / Air Conditioning (zone 0x2300)
// =============================================================================

inline constexpr BsiZoneParam kHeatingACParams[] = {
    {0x2300, 0, 0x01, "Presence of exterior temperature sensor", "Heating/AC", ZT_BOOL, kYesNo},
    {0x2300, 0, 0x02, "Presence of AC compressor with external control", "Heating/AC", ZT_BOOL, kYesNo},
    {0x2300, 0, 0x04, "Presence of pollutant sensor", "Heating/AC", ZT_BOOL, kYesNo},
    {0x2300, 0, 0x08, "Type of sunshine sensor", "Heating/AC", ZT_ENUM, kSunshineSensorType},
    {0x2300, 0, 0x30, "Type of air mixing", "Heating/AC", ZT_ENUM, kAirMixingType},
    {0x2300, 0, 0xC0, "Type of air distribution", "Heating/AC", ZT_ENUM, kAirDistributionType},
    {0x2300, 1, 0x07, "Type of additional heating", "Heating/AC", ZT_ENUM, kAdditionalHeatingType},
    {0x2300, 1, 0xF8, "AC compressor drive ratio (/100)", "Heating/AC", ZT_NUMERIC, nullptr},
    {0x2300, 2, 0x01, "Presence of a controlled blower motor", "Heating/AC", ZT_BOOL, kYesNo},
};

// =============================================================================
// Lighting / Signalling / Visibility (zones 0x2200 - 0x2201)
// =============================================================================

inline constexpr BsiZoneParam kLightingParams[] = {
    // 0x2200
    {0x2200, 0, 0x01, "Brightness sensor option", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 0, 0x02, "Rain sensor option", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 0, 0x04, "Rear screen wiper option", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 0, 0x08, "Headlamp washer option", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 0, 0x10, "Automatic hazard warning lamps illumination on impact", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 0, 0x20, "Front fog lamps presence", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 0, 0x40, "Multiplexed electric door mirrors with fold back", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 0, 0x80, "Indexed mirrors for reverse gear", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 1, 0x01, "Presence of directional headlamps", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 1, 0x0E, "Type of front lighting", "Lighting", ZT_ENUM, kFrontLightingType},
    {0x2200, 1, 0x70, "Type of day running lights", "Lighting", ZT_ENUM, kDayRunningLightsType},
    {0x2200, 1, 0x80, "Black Panel mode option", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 2, 0x01, "Vehicle location using indicators", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 2, 0x02, "Illumination of hazard on heavy deceleration", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 2, 0x04, "Dipped beam and main beam in same lens unit", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 2, 0x08, "Presence of rear wiping in reverse gear", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 2, 0x10, "Illumination of hazard when emergency call pressed", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 2, 0x20, "Cold climate option", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 2, 0x40, "Main beam and fog lamps in same lens unit", "Lighting", ZT_BOOL, kPresentAbsent},
    {0x2200, 2, 0x80, "Stalk with one-touch automatic wiper activation", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 3, 0x01, "Presence of LH reversing lamp", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 3, 0x02, "Presence of RH reversing lamp", "Lighting", ZT_BOOL, kYesNo},
    {0x2200, 3, 0x0C, "Type of interior lamp switch", "Lighting", ZT_ENUM, kInteriorLampSwitch},
};

// =============================================================================
// Locking / Doors / Immobiliser / Alarm (zones 0x2400 - 0x2401)
// =============================================================================

inline constexpr BsiZoneParam kLockingParams[] = {
    // 0x2400
    {0x2400, 0, 0x01, "Locking when driving option", "Locking", ZT_BOOL, kPresentAbsent},
    {0x2400, 0, 0x02, "Mercosur electric window logic", "Locking", ZT_BOOL, kYesNo},
    {0x2400, 0, 0x04, "Central closing using HF remote control", "Locking", ZT_BOOL, kYesNo},
    {0x2400, 0, 0x0C, "Type of locking", "Locking", ZT_ENUM, kLockingType},
    {0x2400, 0, 0x10, "Two front multiplexed electric windows", "Locking", ZT_BOOL, kYesNo},
    {0x2400, 0, 0x60, "Sunroof number / type", "Locking", ZT_ENUM, kSunroofType},
    {0x2400, 0, 0x80, "Type of child safety", "Locking", ZT_ENUM, kChildSafetyType},
    {0x2400, 1, 0x01, "Automatic relocking", "Locking", ZT_BOOL, kYesNo},
    {0x2400, 1, 0x06, "Alarm type", "Locking", ZT_ENUM, kAlarmType},
    {0x2400, 1, 0x18, "Type of key", "Locking", ZT_ENUM, kKeyType},
    {0x2400, 1, 0x20, "Theft-proof mode", "Locking", ZT_BOOL, kYesNo},
    {0x2400, 1, 0x40, "THATCHAM mode activation", "Locking", ZT_BOOL, kYesNo},
    {0x2400, 1, 0x80, "Permanent locking of boot option", "Locking", ZT_BOOL, kPresentAbsent},
    // 0x2401: Additional locking
    {0x2401, 0, 0x01, "Two front electric windows", "Locking", ZT_BOOL, kYesNo},
    {0x2401, 0, 0x02, "Opening rear screen option", "Locking", ZT_BOOL, kPresentAbsent},
    {0x2401, 0, 0x04, "Child safety option", "Locking", ZT_BOOL, kPresentAbsent},
    {0x2401, 0, 0x08, "Close windows with remote and key", "Locking", ZT_BOOL, kPresentAbsent},
};

// =============================================================================
// Fuel Sender Law - Oil Level Sender Law (zones 0x2500 - 0x2501)
// =============================================================================

inline constexpr BsiZoneParam kFuelOilParams[] = {
    // 0x2500
    {0x2500, 0, 0x07, "Fuel type", "Fuel/Oil", ZT_ENUM, kFuelType},
    {0x2500, 0, 0x08, "Oil level sensor option", "Fuel/Oil", ZT_BOOL, kPresentAbsent},
    {0x2500, 1, 0xFF, "Tank capacity (litres)", "Fuel/Oil", ZT_NUMERIC, nullptr},
    {0x2500, 2, 0xFF, "Fuel sender warning level (litres)", "Fuel/Oil", ZT_NUMERIC, nullptr},
    {0x2500, 3, 0x03, "Oil level measuring condition", "Fuel/Oil", ZT_ENUM, kOilLevelMeasureCond},
    // 0x2501: Extended fuel/oil parameters
    {0x2501, 0, 0xFF, "Fuel sender resistance at full (ohms)", "Fuel/Oil", ZT_NUMERIC, nullptr},
    {0x2501, 1, 0x07, "Fuel sender law - vehicle selection", "Fuel/Oil", ZT_ENUM, kFuelSenderSelection},
    {0x2501, 1, 0xF8, "Dipstick law - engine", "Fuel/Oil", ZT_ENUM, kDipstickLaw},
    {0x2501, 2, 0x01, "Origin of oil level information", "Fuel/Oil", ZT_ENUM, kOilLevelSource},
};

// =============================================================================
// Master category table
// =============================================================================

inline constexpr ZoneCategory kZoneCategories[] = {
    {"Vehicle Definition - Equipment - Driving Information",
     kVehicleDefParams, sizeof(kVehicleDefParams) / sizeof(kVehicleDefParams[0])},
    {"Customer Options",
     kCustomerOptionsParams, sizeof(kCustomerOptionsParams) / sizeof(kCustomerOptionsParams[0])},
    {"Passenger Compartment Heating - Air Conditioning",
     kHeatingACParams, sizeof(kHeatingACParams) / sizeof(kHeatingACParams[0])},
    {"Lighting - Signalling - Visibility - Rear View Mirrors",
     kLightingParams, sizeof(kLightingParams) / sizeof(kLightingParams[0])},
    {"Locking - Doors/Windows - Immobiliser - Alarm",
     kLockingParams, sizeof(kLockingParams) / sizeof(kLockingParams[0])},
    {"Fuel Sender Law - Oil Level Sender Law",
     kFuelOilParams, sizeof(kFuelOilParams) / sizeof(kFuelOilParams[0])},
};

inline constexpr size_t kZoneCategoryCount =
    sizeof(kZoneCategories) / sizeof(kZoneCategories[0]);

// =============================================================================
// Helper functions
// =============================================================================

inline const BsiZoneParam* findZoneParam(uint16_t zone_id) {
    for (size_t c = 0; c < kZoneCategoryCount; ++c) {
        for (size_t p = 0; p < kZoneCategories[c].count; ++p) {
            if (kZoneCategories[c].params[p].zone_id == zone_id)
                return &kZoneCategories[c].params[p];
        }
    }
    return nullptr;
}

inline const ZoneCategory* findParamsByCategory(const char* category) {
    if (!category) return nullptr;
    for (size_t c = 0; c < kZoneCategoryCount; ++c) {
        const char* cat = kZoneCategories[c].name;
        size_t i = 0;
        while (cat[i] && category[i] && cat[i] == category[i]) ++i;
        if (cat[i] == '\0' && category[i] == '\0')
            return &kZoneCategories[c];
    }
    return nullptr;
}

inline const ZoneCategory* getZoneCategories() { return kZoneCategories; }
inline constexpr size_t getZoneCategoryCount() { return kZoneCategoryCount; }

} // namespace psa
