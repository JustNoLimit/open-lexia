// ECU PIN keys for PSA SecurityAccess.
#pragma once
#include <cstdint>
#include <cstring>
#include <cctype>

namespace psa {

// The SecurityAccess "PIN" is a 16-bit secret used to turn the ECU's 32-bit seed
// into the 32-bit key (see seed_key::compute). IMPORTANT: in the real PSA data
// (ludwig-v/psa-seedkey-algorithm ECU_KEYS.md) the PIN is per *exact ECU model /
// supplier*, NOT per family — e.g. the BSI family alone has 8 distinct keys
// (VALEO=B2B2, JCAE=B0B0, BSI_2010 VALEO=E4D8, ...) and INJ has 50+.
//
// So a family lookup can only ever be a best-effort default for the most common
// C5 Mk1 FL variant. Only the values below are taken verbatim from that source;
// every other family returns 0x0000 ("unknown"). When it's 0x0000 (or the fitted
// ECU is a different variant), supply the correct PIN at runtime with the shell's
// `pin <hex>` command — look it up in ECU_KEYS.md for your exact ECU part number.
inline uint16_t getEcuPin(const char* family) {
    if (!family) return 0x0000;

    auto equals = [](const char* s1, const char* s2) {
        while (*s1 && *s2) {
            if (std::tolower(static_cast<unsigned char>(*s1)) != std::tolower(static_cast<unsigned char>(*s2))) {
                return false;
            }
            s1++;
            s2++;
        }
        return *s1 == '\0' && *s2 == '\0';
    };

    // BSI / central gateway — default to the VALEO BSI variant (common on C5 FL).
    if (equals(family, "BMF") || equals(family, "BSI")) return 0xB2B2;
    // Engine ECU — default to EDC16C3 (the 2.0 HDi found on most C5 Mk1 FL).
    if (equals(family, "INJ")) return 0x475A;
    // Telematic / nav — NAC head unit.
    if (equals(family, "TELEMAT")) return 0xD91C;

    // Below: added 2026-07-18 from ludwig-v/psa-seedkey-algorithm ECU_KEYS.md.
    // High confidence (exact model-name match to the family in this project):
    if (equals(family, "AIRBAG")) return 0xB2DF;   // SAC_AUTOLIV
    if (equals(family, "CPL")) return 0xEE3E;      // CDPL (rain/light sensor)
    if (equals(family, "ECRAN_C")) return 0xF6C4;  // EMF_C

    // Best-effort guess (source lists several model variants per family; picked
    // the one matching this project's KWP2000/C5-Mk1-FL era, not the newer UDS
    // ones). Verify with `pin <hex>` against the real unlock result.
    if (equals(family, "BOITEVIT")) return 0x8962; // AL4_AT8 (KWP-era AL4 auto gearbox)
    if (equals(family, "AMPLHIFI")) return 0xA7D8; // AMPLI_AUDIO
    if (equals(family, "DIRECTN")) return 0xBF62;  // DAE (KWP-era EPS, not *_UDS2 variants)
    if (equals(family, "ABRASR")) return 0xABFB;   // ESP81 (pre-2010 Bosch ESP, closest to MK60 era)
    if (equals(family, "DSG")) return 0xAC58;      // DSG_UDS — weakest guess, source only lists a UDS variant

    // Everything else: no source-verified family default. Use `pin <hex>`.
    return 0x0000;
}

} // namespace psa
