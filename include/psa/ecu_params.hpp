// Per-ECU parameter/config/actuator databases for all Lexia 3 ECUs.
// Each ECU gets its own set of parameter DIDs/LIDs, configuration zones,
// and actuator test IDs, matching the Lexia 3 menu structure.
#pragma once
#include <cstdint>
#include <cstddef>
#include "psa/live_data.hpp"

namespace psa {

// =============================================================================
// Enum value tables shared across ECUs
// =============================================================================

inline constexpr const char* kYesNo[] = {
    "0=No / Absent",
    "1=Yes / Present",
    nullptr
};

inline constexpr const char* kActivatedDeactivated[] = {
    "0=Deactivated",
    "1=Activated",
    nullptr
};

// AUTORADIO enums
inline constexpr const char* kUsageZone[] = {
    "0=Europe",
    "1=Asia",
    "2=America",
    "3=Africa",
    "4=Oceania",
    nullptr
};

inline constexpr const char* kVolumeCorrectionLaw[] = {
    "0=Law N°1",
    "1=Law N°2",
    "2=Law N°3",
    "3=Law N°4",
    nullptr
};

inline constexpr const char* kSensitivityCurve[] = {
    "0=Curve n°1",
    "1=Curve n°2",
    "2=Curve n°3",
    nullptr
};

inline constexpr const char* kAuxInputType[] = {
    "0=Missing",
    "1=Classic (RCA)",
    "2=USB",
    "3=Bluetooth",
    "4=HDMI",
    nullptr
};

inline constexpr const char* kSteeringWheelType[] = {
    "0=Standard steering wheel",
    "1=Fixed central controls",
    "2=Multifunction with scroll wheel",
    nullptr
};

// ESP MK60 enums
inline constexpr const char* kBrakeSwitchType[] = {
    "0=Standard switch",
    "1=Redundant switch",
    "2=Hall effect sensor",
    nullptr
};

// Gearbox enums
inline constexpr const char* kGearboxType[] = {
    "0=Manual",
    "1=Automatic AL4",
    "2=Automatic AM6",
    "3=Automatic ZF6HP",
    nullptr
};

inline constexpr const char* kGearboxOilType[] = {
    "0=Standard ATF",
    "1=LT 71141",
    "2=ESSO LT 71141",
    "3=Mobil ATF",
    nullptr
};

// Suspension enums
inline constexpr const char* kSuspensionMode[] = {
    "0=Normal",
    "1=Sport",
    "2=Comfort",
    "3=Automatic",
    nullptr
};

inline constexpr const char* kSuspensionHeight[] = {
    "0=Low (highway)",
    "1=Normal",
    "2=High (rough road)",
    "3=Very high (off-road)",
    nullptr
};

// Climate enums
inline constexpr const char* kClimateZoneType[] = {
    "0=Manual",
    "1=Single zone auto",
    "2=Dual zone auto",
    "3=Triclim (3 zone)",
    nullptr
};

// =============================================================================
// AUTORADIO (Radio) Configuration - zone 0x2A00
// =============================================================================

inline constexpr const char* kRadioEnumVals_zone[] = { "0=Europe","1=Asia","2=America","3=Africa","4=Oceania",nullptr };
inline constexpr const char* kRadioEnumVals_volumeCorrection[] = { "0=Law N°1","1=Law N°2","2=Law N°3","3=Law N°4",nullptr };
inline constexpr const char* kRadioEnumVals_sensitivity[] = { "0=Curve n°1","1=Curve n°2","2=Curve n°3",nullptr };
inline constexpr const char* kRadioEnumVals_auxInput[] = { "0=Missing","1=Classic","2=USB","3=Bluetooth","4=HDMI",nullptr };
inline constexpr const char* kRadioEnumVals_steeringWheel[] = { "0=Standard","1=Fixed central controls","2=Multifunction with scroll wheel",nullptr };

inline constexpr BsiZoneParam kRadioConfigParams[] = {
    {0x2A00, 0, 0xFF, "Vehicle serial number (VIN)", "Radio Config", ZT_STRING, nullptr},
    {0x2A00, 1, 0x07, "Usage geographical zone", "Radio Config", ZT_ENUM, kRadioEnumVals_zone},
    {0x2A00, 1, 0x08, "CD player", "Radio Config", ZT_BOOL, kYesNo},
    {0x2A00, 1, 0x10, "Fader function", "Radio Config", ZT_BOOL, kActivatedDeactivated},
    {0x2A00, 1, 0x20, "AM frequency band", "Radio Config", ZT_BOOL, kActivatedDeactivated},
    {0x2A00, 1, 0x40, "Volume linked to vehicle speed", "Radio Config", ZT_BOOL, kYesNo},
    {0x2A00, 1, 0x80, "Sound amplifier", "Radio Config", ZT_BOOL, kYesNo},
    {0x2A00, 2, 0x07, "Volume level correction law", "Radio Config", ZT_ENUM, kRadioEnumVals_volumeCorrection},
    {0x2A00, 2, 0x38, "LO/DX sensitivity curve", "Radio Config", ZT_ENUM, kRadioEnumVals_sensitivity},
    {0x2A00, 2, 0x40, "Radiotext function", "Radio Config", ZT_BOOL, kActivatedDeactivated},
    {0x2A00, 2, 0x80, "CDtext function", "Radio Config", ZT_BOOL, kActivatedDeactivated},
    {0x2A00, 3, 0x01, "Parking assistance", "Radio Config", ZT_BOOL, kYesNo},
    {0x2A00, 3, 0x06, "Auxiliary input n°1", "Radio Config", ZT_ENUM, kRadioEnumVals_auxInput},
    {0x2A00, 3, 0x18, "Auxiliary input n°2", "Radio Config", ZT_ENUM, kRadioEnumVals_auxInput},
    {0x2A00, 3, 0x20, "Steering wheel with fixed central controls", "Radio Config", ZT_BOOL, kYesNo},
};

// AUTORADIO parameter measurements
inline constexpr LiveDataParam kRadioMeasParams[] = {
    {0x90, "Radio power state", "", decodeRawByte},
    {0x91, "Audio source", "", decodeRawByte},
    {0x92, "Volume level", "", decodeRawByte},
    {0x93, "Tuner frequency", "MHz", decodeRawByte},
};

// =============================================================================
// ME747 (Bosch Injection) Configuration - zone 0x2B00
// =============================================================================

inline constexpr const char* kInjectionEnumVars_throttleType[] = { "0=Mechanical","1=Electronic (E-Gas)",nullptr };
inline constexpr const char* kInjectionEnumVars_oxygenSensor[] = { "0=Not present","1=Single sensor","2=Dual sensor (pre+post cat)","3=Broadband LSU",nullptr };
inline constexpr const char* kInjectionEnumVars_fuelSystem[] = { "0=Single point","1=Multipoint","2=Direct injection","3=Combined direct+port",nullptr };

inline constexpr BsiZoneParam kInjectionConfigParams[] = {
    {0x2B00, 0, 0x01, "Throttle type", "Injection Config", ZT_ENUM, kInjectionEnumVars_throttleType},
    {0x2B00, 0, 0x02, "Oxygen sensor type", "Injection Config", ZT_ENUM, kInjectionEnumVars_oxygenSensor},
    {0x2B00, 0, 0x0C, "Fuel system type", "Injection Config", ZT_ENUM, kInjectionEnumVars_fuelSystem},
    {0x2B00, 0, 0x10, "Turbocharger present", "Injection Config", ZT_BOOL, kYesNo},
    {0x2B00, 0, 0x20, "EGR valve present", "Injection Config", ZT_BOOL, kYesNo},
    {0x2B00, 0, 0x40, "Variable valve timing", "Injection Config", ZT_BOOL, kYesNo},
};

// ME747 live parameters (UDS DIDs for engine)
inline constexpr LiveDataParam kInjectionMeasParams[] = {
    {0x100A, "Engine RPM", "rpm", decodeEngineRpm},
    {0x100B, "Coolant Temperature", "°C", decodeCoolantTemp},
    {0x100C, "Battery Voltage", "V", decodeBatteryVoltage},
    {0x100D, "Throttle Position", "%", decodeThrottlePosition},
    {0x100E, "Calculated Load", "%", decodeFuelLevel},
    {0x100F, "Intake Air Temperature", "°C", decodeCoolantTemp},
    {0x1010, "Fuel Pressure", "bar", decodeRawByte},
    {0x1011, "Oxygen Sensor Voltage", "V", decodeRawByte},
    {0x1012, "Ignition Timing Advance", "°", decodeRawByte},
    {0x1013, "Injection Timing", "ms", decodeRawByte},
    {0x1014, "Idle Speed Setpoint", "rpm", decodeEngineRpm},
    {0x1015, "Idle Speed Actual", "rpm", decodeEngineRpm},
    {0x1016, "Intake Manifold Pressure", "mbar", decodeEngineRpm},
    {0x1017, "Mass Air Flow", "g/s", decodeRawByte},
};

// ME747 actuator test IDs
struct ActuatorTestEntry {
    uint16_t test_id;
    const char* name;
    const char* description;
};

inline constexpr ActuatorTestEntry kInjectionActuatorTests[] = {
    {0x3101, "Injector 1", "Cylinder 1 fuel injector activation"},
    {0x3102, "Injector 2", "Cylinder 2 fuel injector activation"},
    {0x3103, "Injector 3", "Cylinder 3 fuel injector activation"},
    {0x3104, "Injector 4", "Cylinder 4 fuel injector activation"},
    {0x3105, "Injector 5", "Cylinder 5 fuel injector activation"},
    {0x3106, "Injector 6", "Cylinder 6 fuel injector activation"},
    {0x3107, "Fuel Pump Relay", "Fuel pump relay activation"},
    {0x3108, "Cooling Fan Low", "Engine cooling fan - low speed"},
    {0x3109, "Cooling Fan High", "Engine cooling fan - high speed"},
    {0x310A, "EGR Valve", "EGR valve solenoid activation"},
    {0x310B, "EVAP Canister Purge", "EVAP canister purge valve"},
    {0x310C, "Oxygen Sensor Heater", "O2 sensor heater activation"},
};

// =============================================================================
// ESP MK60 (ABS/ESP) Parameters - zone 0x2C00
// =============================================================================

inline constexpr BsiZoneParam kEspConfigParams[] = {
    {0x2C00, 0, 0x01, "ESP function enabled", "ESP Config", ZT_BOOL, kYesNo},
    {0x2C00, 0, 0x02, "Traction control (ASR)", "ESP Config", ZT_BOOL, kYesNo},
    {0x2C00, 0, 0x04, "Hill start assist", "ESP Config", ZT_BOOL, kYesNo},
    {0x2C00, 0, 0x08, "Brake assist", "ESP Config", ZT_BOOL, kYesNo},
    {0x2C00, 0, 0x10, "Brake switch type", "ESP Config", ZT_ENUM, kBrakeSwitchType},
    {0x2C00, 0, 0x20, "Rollover mitigation", "ESP Config", ZT_BOOL, kYesNo},
    {0x2C00, 0, 0x40, "Trailer stability assist", "ESP Config", ZT_BOOL, kYesNo},
};

// ESP parameters
inline constexpr LiveDataParam kEspMeasParams[] = {
    {0x80, "Wheel Speed FL", "km/h", decodeRawByte},
    {0x81, "Wheel Speed FR", "km/h", decodeRawByte},
    {0x82, "Wheel Speed RL", "km/h", decodeRawByte},
    {0x83, "Wheel Speed RR", "km/h", decodeRawByte},
    {0x84, "Steering Wheel Angle", "°", decodeEngineRpm},
    {0x85, "Brake Switch", "", decodeRawByte},
    {0x86, "Brake Pressure", "bar", decodeRawByte},
    {0x87, "Longitudinal Acceleration", "m/s²", decodeRawByte},
    {0x88, "Lateral Acceleration", "m/s²", decodeRawByte},
    {0x89, "Yaw Rate", "°/s", decodeRawByte},
};

// ESP actuator tests
inline constexpr ActuatorTestEntry kEspActuatorTests[] = {
    {0x3201, "ABS Pump Motor", "Hydraulic pump motor activation"},
    {0x3202, "Left Front Inlet Valve", "LF brake caliper inlet valve"},
    {0x3203, "Left Front Outlet Valve", "LF brake caliper outlet valve"},
    {0x3204, "Right Front Inlet Valve", "RF brake caliper inlet valve"},
    {0x3205, "Right Front Outlet Valve", "RF brake caliper outlet valve"},
    {0x3206, "Left Rear Inlet Valve", "LR brake caliper inlet valve"},
    {0x3207, "Left Rear Outlet Valve", "LR brake caliper outlet valve"},
    {0x3208, "Right Rear Inlet Valve", "RR brake caliper inlet valve"},
    {0x3209, "Right Rear Outlet Valve", "RR brake caliper outlet valve"},
};

// =============================================================================
// BVA_AM6 (Gearbox) Parameters
// =============================================================================

inline constexpr BsiZoneParam kGearboxConfigParams[] = {
    {0x2D00, 0, 0x01, "Gearbox type", "Gearbox Config", ZT_ENUM, kGearboxType},
    {0x2D00, 0, 0x02, "Oil type", "Gearbox Config", ZT_ENUM, kGearboxOilType},
    {0x2D00, 0, 0x04, "Sport mode", "Gearbox Config", ZT_BOOL, kYesNo},
    {0x2D00, 0, 0x08, "Snow mode", "Gearbox Config", ZT_BOOL, kYesNo},
    {0x2D00, 0, 0x10, "Steering wheel paddles", "Gearbox Config", ZT_BOOL, kYesNo},
    {0x2D00, 0, 0x20, "Adaptive shift logic", "Gearbox Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kGearboxMeasParams[] = {
    {0xA0, "Gear Position", "", decodeRawByte},
    {0xA1, "Selected Gear", "", decodeRawByte},
    {0xA2, "Transmission Oil Temp", "°C", decodeCoolantTemp},
    {0xA3, "Engine Torque", "Nm", decodeRawByte},
    {0xA4, "Turbine Speed", "rpm", decodeEngineRpm},
    {0xA5, "Output Speed", "rpm", decodeEngineRpm},
    {0xA6, "Current Shift", "", decodeRawByte},
    {0xA7, "Line Pressure", "bar", decodeRawByte},
};

inline constexpr ActuatorTestEntry kGearboxActuatorTests[] = {
    {0x3301, "Shift Solenoid 1", "Gear selection solenoid 1"},
    {0x3302, "Shift Solenoid 2", "Gear selection solenoid 2"},
    {0x3303, "Lockup Solenoid", "Torque converter lockup solenoid"},
    {0x3304, "Pressure Regulator", "Line pressure regulator"},
};

// =============================================================================
// SUSPENSION (Hydractive / Ecotech) Parameters
// =============================================================================

inline constexpr BsiZoneParam kSuspensionConfigParams[] = {
    {0x2E00, 0, 0x01, "Hydractive suspension present", "Suspension Config", ZT_BOOL, kYesNo},
    {0x2E00, 0, 0x02, "Default mode", "Suspension Config", ZT_ENUM, kSuspensionMode},
    {0x2E00, 0, 0x0C, "Default height", "Suspension Config", ZT_ENUM, kSuspensionHeight},
    {0x2E00, 0, 0x10, "Sport firmness", "Suspension Config", ZT_BOOL, kYesNo},
    {0x2E00, 0, 0x20, "Loading compensation", "Suspension Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kSuspensionMeasParams[] = {
    {0xB0, "Vehicle Height FL", "mm", decodeRawByte},
    {0xB1, "Vehicle Height FR", "mm", decodeRawByte},
    {0xB2, "Vehicle Height RL", "mm", decodeRawByte},
    {0xB3, "Vehicle Height RR", "mm", decodeRawByte},
    {0xB4, "Suspension Mode", "", decodeRawByte},
    {0xB5, "Current Height State", "", decodeRawByte},
    {0xB6, "Pressure Front", "bar", decodeRawByte},
    {0xB7, "Pressure Rear", "bar", decodeRawByte},
};

inline constexpr ActuatorTestEntry kSuspensionActuatorTests[] = {
    {0x3401, "Front Height Raise", "Raise front suspension"},
    {0x3402, "Front Height Lower", "Lower front suspension"},
    {0x3403, "Rear Height Raise", "Raise rear suspension"},
    {0x3404, "Rear Height Lower", "Lower rear suspension"},
    {0x3405, "Front Stiffen", "Stiffen front dampers"},
    {0x3406, "Rear Stiffen", "Stiffen rear dampers"},
};

// =============================================================================
// COMBINE (Instrument Cluster) Configuration
// =============================================================================

inline constexpr BsiZoneParam kClusterConfigParams[] = {
    {0x2F00, 0, 0x01, "Distance unit", "Cluster Config", ZT_BOOL, nullptr},
    {0x2F00, 0, 0x02, "Temperature unit", "Cluster Config", ZT_BOOL, nullptr},
    {0x2F00, 0, 0x04, "Speed display unit", "Cluster Config", ZT_BOOL, nullptr},
    {0x2F00, 0, 0x08, "Language", "Cluster Config", ZT_NUMERIC, nullptr},
};

inline constexpr LiveDataParam kClusterMeasParams[] = {
    {0xC0, "Vehicle Speed", "km/h", decodeRawByte},
    {0xC1, "Total Mileage", "km", decodeEngineRpm},
    {0xC2, "Odometer", "km", decodeEngineRpm},
    {0xC3, "Fuel Level", "L", decodeRawByte},
    {0xC4, "Engine Temp", "°C", decodeCoolantTemp},
    {0xC5, "Ambient Temp", "°C", decodeCoolantTemp},
};

inline constexpr ActuatorTestEntry kClusterActuatorTests[] = {
    {0x3501, "Speedometer Sweep", "Full scale speedometer gauge sweep"},
    {0x3502, "Tachometer Sweep", "Full scale tachometer gauge sweep"},
    {0x3503, "Indicator LED Test", "All indicator lamps test"},
    {0x3504, "Segment Test", "LCD segment display test"},
    {0x3505, "Buzzer Test", "Audible buzzer test"},
};

// =============================================================================
// CLIMATISATION (Climate / HVAC) Parameters
// =============================================================================

inline constexpr LiveDataParam kClimateMeasParams[] = {
    {0xD0, "Interior Temperature", "°C", decodeCoolantTemp},
    {0xD1, "Exterior Temperature", "°C", decodeCoolantTemp},
    {0xD2, "Evaporator Temperature", "°C", decodeCoolantTemp},
    {0xD3, "Coolant Temperature (heater)", "°C", decodeCoolantTemp},
    {0xD4, "AC Pressure", "bar", decodeRawByte},
    {0xD5, "Blower Speed", "", decodeRawByte},
    {0xD6, "Air Distribution Position", "", decodeRawByte},
    {0xD7, "Recirculation State", "", decodeRawByte},
    {0xD8, "AC Compressor State", "", decodeRawByte},
    {0xD9, "Sunshine Sensor", "W/m²", decodeRawByte},
};

inline constexpr ActuatorTestEntry kClimateActuatorTests[] = {
    {0x3601, "AC Compressor", "Air conditioning compressor clutch"},
    {0x3602, "Blower Motor", "Cabin blower motor activation"},
    {0x3603, "Recirculation Flap", "Air recirculation flap"},
    {0x3604, "Air Distribution Flap", "Air distribution flap"},
    {0x3605, "Heater Valve", "Heater coolant valve"},
};

// =============================================================================
// AIRBAG (SRS) — zone 0x3000, LIDs 0xE0-0xE7
// =============================================================================

inline constexpr const char* kAirbagEnum_passAirbag[] = { "0=Activated","1=Deactivated",nullptr };
inline constexpr const char* kAirbagEnum_curtain[] = { "0=Not present","1=Present",nullptr };
inline constexpr const char* kAirbagEnum_seatBelt[] = { "0=Not present","1=Belt reminder present",nullptr };

inline constexpr BsiZoneParam kAirbagConfigParams[] = {
    {0x3000, 0, 0x01, "Passenger airbag", "SRS Config", ZT_ENUM, kAirbagEnum_passAirbag},
    {0x3000, 0, 0x02, "Curtain airbags", "SRS Config", ZT_ENUM, kAirbagEnum_curtain},
    {0x3000, 0, 0x04, "Seat belt reminder (driver)", "SRS Config", ZT_ENUM, kAirbagEnum_seatBelt},
    {0x3000, 0, 0x08, "Seat belt reminder (passenger)", "SRS Config", ZT_ENUM, kAirbagEnum_seatBelt},
    {0x3000, 0, 0x10, "Knee airbag (driver)", "SRS Config", ZT_BOOL, kYesNo},
    {0x3000, 0, 0x20, "Side airbags", "SRS Config", ZT_BOOL, kYesNo},
    {0x3000, 0, 0x40, "Sensor type", "SRS Config", ZT_NUMERIC, nullptr},
};

inline constexpr LiveDataParam kAirbagMeasParams[] = {
    {0xE0, "Driver seatbelt status", "", decodeRawByte},
    {0xE1, "Passenger seatbelt status", "", decodeRawByte},
    {0xE2, "System status", "", decodeRawByte},
    {0xE3, "Passenger airbag deactivation", "", decodeRawByte},
};

// =============================================================================
// DIRECTN (EPS / Power Steering) — zone 0x3100, LIDs 0xF0-0xF6
// =============================================================================

inline constexpr const char* kEpsEnum_assistMap[] = { "0=Standard","1=Sport","2=Comfort","3=Automatic",nullptr };
inline constexpr const char* kEpsEnum_steeringType[] = { "0=Hydraulic","1=Electric (EPS)","2=Electro-hydraulic",nullptr };

inline constexpr BsiZoneParam kEpsConfigParams[] = {
    {0x3100, 0, 0x01, "Steering type", "EPS Config", ZT_ENUM, kEpsEnum_steeringType},
    {0x3100, 0, 0x02, "Assistance map", "EPS Config", ZT_ENUM, kEpsEnum_assistMap},
    {0x3100, 0, 0x04, "Variable assist (speed)", "EPS Config", ZT_BOOL, kYesNo},
    {0x3100, 0, 0x08, "Return-to-centre active", "EPS Config", ZT_BOOL, kYesNo},
    {0x3100, 0, 0x10, "Parking assist steering", "EPS Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kEpsMeasParams[] = {
    {0xF0, "Steering Wheel Angle", "°", decodeEngineRpm},
    {0xF1, "Steering Torque", "Nm", decodeRawByte},
    {0xF2, "Motor Current", "A", decodeRawByte},
    {0xF3, "Assistance Level", "%", decodeRawByte},
    {0xF4, "Motor Temperature", "°C", decodeCoolantTemp},
    {0xF5, "Steering Speed", "rpm", decodeEngineRpm},
};

inline constexpr ActuatorTestEntry kEpsActuatorTests[] = {
    {0x3801, "Steering Calibration", "Re-calibrate steering angle sensor"},
    {0x3802, "Motor Test", "Power steering motor activation test"},
};

// =============================================================================
// HDC (COM2000 / Steering-wheel switch) — LIDs 0x11-0x15
// =============================================================================

inline constexpr LiveDataParam kHdcMeasParams[] = {
    {0x11, "Left stalk position", "", decodeRawByte},
    {0x12, "Right stalk position", "", decodeRawByte},
    {0x13, "Steering wheel button ID", "", decodeRawByte},
    {0x14, "Scroll wheel value", "", decodeRawByte},
    {0x15, "Wiper position", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kHdcActuatorTests[] = {
    {0x3901, "Button LED Test", "Test COM2000 button backlighting"},
};

// =============================================================================
// DSG (TPMS) — zone 0x3200, LIDs 0x21-0x28
// =============================================================================

inline constexpr const char* kTpmsEnum_wheelConfig[] = { "0=Standard","1=Summer","2=Winter","3=All-season",nullptr };
inline constexpr const char* kTpmsEnum_sensorFreq[] = { "0=433 MHz","1=315 MHz",nullptr };

inline constexpr BsiZoneParam kTpmsConfigParams[] = {
    {0x3200, 0, 0x01, "Tyre pressure monitoring", "TPMS Config", ZT_BOOL, kYesNo},
    {0x3200, 0, 0x02, "Wheel configuration set", "TPMS Config", ZT_ENUM, kTpmsEnum_wheelConfig},
    {0x3200, 0, 0x04, "Sensor frequency", "TPMS Config", ZT_ENUM, kTpmsEnum_sensorFreq},
    {0x3200, 0, 0x08, "Low pressure threshold", "TPMS Config", ZT_NUMERIC, nullptr},
    {0x3200, 0, 0x10, "Learning mode active", "TPMS Config", ZT_BOOL, kActivatedDeactivated},
};

inline constexpr LiveDataParam kTpmsMeasParams[] = {
    {0x21, "Pressure FL", "bar", decodeRawByte},
    {0x22, "Pressure FR", "bar", decodeRawByte},
    {0x23, "Pressure RL", "bar", decodeRawByte},
    {0x24, "Pressure RR", "bar", decodeRawByte},
    {0x25, "Temperature FL", "°C", decodeCoolantTemp},
    {0x26, "Temperature FR", "°C", decodeCoolantTemp},
    {0x27, "Temperature RL", "°C", decodeCoolantTemp},
    {0x28, "Temperature RR", "°C", decodeCoolantTemp},
};

inline constexpr ActuatorTestEntry kTpmsActuatorTests[] = {
    {0x3A01, "Sensor Learn (FL)", "Trigger FL sensor learn"},
    {0x3A02, "Sensor Learn (FR)", "Trigger FR sensor learn"},
    {0x3A03, "Sensor Learn (RL)", "Trigger RL sensor learn"},
    {0x3A04, "Sensor Learn (RR)", "Trigger RR sensor learn"},
};

// =============================================================================
// TELEMAT (Navigation/Telematic, UDS) — DIDs 0x2000-0x2005
// =============================================================================

inline constexpr const char* kTelematEnum_region[] = { "0=Europe","1=North America","2=Asia Pacific","3=Middle East",nullptr };
inline constexpr const char* kTelematEnum_navSystem[] = { "0=RT3","1=NAC","2=SMEG","3=RNEG",nullptr };

inline constexpr BsiZoneParam kTelematConfigParams[] = {
    {0x2000, 0, 0x01, "Navigation system type", "Telemat Config", ZT_ENUM, kTelematEnum_navSystem},
    {0x2000, 0, 0x02, "Region", "Telemat Config", ZT_ENUM, kTelematEnum_region},
    {0x2000, 0, 0x04, "GPS receiver present", "Telemat Config", ZT_BOOL, kYesNo},
    {0x2000, 0, 0x08, "Bluetooth module", "Telemat Config", ZT_BOOL, kYesNo},
    {0x2000, 0, 0x10, "Voice control", "Telemat Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kTelematMeasParams[] = {
    {0x2001, "GPS satellite count", "", decodeRawByte},
    {0x2002, "GPS signal quality", "", decodeRawByte},
    {0x2003, "System temperature", "°C", decodeCoolantTemp},
    {0x2004, "Battery backup voltage", "V", decodeBatteryVoltage},
    {0x2005, "Radio tuner status", "", decodeRawByte},
};

// =============================================================================
// AMPLHIFI (Amplifier) — LIDs 0x31-0x34
// =============================================================================

inline constexpr LiveDataParam kAmplifierMeasParams[] = {
    {0x31, "Temperature", "°C", decodeCoolantTemp},
    {0x32, "Supply Voltage", "V", decodeBatteryVoltage},
    {0x33, "Amplifier status", "", decodeRawByte},
    {0x34, "Input signal present", "", decodeRawByte},
};

// =============================================================================
// CPL (Rain/Light Sensor) — zone 0x3300, LIDs 0x41-0x44
// =============================================================================

inline constexpr const char* kCplEnum_rainSens[] = { "0=Off","1=Low","2=Medium","3=High",nullptr };
inline constexpr const char* kCplEnum_lightSens[] = { "0=Off","1=Low","2=Medium","3=High",nullptr };

inline constexpr BsiZoneParam kCplConfigParams[] = {
    {0x3300, 0, 0x01, "Rain sensor sensitivity", "Sensor Config", ZT_ENUM, kCplEnum_rainSens},
    {0x3300, 0, 0x02, "Light sensor sensitivity", "Sensor Config", ZT_ENUM, kCplEnum_lightSens},
    {0x3300, 0, 0x04, "Automatic headlights", "Sensor Config", ZT_BOOL, kActivatedDeactivated},
    {0x3300, 0, 0x08, "Automatic wipers", "Sensor Config", ZT_BOOL, kActivatedDeactivated},
    {0x3300, 0, 0x10, "Coming home delay", "Sensor Config", ZT_NUMERIC, nullptr},
};

inline constexpr LiveDataParam kCplMeasParams[] = {
    {0x41, "Rain intensity", "", decodeRawByte},
    {0x42, "Ambient light level", "lux", decodeRawByte},
    {0x43, "Sensor status", "", decodeRawByte},
    {0x44, "Temperature", "°C", decodeCoolantTemp},
};

inline constexpr ActuatorTestEntry kCplActuatorTests[] = {
    {0x3B01, "Rain Sensor Test", "Trigger rain sensor self-test"},
    {0x3B02, "Light Sensor Test", "Trigger light sensor self-test"},
};

// =============================================================================
// BML (Lighting Control) — zone 0x3400, LIDs 0x51-0x56
// =============================================================================

inline constexpr const char* kBmlEnum_drl[] = { "0=Off","1=Daytime running lights","2=Position lights",nullptr };
inline constexpr const char* kBmlEnum_headlightType[] = { "0=Halogen","1=Xenon","2=LED",nullptr };

inline constexpr BsiZoneParam kBmlConfigParams[] = {
    {0x3400, 0, 0x01, "Headlight type", "Lighting Config", ZT_ENUM, kBmlEnum_headlightType},
    {0x3400, 0, 0x02, "DRL function", "Lighting Config", ZT_ENUM, kBmlEnum_drl},
    {0x3400, 0, 0x04, "Cornering lights", "Lighting Config", ZT_BOOL, kYesNo},
    {0x3400, 0, 0x08, "Automatic high beam", "Lighting Config", ZT_BOOL, kYesNo},
    {0x3400, 0, 0x10, "Fog lights with high beam", "Lighting Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kBmlMeasParams[] = {
    {0x51, "Left low beam status", "", decodeRawByte},
    {0x52, "Right low beam status", "", decodeRawByte},
    {0x53, "Left high beam status", "", decodeRawByte},
    {0x54, "Right high beam status", "", decodeRawByte},
    {0x55, "DRL status", "", decodeRawByte},
    {0x56, "Lighting power mode", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kBmlActuatorTests[] = {
    {0x3C01, "Low Beam On", "Left low beam activation"},
    {0x3C02, "High Beam On", "Left high beam activation"},
    {0x3C03, "Cornering Light", "Left cornering light activation"},
    {0x3C04, "Fog Light", "Front fog light activation"},
};

// =============================================================================
// ADC (Immobiliser / Key) — LIDs 0x61-0x64
// =============================================================================

inline constexpr LiveDataParam kAdcMeasParams[] = {
    {0x61, "Transponder status", "", decodeRawByte},
    {0x62, "Key programming mode", "", decodeRawByte},
    {0x63, "Immobiliser state", "", decodeRawByte},
    {0x64, "Registered keys count", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kAdcActuatorTests[] = {
    {0x3D01, "Key Learn", "Enter key programming mode"},
    {0x3D02, "Immobiliser Unlock", "Unlock immobiliser for ECU replacement"},
};

// =============================================================================
// BSM (Engine Relay Unit) — zone 0x3500, LIDs 0x71-0x77
//
// NOT ADDRESSABLE as its own ECU: the BSM row was removed from kEcuTable
// (psa_protocol.hpp) because it answers on the BSI's own CAN address, so
// `connect BSM` would just be a second name for BMF. The tables below are kept
// as reference — the zone and mask layout is real and reverse-engineered — but
// nothing reaches them from the UI (there is no BSM entry in dashboard.js's
// ECUS[]). Do not merge them into BMF's set without verifying on a car: a write
// aimed at the wrong ECU behind that address is not a recoverable mistake.
// =============================================================================

inline constexpr const char* kBsmEnum_engineVariant[] = { "0=DW10 (HDi 110)","1=DW12 (HDi 136)","2=ES9 (V6)","3=EW10 (2.0i)","4=EW7 (1.8i)",nullptr };
inline constexpr const char* kBsmEnum_glowPlug[] = { "0=Not present","1=Standard glow plugs","2=Instant glow plugs",nullptr };

inline constexpr BsiZoneParam kBsmConfigParams[] = {
    {0x3500, 0, 0x01, "Engine variant", "BSM Config", ZT_ENUM, kBsmEnum_engineVariant},
    {0x3500, 0, 0x02, "Glow plug type", "BSM Config", ZT_ENUM, kBsmEnum_glowPlug},
    {0x3500, 0, 0x04, "Engine cooling fan", "BSM Config", ZT_BOOL, kActivatedDeactivated},
    {0x3500, 0, 0x08, "Starter relay", "BSM Config", ZT_BOOL, kActivatedDeactivated},
    {0x3500, 0, 0x10, "Fuel pump relay", "BSM Config", ZT_BOOL, kActivatedDeactivated},
    {0x3500, 0, 0x20, "Dual battery", "BSM Config", ZT_BOOL, kYesNo},
    {0x3500, 0, 0x40, "Glow plug timer", "BSM Config", ZT_NUMERIC, nullptr},
};

inline constexpr LiveDataParam kBsmMeasParams[] = {
    {0x71, "Supply voltage", "V", decodeBatteryVoltage},
    {0x72, "Cooling fan speed", "rpm", decodeEngineRpm},
    {0x73, "Engine coolant temp", "°C", decodeCoolantTemp},
    {0x74, "Starter relay state", "", decodeRawByte},
    {0x75, "Fuel pump relay state", "", decodeRawByte},
    {0x76, "BSM internal temp", "°C", decodeCoolantTemp},
    {0x77, "Glow plug status", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kBsmActuatorTests[] = {
    {0x3E01, "Starter Relay", "Activate starter relay"},
    {0x3E02, "Fuel Pump Relay", "Activate fuel pump relay"},
    {0x3E03, "Cooling Fan Low", "Engine cooling fan low speed"},
    {0x3E04, "Cooling Fan High", "Engine cooling fan high speed"},
    {0x3E05, "Glow Plug Relay", "Activate glow plug relay"},
};

// =============================================================================
// ALARME (Alarm System) — zone 0x3600, LIDs 0x81-0x85
// =============================================================================

inline constexpr const char* kAlarmEnum_sirenType[] = { "0=Standard","1=Autonomous (battery backed)","2=Remote",nullptr };
inline constexpr const char* kAlarmEnum_trigger[] = { "0=Perimeter only","1=Perimeter+volumetric","2=Perimeter+volumetric+tilt",nullptr };

inline constexpr BsiZoneParam kAlarmConfigParams[] = {
    {0x3600, 0, 0x01, "Alarm system enabled", "Alarm Config", ZT_BOOL, kYesNo},
    {0x3600, 0, 0x02, "Siren type", "Alarm Config", ZT_ENUM, kAlarmEnum_sirenType},
    {0x3600, 0, 0x04, "Trigger zones", "Alarm Config", ZT_ENUM, kAlarmEnum_trigger},
    {0x3600, 0, 0x08, "Interior monitoring", "Alarm Config", ZT_BOOL, kYesNo},
    {0x3600, 0, 0x10, "Tilt sensor", "Alarm Config", ZT_BOOL, kYesNo},
    {0x3600, 0, 0x20, "Panic alarm", "Alarm Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kAlarmMeasParams[] = {
    {0x81, "Alarm system status", "", decodeRawByte},
    {0x82, "Trigger zone last", "", decodeRawByte},
    {0x83, "Siren battery", "V", decodeBatteryVoltage},
    {0x84, "Interior sensor", "", decodeRawByte},
    {0x85, "Tilt sensor", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kAlarmActuatorTests[] = {
    {0x3F01, "Siren Test", "Trigger alarm siren"},
    {0x3F02, "LED Flash", "Alarm status LED flash"},
};

// =============================================================================
// MDP_CONDUCT (Driver Door Module) — zone 0x3700, LIDs 0x91-0x97
// =============================================================================

inline constexpr const char* kDoorEnum_windowType[] = { "0=Manual","1=Electric","2=One-touch auto",nullptr };
inline constexpr const char* kDoorEnum_mirrorType[] = { "0=Manual","1=Electric","2=Electric+fold","3=Electric+fold+memory",nullptr };

inline constexpr BsiZoneParam kDrvDoorConfigParams[] = {
    {0x3700, 0, 0x01, "Window type", "Driver Door Config", ZT_ENUM, kDoorEnum_windowType},
    {0x3700, 0, 0x02, "Mirror type", "Driver Door Config", ZT_ENUM, kDoorEnum_mirrorType},
    {0x3700, 0, 0x04, "Auto-fold mirrors", "Driver Door Config", ZT_BOOL, kYesNo},
    {0x3700, 0, 0x08, "Window closing with remote", "Driver Door Config", ZT_BOOL, kYesNo},
    {0x3700, 0, 0x10, "Anti-pinch windows", "Driver Door Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kDrvDoorMeasParams[] = {
    {0x91, "Window position", "%", decodeRawByte},
    {0x92, "Mirror horizontal", "", decodeRawByte},
    {0x93, "Mirror vertical", "", decodeRawByte},
    {0x94, "Door lock status", "", decodeRawByte},
    {0x95, "Window motor current", "A", decodeRawByte},
    {0x96, "Mirror fold status", "", decodeRawByte},
    {0x97, "Door handle switch", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kDrvDoorActuatorTests[] = {
    {0x4001, "Window Up", "Driver window upward movement"},
    {0x4002, "Window Down", "Driver window downward movement"},
    {0x4003, "Mirror Fold", "Fold driver mirror"},
    {0x4004, "Mirror Unfold", "Unfold driver mirror"},
    {0x4005, "Lock Actuator", "Driver door lock actuator"},
};

// =============================================================================
// MDP_PASSAG (Passenger Door Module) — zone 0x3701, LIDs 0xA1-0xA7
// =============================================================================

inline constexpr BsiZoneParam kPassDoorConfigParams[] = {
    {0x3701, 0, 0x01, "Window type", "Pass Door Config", ZT_ENUM, kDoorEnum_windowType},
    {0x3701, 0, 0x02, "Mirror type", "Pass Door Config", ZT_ENUM, kDoorEnum_mirrorType},
    {0x3701, 0, 0x04, "Auto-fold mirrors", "Pass Door Config", ZT_BOOL, kYesNo},
    {0x3701, 0, 0x08, "Window closing with remote", "Pass Door Config", ZT_BOOL, kYesNo},
    {0x3701, 0, 0x10, "Anti-pinch windows", "Pass Door Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kPassDoorMeasParams[] = {
    {0xA1, "Window position", "%", decodeRawByte},
    {0xA2, "Mirror horizontal", "", decodeRawByte},
    {0xA3, "Mirror vertical", "", decodeRawByte},
    {0xA4, "Door lock status", "", decodeRawByte},
    {0xA5, "Window motor current", "A", decodeRawByte},
    {0xA6, "Mirror fold status", "", decodeRawByte},
    {0xA7, "Door handle switch", "", decodeRawByte},
};

inline constexpr ActuatorTestEntry kPassDoorActuatorTests[] = {
    {0x4101, "Window Up", "Passenger window upward movement"},
    {0x4102, "Window Down", "Passenger window downward movement"},
    {0x4103, "Mirror Fold", "Fold passenger mirror"},
    {0x4104, "Mirror Unfold", "Unfold passenger mirror"},
    {0x4105, "Lock Actuator", "Passenger door lock actuator"},
};

// =============================================================================
// ECRAN_C (Multifunction Screen) — DIDs 0x2500-0x2504
// =============================================================================

inline constexpr const char* kScreenEnum_variant[] = { "0=Monochrome","1=Colour CMB","2=Colour RT3","3=Colour RT6",nullptr };

inline constexpr BsiZoneParam kScreenConfigParams[] = {
    {0x2500, 0, 0x01, "Screen variant", "Screen Config", ZT_ENUM, kScreenEnum_variant},
    {0x2500, 0, 0x02, "Brightness", "Screen Config", ZT_NUMERIC, nullptr},
    {0x2500, 0, 0x04, "Contrast", "Screen Config", ZT_NUMERIC, nullptr},
};

inline constexpr LiveDataParam kScreenMeasParams[] = {
    {0x2501, "Display state", "", decodeRawByte},
    {0x2502, "Backlight level", "", decodeRawByte},
    {0x2503, "Internal temperature", "°C", decodeCoolantTemp},
};

// =============================================================================
// AIDE_STAT (Parking Assistance / CD Player) — LIDs 0xB1-0xB8
// =============================================================================

inline constexpr const char* kParkEnum_sensorConfig[] = { "0=Rear only","1=Front+Rear","2=Front+Rear+Side",nullptr };

inline constexpr BsiZoneParam kParkConfigParams[] = {
    {0x3800, 0, 0x01, "Parking assist present", "Park Assist Config", ZT_BOOL, kYesNo},
    {0x3800, 0, 0x02, "Sensor configuration", "Park Assist Config", ZT_ENUM, kParkEnum_sensorConfig},
    {0x3800, 0, 0x04, "Audible warnings", "Park Assist Config", ZT_BOOL, kActivatedDeactivated},
    {0x3800, 0, 0x08, "Visual display", "Park Assist Config", ZT_BOOL, kActivatedDeactivated},
    {0x3800, 0, 0x10, "CD changer installed", "Park Assist Config", ZT_BOOL, kYesNo},
};

inline constexpr LiveDataParam kParkMeasParams[] = {
    {0xB1, "Distance rear center", "cm", decodeRawByte},
    {0xB2, "Distance rear left", "cm", decodeRawByte},
    {0xB3, "Distance rear right", "cm", decodeRawByte},
    {0xB4, "Distance front center", "cm", decodeRawByte},
    {0xB5, "Distance front left", "cm", decodeRawByte},
    {0xB6, "Distance front right", "cm", decodeRawByte},
    {0xB7, "CD status", "", decodeRawByte},
    {0xB8, "Parking audio tone", "Hz", decodeRawByte},
};

inline constexpr ActuatorTestEntry kParkActuatorTests[] = {
    {0x4201, "Rear Buzzer Test", "Activate rear parking buzzer"},
    {0x4202, "Front Buzzer Test", "Activate front parking buzzer"},
    {0x4203, "Sensor Self-Test", "Run parking sensor self-diagnosis"},
};

// =============================================================================
// PROJECTEURS (Directional Headlamps / AFL) — LIDs 0xC1-0xC6
// =============================================================================

inline constexpr const char* kAflEnum_lampType[] = { "0=Halogen static","1=Xenon directional","2=LED directional",nullptr };
inline constexpr const char* kAflEnum_beamPattern[] = { "0=LHD (left traffic)","1=RHD (right traffic)",nullptr };

inline constexpr BsiZoneParam kAflConfigParams[] = {
    {0x3900, 0, 0x01, "Adaptive headlamps present", "AFL Config", ZT_BOOL, kYesNo},
    {0x3900, 0, 0x02, "Lamp type", "AFL Config", ZT_ENUM, kAflEnum_lampType},
    {0x3900, 0, 0x04, "Beam pattern", "AFL Config", ZT_ENUM, kAflEnum_beamPattern},
    {0x3900, 0, 0x08, "Cornering function", "AFL Config", ZT_BOOL, kActivatedDeactivated},
    {0x3900, 0, 0x10, "Motorway light", "AFL Config", ZT_BOOL, kActivatedDeactivated},
};

inline constexpr LiveDataParam kAflMeasParams[] = {
    {0xC1, "Left vertical aim", "°", decodeRawByte},
    {0xC2, "Right vertical aim", "°", decodeRawByte},
    {0xC3, "Left horizontal aim", "°", decodeRawByte},
    {0xC4, "Right horizontal aim", "°", decodeRawByte},
    {0xC5, "Headlamp level sensor FL", "mm", decodeRawByte},
    {0xC6, "Headlamp level sensor RL", "mm", decodeRawByte},
};

inline constexpr ActuatorTestEntry kAflActuatorTests[] = {
    {0x4301, "Left Leveling", "Left headlamp vertical leveling"},
    {0x4302, "Right Leveling", "Right headlamp vertical leveling"},
    {0x4303, "Left Cornering", "Left cornering light activation"},
    {0x4304, "Right Cornering", "Right cornering light activation"},
};

// =============================================================================
// BMF (BSI) actuator tests — standard Lexia-3 BSI output tests
// =============================================================================

inline constexpr ActuatorTestEntry kBmfActuatorTests[] = {
    {0x3101, "Horn Test", "BSI horn relay activation"},
    {0x3102, "Low Beam Test", "Low beam headlamp relay"},
    {0x3103, "High Beam Test", "High beam headlamp relay"},
    {0x3104, "Left Turn Signal", "Left indicator relay"},
    {0x3105, "Right Turn Signal", "Right indicator relay"},
    {0x3106, "Fog Lights", "Front fog lamp relay"},
    {0x3107, "Wipers Test", "Windscreen wiper relay"},
    {0x3108, "Door Locking", "Central door locking (lock)"},
    {0x3109, "Door Unlocking", "Central door locking (unlock)"},
};

// =============================================================================
// ECU-to-parameter mapping
// =============================================================================

struct EcuParamSet {
    const char* family;          // matches kEcuTable family name
    const BsiZoneParam* config_params;  // configuration zone definitions
    size_t config_count;
    const LiveDataParam* meas_params;   // measurement parameters
    size_t meas_count;
    const ActuatorTestEntry* act_tests; // actuator test definitions
    size_t act_count;
};

inline constexpr EcuParamSet kEcuParamSets[] = {
    {"BMF", nullptr, 0, nullptr, 0,
            kBmfActuatorTests, sizeof(kBmfActuatorTests)/sizeof(kBmfActuatorTests[0])},
    {"INJ", kInjectionConfigParams, sizeof(kInjectionConfigParams)/sizeof(kInjectionConfigParams[0]),
            kInjectionMeasParams, sizeof(kInjectionMeasParams)/sizeof(kInjectionMeasParams[0]),
            kInjectionActuatorTests, sizeof(kInjectionActuatorTests)/sizeof(kInjectionActuatorTests[0])},
    {"ABRASR", kEspConfigParams, sizeof(kEspConfigParams)/sizeof(kEspConfigParams[0]),
               kEspMeasParams, sizeof(kEspMeasParams)/sizeof(kEspMeasParams[0]),
               kEspActuatorTests, sizeof(kEspActuatorTests)/sizeof(kEspActuatorTests[0])},
    {"AIRBAG", kAirbagConfigParams, sizeof(kAirbagConfigParams)/sizeof(kAirbagConfigParams[0]),
               kAirbagMeasParams, sizeof(kAirbagMeasParams)/sizeof(kAirbagMeasParams[0]),
               nullptr, 0},
    {"CLIM", nullptr, 0,
             kClimateMeasParams, sizeof(kClimateMeasParams)/sizeof(kClimateMeasParams[0]),
             kClimateActuatorTests, sizeof(kClimateActuatorTests)/sizeof(kClimateActuatorTests[0])},
    {"COMBINE", kClusterConfigParams, sizeof(kClusterConfigParams)/sizeof(kClusterConfigParams[0]),
                kClusterMeasParams, sizeof(kClusterMeasParams)/sizeof(kClusterMeasParams[0]),
                kClusterActuatorTests, sizeof(kClusterActuatorTests)/sizeof(kClusterActuatorTests[0])},
    {"DIRECTN", kEpsConfigParams, sizeof(kEpsConfigParams)/sizeof(kEpsConfigParams[0]),
                kEpsMeasParams, sizeof(kEpsMeasParams)/sizeof(kEpsMeasParams[0]),
                kEpsActuatorTests, sizeof(kEpsActuatorTests)/sizeof(kEpsActuatorTests[0])},
    {"HDC", nullptr, 0,
            kHdcMeasParams, sizeof(kHdcMeasParams)/sizeof(kHdcMeasParams[0]),
            kHdcActuatorTests, sizeof(kHdcActuatorTests)/sizeof(kHdcActuatorTests[0])},
    {"BOITEVIT", kGearboxConfigParams, sizeof(kGearboxConfigParams)/sizeof(kGearboxConfigParams[0]),
                 kGearboxMeasParams, sizeof(kGearboxMeasParams)/sizeof(kGearboxMeasParams[0]),
                 kGearboxActuatorTests, sizeof(kGearboxActuatorTests)/sizeof(kGearboxActuatorTests[0])},
    {"SPNEU", kSuspensionConfigParams, sizeof(kSuspensionConfigParams)/sizeof(kSuspensionConfigParams[0]),
              kSuspensionMeasParams, sizeof(kSuspensionMeasParams)/sizeof(kSuspensionMeasParams[0]),
              kSuspensionActuatorTests, sizeof(kSuspensionActuatorTests)/sizeof(kSuspensionActuatorTests[0])},
    {"DSG", kTpmsConfigParams, sizeof(kTpmsConfigParams)/sizeof(kTpmsConfigParams[0]),
            kTpmsMeasParams, sizeof(kTpmsMeasParams)/sizeof(kTpmsMeasParams[0]),
            kTpmsActuatorTests, sizeof(kTpmsActuatorTests)/sizeof(kTpmsActuatorTests[0])},
    {"TELEMAT", kTelematConfigParams, sizeof(kTelematConfigParams)/sizeof(kTelematConfigParams[0]),
                kTelematMeasParams, sizeof(kTelematMeasParams)/sizeof(kTelematMeasParams[0]),
                nullptr, 0},
    {"AUTORADIO", kRadioConfigParams, sizeof(kRadioConfigParams)/sizeof(kRadioConfigParams[0]),
                  kRadioMeasParams, sizeof(kRadioMeasParams)/sizeof(kRadioMeasParams[0]),
                  nullptr, 0},
    {"AMPLHIFI", nullptr, 0,
                 kAmplifierMeasParams, sizeof(kAmplifierMeasParams)/sizeof(kAmplifierMeasParams[0]),
                 nullptr, 0},
    {"CPL", kCplConfigParams, sizeof(kCplConfigParams)/sizeof(kCplConfigParams[0]),
            kCplMeasParams, sizeof(kCplMeasParams)/sizeof(kCplMeasParams[0]),
            kCplActuatorTests, sizeof(kCplActuatorTests)/sizeof(kCplActuatorTests[0])},
    {"BML", kBmlConfigParams, sizeof(kBmlConfigParams)/sizeof(kBmlConfigParams[0]),
            kBmlMeasParams, sizeof(kBmlMeasParams)/sizeof(kBmlMeasParams[0]),
            kBmlActuatorTests, sizeof(kBmlActuatorTests)/sizeof(kBmlActuatorTests[0])},
    {"ADC", nullptr, 0,
            kAdcMeasParams, sizeof(kAdcMeasParams)/sizeof(kAdcMeasParams[0]),
            kAdcActuatorTests, sizeof(kAdcActuatorTests)/sizeof(kAdcActuatorTests[0])},
    {"BSM", kBsmConfigParams, sizeof(kBsmConfigParams)/sizeof(kBsmConfigParams[0]),
            kBsmMeasParams, sizeof(kBsmMeasParams)/sizeof(kBsmMeasParams[0]),
            kBsmActuatorTests, sizeof(kBsmActuatorTests)/sizeof(kBsmActuatorTests[0])},
    {"ALARME", kAlarmConfigParams, sizeof(kAlarmConfigParams)/sizeof(kAlarmConfigParams[0]),
               kAlarmMeasParams, sizeof(kAlarmMeasParams)/sizeof(kAlarmMeasParams[0]),
               kAlarmActuatorTests, sizeof(kAlarmActuatorTests)/sizeof(kAlarmActuatorTests[0])},
    {"MDP_CONDUCT", kDrvDoorConfigParams, sizeof(kDrvDoorConfigParams)/sizeof(kDrvDoorConfigParams[0]),
                    kDrvDoorMeasParams, sizeof(kDrvDoorMeasParams)/sizeof(kDrvDoorMeasParams[0]),
                    kDrvDoorActuatorTests, sizeof(kDrvDoorActuatorTests)/sizeof(kDrvDoorActuatorTests[0])},
    {"MDP_PASSAG", kPassDoorConfigParams, sizeof(kPassDoorConfigParams)/sizeof(kPassDoorConfigParams[0]),
                   kPassDoorMeasParams, sizeof(kPassDoorMeasParams)/sizeof(kPassDoorMeasParams[0]),
                   kPassDoorActuatorTests, sizeof(kPassDoorActuatorTests)/sizeof(kPassDoorActuatorTests[0])},
    {"ECRAN_C", kScreenConfigParams, sizeof(kScreenConfigParams)/sizeof(kScreenConfigParams[0]),
                kScreenMeasParams, sizeof(kScreenMeasParams)/sizeof(kScreenMeasParams[0]),
                nullptr, 0},
    {"AIDE_STAT", kParkConfigParams, sizeof(kParkConfigParams)/sizeof(kParkConfigParams[0]),
                  kParkMeasParams, sizeof(kParkMeasParams)/sizeof(kParkMeasParams[0]),
                  kParkActuatorTests, sizeof(kParkActuatorTests)/sizeof(kParkActuatorTests[0])},
    {"PROJECTEURS", kAflConfigParams, sizeof(kAflConfigParams)/sizeof(kAflConfigParams[0]),
                    kAflMeasParams, sizeof(kAflMeasParams)/sizeof(kAflMeasParams[0]),
                    kAflActuatorTests, sizeof(kAflActuatorTests)/sizeof(kAflActuatorTests[0])},
};

inline constexpr size_t kEcuParamSetCount = sizeof(kEcuParamSets) / sizeof(kEcuParamSets[0]);

inline const EcuParamSet* findEcuParamSet(const char* family) {
    if (!family) return nullptr;
    for (size_t i = 0; i < kEcuParamSetCount; ++i) {
        const char* a = family;
        const char* b = kEcuParamSets[i].family;
        while (*a && *b && (*a == *b || (*a | 32) == (*b | 32))) { a++; b++; }
        if (!*a && !*b) return &kEcuParamSets[i];
    }
    return nullptr;
}

} // namespace psa
