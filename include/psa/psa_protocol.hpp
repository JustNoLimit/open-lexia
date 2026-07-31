// PSA diagnostic protocol primitives — pure, host-compilable, no hardware deps.
// ECU address table, KWP2000/UDS service constants, PSA seed/key algorithm.
// Reference: docs/psa_can_reference.md (sections 3, 4.2, 4.3, 4.4).
#pragma once
#include <cstdint>
#include <cstddef>
#include <array>

namespace psa {

// --- Protocol flavour ---------------------------------------------------------
// C5 Mk1 FL (CAN2004) speaks KWP2000-over-CAN. CAN2010+ ECUs speak UDS.
// KWP itself has two wire encodings: long-form on HAB (125k comfort) and
// short-form on IS (500k powertrain). Keep the variant on a per-ECU basis.
enum class Protocol : uint8_t {
    KWP_HAB, // KWP2000 long-form, comfort/125k  (10C0 / 1081 / 21XX / 3BXX)
    KWP_IS,  // KWP2000 short-form, powertrain/500k (81 / 82 / 21XX / 34XX)
    UDS,     // ISO 14229 (1003 / 2703 / 22XXXX / 2EXXXX)
};

// --- ECU address table (EMIT_ID : RECV_ID) -----------------------------------
// Source: ludwig-v/arduino-psa-diag ECU_LIST.md; subset relevant to C5 Mk1 FL.
struct EcuAddr {
    const char* family;
    uint16_t emit_id;  // tester -> ECU
    uint16_t recv_id;  // ECU -> tester
    Protocol  proto;   // default protocol flavour for this ECU's bus
    const char* note;
};

inline constexpr EcuAddr kEcuTable[] = {
    {"INJ",      0x6A8, 0x688, Protocol::KWP_IS,  "Engine ECU (EDC16/SID80/MM6/DCM)"},
    {"BMF",      0x752, 0x652, Protocol::KWP_IS,  "BSI (central gateway)"},
    {"ABRASR",   0x6AD, 0x68D, Protocol::KWP_IS,  "ABS / ESP"},
    // AIRBAG/COMBINE were KWP_IS; on the real car both rejected 0x81 with
    // NRC 11 (service not supported) — the signature of the wrong KWP form,
    // not a real precondition failure (compare BSI's NRC 22 below, which
    // means the service byte WAS recognised). Switched to KWP_HAB.
    {"AIRBAG",   0x744, 0x644, Protocol::KWP_HAB, "Airbag / SRS"},
    {"CLIM",     0x76D, 0x66D, Protocol::KWP_HAB, "Climate control"},
    {"COMBINE",  0x75F, 0x65F, Protocol::KWP_HAB, "Instrument cluster"},
    {"DIRECTN",  0x6B5, 0x695, Protocol::KWP_IS,  "Electric power steering (EPS)"},
    {"HDC",      0x742, 0x642, Protocol::KWP_HAB, "Steering-wheel COM2000"},
    {"BOITEVIT", 0x6A9, 0x689, Protocol::KWP_IS,  "Automatic gearbox"},
    {"SPNEU",    0x6B8, 0x698, Protocol::KWP_IS,  "Hydractive suspension"},
    {"DSG",      0x6AF, 0x68F, Protocol::KWP_IS,  "Tyre pressure (TPMS)"},
    {"TELEMAT",  0x764, 0x664, Protocol::UDS,     "Telematic / nav (RT3/NAC/SMEG)"},
    {"AUTORADIO",0x760, 0x660, Protocol::KWP_HAB, "Radio (RD4/RD45)"},
    {"AMPLHIFI", 0x77D, 0x67D, Protocol::KWP_HAB, "Amplifier"},
    {"CPL",      0x74A, 0x64A, Protocol::KWP_HAB, "Rain / light sensor"},
    {"BML",      0x741, 0x641, Protocol::KWP_HAB, "Lighting control"},
    {"ADC",      0x703, 0x70B, Protocol::KWP_IS,  "Immobiliser / key"},
    // Addresses below re-verified against ludwig-v/arduino-psa-diag ECU_LIST.md.
    // (The former BSM 0x700:0x708 row was dropped: the real BSM shares the BSI
    //  address 0x752:0x652, so it duplicated BMF above.)
    {"ALARME",   0x75C, 0x65C, Protocol::KWP_HAB, "Alarm system (ALARMES)"},
    {"MDP_CONDUCT", 0x756, 0x656, Protocol::KWP_HAB, "Driver door module (MDPLC_D)"},
    {"MDP_PASSAG",  0x755, 0x655, Protocol::KWP_HAB, "Passenger door module (MDPLC_G)"},
    {"PROJECTEURS", 0x6B7, 0x697, Protocol::KWP_HAB, "Directional headlamps (CORPRO)"},
    // Not found in ECU_LIST.md under these names — kept as best-effort, unverified:
    {"ECRAN_C",  0x770, 0x670, Protocol::KWP_HAB, "Multifunction screen (unverified)"},
    {"AIDE_STAT",0x76E, 0x66E, Protocol::KWP_HAB, "Parking assistance / CD (unverified)"},
};

inline constexpr size_t kEcuCount = sizeof(kEcuTable) / sizeof(kEcuTable[0]);

inline const EcuAddr* findEcu(uint16_t emit_id) {
    for (size_t i = 0; i < kEcuCount; ++i)
        if (kEcuTable[i].emit_id == emit_id) return &kEcuTable[i];
    return nullptr;
}

// --- KWP2000 service bytes (ISO 14230-3 on CAN) ------------------------------
namespace kwp {
inline constexpr uint8_t StartDiagnosticSession = 0x10; // long-form: 10C0
inline constexpr uint8_t StopDiagnosticSession  = 0x10; // long-form: 1081
inline constexpr uint8_t ReadDataByLocalId      = 0x21; // 21 XX
inline constexpr uint8_t WriteDataByLocalId     = 0x3B; // 3B XX ..  (HAB long-form)
inline constexpr uint8_t WriteDataByLocalId_IS  = 0x34; // 34 XX YYYY  (IS short-form)
inline constexpr uint8_t SecurityAccess         = 0x27; // 2781/2783/2782/2784
inline constexpr uint8_t ReadDTC                = 0x17; // 17FF00
inline constexpr uint8_t ClearDTC               = 0x14; // 14FF00
inline constexpr uint8_t StartRoutineByLocalId  = 0x31; // 31A800
inline constexpr uint8_t ECUReset               = 0x31; // 31A800/31A801
// IS short-form replaces StartSession=0x10 with 0x81 and StopSession with 0x82:
inline constexpr uint8_t StartSession_IS        = 0x81;
inline constexpr uint8_t StopSession_IS         = 0x82;
// Positive responses
inline constexpr uint8_t PosSession             = 0x50; // 50C0 (HAB) / C1XXXX (IS)
inline constexpr uint8_t PosRead                = 0x61;
inline constexpr uint8_t PosWrite               = 0x7B;  // response to 0x3B (HAB)
inline constexpr uint8_t PosWrite_IS            = 0x74;  // response to 0x34 (IS)
inline constexpr uint8_t PosSecuritySeed        = 0x67;
inline constexpr uint8_t PosSecurityKey         = 0x67;
inline constexpr uint8_t NegResponse            = 0x7F;
inline constexpr uint8_t KeepAlive              = 0x3E;
// Session IDs
inline constexpr uint8_t SessDiag               = 0xC0; // HAB diagnostic
inline constexpr uint8_t SessEnd                = 0x81; // HAB end
// SecurityAccess sub-functions
inline constexpr uint8_t SecSeedDownload        = 0x81;
inline constexpr uint8_t SecKeyDownload         = 0x82;
inline constexpr uint8_t SecSeedConfig          = 0x83;
inline constexpr uint8_t SecKeyConfig           = 0x84;
} // namespace kwp

// --- UDS service bytes (ISO 14229) -------------------------------------------
namespace uds {
inline constexpr uint8_t DiagnosticSessionControl = 0x10; // 1003 / 1002 / 1001
inline constexpr uint8_t SecurityAccess           = 0x27; // 2701/2703/2702/2704
inline constexpr uint8_t ReadDataByIdentifier     = 0x22; // 22 XXXX
inline constexpr uint8_t WriteDataByIdentifier    = 0x2E; // 2E XXXX ..
inline constexpr uint8_t ReadDTCInformation       = 0x19; // 190209
inline constexpr uint8_t ClearDiagnosticInformation = 0x14; // 14FFFFFF
inline constexpr uint8_t RoutineControl           = 0x31; // 3101FF00..
inline constexpr uint8_t RequestDownload          = 0x34; // 3481110000
inline constexpr uint8_t RequestTransferExit      = 0x37;
inline constexpr uint8_t ECUReset                 = 0x11; // 1103
inline constexpr uint8_t TesterPresent            = 0x3E; // 3E00
// Session IDs
inline constexpr uint8_t SessDiag                 = 0x03;
inline constexpr uint8_t SessDownload             = 0x02;
inline constexpr uint8_t SessEnd                  = 0x01;
// SecurityAccess sub-functions
inline constexpr uint8_t SecSeedDownload          = 0x01;
inline constexpr uint8_t SecKeyDownload           = 0x02;
inline constexpr uint8_t SecSeedConfig            = 0x03;
inline constexpr uint8_t SecKeyConfig             = 0x04;
// Positive response service bytes (= request | 0x40)
inline constexpr uint8_t PosSession               = 0x50;
inline constexpr uint8_t PosRead                  = 0x62;
inline constexpr uint8_t PosWrite                 = 0x6E;
inline constexpr uint8_t PosSecurity              = 0x67;
inline constexpr uint8_t PosClear                 = 0x54;
inline constexpr uint8_t PosRoutine               = 0x71;
inline constexpr uint8_t NegResponse              = 0x7F;
// Common NRCs (Negative Response Codes)
namespace NRC {
inline constexpr uint8_t GeneralReject             = 0x10;
inline constexpr uint8_t SubFunctionNotSupported   = 0x12;
inline constexpr uint8_t ConditionsNotCorrect      = 0x22;
inline constexpr uint8_t SecurityAccessDenied      = 0x33;
inline constexpr uint8_t RequestOutOfRange         = 0x31;
inline constexpr uint8_t RequestSequenceError      = 0x24;
inline constexpr uint8_t ExceededAttempts          = 0x36;
inline constexpr uint8_t RequiredTimeDelayExpired  = 0x37;
inline constexpr uint8_t InvalidKey                = 0x35; // ISO 14229; 0x13 is
                                                           // incorrectMessageLength
inline constexpr uint8_t BusyRepeatRequest         = 0x21;
inline constexpr uint8_t ResponsePending           = 0x78;
} // namespace NRC
} // namespace uds

// --- PSA Seed/Key (SecurityAccess) -------------------------------------------
// Reverse-engineered from PSA ECU firmware; constants fixed across the family.
// Reference: ludwig-v/psa-seedkey-algorithm (CIROCCO assembly trace).
namespace seed_key {

// The bitwise-OR combiner below is intentional and verified against the CIROCCO
// firmware trace (docs/psa_can_reference.md §4.4); each transform() output stays
// within 15 bits for any 16-bit input, so the OR never overflows int16_t.
inline long transform(uint8_t msb, uint8_t lsb, const uint8_t sec[3]) {
    long data = (static_cast<long>(msb) << 8) | lsb;
    long r = ((data % sec[0]) * sec[2]) - ((data / sec[0]) * sec[1]);
    if (r < 0) r += (sec[0] * sec[2]) + sec[1];
    return r;
}

inline constexpr uint8_t kSec1[3] = {0xB2, 0x3F, 0xAA};
inline constexpr uint8_t kSec2[3] = {0xB1, 0x02, 0xAB};

// pin = per-ECU 16-bit unlock key (see ECU_KEYS.md); seed = 32-bit from 2701/2703.
inline uint32_t compute(uint16_t pin, uint32_t seed) {
    long r_msb = transform(static_cast<uint8_t>(pin >> 8),
                           static_cast<uint8_t>(pin & 0xFF), kSec1)
               | transform(static_cast<uint8_t>(seed >> 24),
                           static_cast<uint8_t>(seed & 0xFF), kSec2);
    long r_lsb = transform(static_cast<uint8_t>((seed >> 16) & 0xFF),
                           static_cast<uint8_t>((seed >> 8) & 0xFF), kSec1)
               | transform(static_cast<uint8_t>(r_msb >> 8),
                           static_cast<uint8_t>(r_msb & 0xFF), kSec2);
    return static_cast<uint32_t>((r_msb << 16) | (r_lsb & 0xFFFF));
}

} // namespace seed_key

// --- Minimal request builders (produce raw service PDUs) ---------------------
// These return the PDU bytes that the ISO-TP layer then segments into CAN frames.
// Kept tiny: a Diagbox/PyPSADiag passthrough can send raw bytes too, so these are
// convenience, not a cage.
// buf must hold the largest PDU we ever build. That is TransferData:
// 0x36 + block-sequence-counter + one S-record payload (kMaxFlashBlock).
// It used to be 16, which a 32-byte S-record silently memcpy'd straight past.
inline constexpr size_t kMaxFlashBlock = 64;
struct Req {
    uint8_t buf[kMaxFlashBlock + 2];
    uint8_t len;
};

inline Req keepAlive(Protocol p) {
    Req r{{},0};
    if (p == Protocol::UDS)        { r.buf[0]=uds::TesterPresent; r.buf[1]=0x00; r.len=2; }
    else /* KWP_HAB / KWP_IS */    { r.buf[0]=kwp::KeepAlive; r.len=1; }
    return r;
}

inline Req startDiagSession(Protocol p) {
    Req r{{},0};
    if (p == Protocol::UDS)        { r.buf[0]=uds::DiagnosticSessionControl; r.buf[1]=uds::SessDiag; r.len=2; }
    else if (p == Protocol::KWP_IS){ r.buf[0]=kwp::StartSession_IS; r.len=1; }
    else /* KWP_HAB */             { r.buf[0]=kwp::StartDiagnosticSession; r.buf[1]=kwp::SessDiag; r.len=2; }
    return r;
}

// Programming session. An ECU will reject erase/download outright unless it has
// been moved out of the default/diagnostic session first, so the flash sequence
// must open this before anything else.
inline Req startProgrammingSession(Protocol p) {
    Req r{{},0};
    if (p == Protocol::UDS)        { r.buf[0]=uds::DiagnosticSessionControl; r.buf[1]=uds::SessDownload; r.len=2; }
    else if (p == Protocol::KWP_IS){ r.buf[0]=kwp::StartSession_IS; r.len=1; }
    else /* KWP_HAB */             { r.buf[0]=kwp::StartDiagnosticSession; r.buf[1]=0x85; r.len=2; }
    return r;
}

inline Req readDTC(Protocol p) {
    Req r{{},0};
    if (p == Protocol::UDS)        { r.buf[0]=uds::ReadDTCInformation; r.buf[1]=0x02; r.buf[2]=0x09; r.len=3; }
    else                           { r.buf[0]=kwp::ReadDTC; r.buf[1]=0xFF; r.buf[2]=0x00; r.len=3; }
    return r;
}

inline Req clearDTC(Protocol p) {
    Req r{{},0};
    if (p == Protocol::UDS)        { r.buf[0]=uds::ClearDiagnosticInformation; r.buf[1]=0xFF; r.buf[2]=0xFF; r.buf[3]=0xFF; r.len=4; }
    else                           { r.buf[0]=kwp::ClearDTC; r.buf[1]=0xFF; r.buf[2]=0x00; r.len=3; }
    return r;
}

// Request a security seed. Key reply must be sent back via securityKey().
inline Req securitySeed(Protocol p, bool config_access) {
    Req r{{},0};
    if (p == Protocol::UDS) {
        r.buf[0]=uds::SecurityAccess;
        r.buf[1]= config_access ? uds::SecSeedConfig : uds::SecSeedDownload;
        r.len=2;
    } else {
        r.buf[0]=kwp::SecurityAccess;
        r.buf[1]= config_access ? kwp::SecSeedConfig : kwp::SecSeedDownload;
        r.len=2;
    }
    return r;
}

// Build the security-key reply from a 32-bit seed using the per-ECU pin.
inline Req securityKey(Protocol p, bool config_access, uint16_t pin, uint32_t seed) {
    uint32_t key = seed_key::compute(pin, seed);
    Req r{{},0};
    uint8_t sub = config_access
        ? (p == Protocol::UDS ? uds::SecKeyConfig  : kwp::SecKeyConfig)
        : (p == Protocol::UDS ? uds::SecKeyDownload: kwp::SecKeyDownload);
    r.buf[0]= (p == Protocol::UDS) ? uds::SecurityAccess : kwp::SecurityAccess;
    r.buf[1]= sub;
    r.buf[2]= static_cast<uint8_t>(key >> 24);
    r.buf[3]= static_cast<uint8_t>(key >> 16);
    r.buf[4]= static_cast<uint8_t>(key >> 8);
    r.buf[5]= static_cast<uint8_t>(key & 0xFF);
    r.len=6;
    return r;
}

inline Req stopDiagSession(Protocol p) {
    Req r{{},0};
    if (p == Protocol::UDS)        { r.buf[0]=uds::DiagnosticSessionControl; r.buf[1]=uds::SessEnd; r.len=2; }
    else if (p == Protocol::KWP_IS){ r.buf[0]=kwp::StopSession_IS; r.len=1; }
    else /* KWP_HAB */             { r.buf[0]=kwp::StopDiagnosticSession; r.buf[1]=kwp::SessEnd; r.len=2; }
    return r;
}

// Read a zone/DID. KWP uses 1-byte local IDs (21 XX), UDS uses 2-byte DIDs (22 XXXX).
inline Req readZone(Protocol p, uint16_t zone_id) {
    Req r{{},0};
    // BSI config zones and all UDS DIDs are 2-byte identifiers.
    // CAN2004 ECUs (BSI included) accept UDS-style 22 XXXX reads even during KWP
    // sessions. Auto-detect: zone_id > 0xFF -> use UDS framing.
    bool use_uds = (p == Protocol::UDS || zone_id > 0xFF);
    if (use_uds) {
        r.buf[0]=uds::ReadDataByIdentifier;
        r.buf[1]=static_cast<uint8_t>(zone_id >> 8);
        r.buf[2]=static_cast<uint8_t>(zone_id & 0xFF);
        r.len=3;
    } else {
        r.buf[0]=kwp::ReadDataByLocalId;
        r.buf[1]=static_cast<uint8_t>(zone_id & 0xFF);
        r.len=2;
    }
    return r;
}

inline Req readLiveData(Protocol p, uint16_t param_id) {
    return readZone(p, param_id);
}

inline Req readEcuIdentification(Protocol p) {
    // Read VIN - most common identification request
    return readZone(p, (p == Protocol::UDS) ? 0xF190 : 0x80);
}

inline Req startActuatorTest(Protocol p, uint16_t test_id, const uint8_t* args = nullptr, size_t args_len = 0) {
    Req r{{}, 0};
    if (p == Protocol::UDS) {
        r.buf[0] = uds::RoutineControl;
        r.buf[1] = 0x01; // StartRoutine
        r.buf[2] = static_cast<uint8_t>(test_id >> 8);
        r.buf[3] = static_cast<uint8_t>(test_id & 0xFF);
        r.len = 4;
    } else {
        // KWP: StartRoutineByLocalId (30 XX or 31 XX)
        // If upper byte is 0x30 or 0x31, use it. Otherwise default to 0x31.
        uint8_t service = static_cast<uint8_t>(test_id >> 8);
        if (service != 0x30 && service != 0x31) {
            service = 0x31;
        }
        r.buf[0] = service;
        r.buf[1] = static_cast<uint8_t>(test_id & 0xFF);
        r.len = 2;
    }
    if (args && args_len > 0) {
        for (size_t i = 0; i < args_len && r.len < sizeof(r.buf); ++i) {
            r.buf[r.len++] = args[i];
        }
    }
    return r;
}

// Write a zone. KWP: 3B XX data...; UDS: 2E XXXX data...
// Caller must append the zone data bytes after calling this.
inline Req writeZoneHeader(Protocol p, uint16_t zone_id) {
    Req r{{},0};
    // Auto-detect: 2-byte DIDs use UDS format (2E XXXX), 1-byte KWP local IDs
    // use the appropriate KWP write service for the bus variant.
    bool use_uds = (p == Protocol::UDS || zone_id > 0xFF);
    if (use_uds) {
        r.buf[0]=uds::WriteDataByIdentifier;
        r.buf[1]=static_cast<uint8_t>(zone_id >> 8);
        r.buf[2]=static_cast<uint8_t>(zone_id & 0xFF);
        r.len=3;
    } else if (p == Protocol::KWP_IS) {
        r.buf[0]=kwp::WriteDataByLocalId_IS; // 0x34 for IS short-form
        r.buf[1]=static_cast<uint8_t>(zone_id & 0xFF);
        r.len=2;
    } else {
        r.buf[0]=kwp::WriteDataByLocalId; // 0x3B for HAB long-form
        r.buf[1]=static_cast<uint8_t>(zone_id & 0xFF);
        r.len=2;
    }
    return r;
}

// --- Common zone / DID identifiers -------------------------------------------
namespace zone {
// UDS DIDs (ISO 14229 Annex B + PSA-specific)
inline constexpr uint16_t VIN                  = 0xF190;
inline constexpr uint16_t ECU_SERIAL           = 0xF18C;
inline constexpr uint16_t SYSTEM_SUPPLIER      = 0xF18A;
inline constexpr uint16_t HW_VERSION           = 0xF191;
inline constexpr uint16_t SW_VERSION           = 0xF194;
inline constexpr uint16_t PROGRAMMING_DATE     = 0xF199;
inline constexpr uint16_t DIAG_VARIANT         = 0xF1A0;
inline constexpr uint16_t TRACEABILITY         = 0x2901;
inline constexpr uint16_t TRACEABILITY_COUNTER = 0xC000;
// KWP local IDs (1-byte, commonly used on C5 Mk1 FL)
inline constexpr uint16_t KWP_VIN             = 0x80;
inline constexpr uint16_t KWP_ECU_ID          = 0x91;
inline constexpr uint16_t KWP_SW_VER          = 0x94;
inline constexpr uint16_t KWP_SUPPLIER        = 0x8A;
inline constexpr uint16_t KWP_TRACEABILITY    = 0xA0;
} // namespace zone

} // namespace psa

