// Actuator-test catalog — the NAMES of the component tests Diagbox/Lexia3
// exposes, grouped by ECU function. Source: docs/lexia3_menu_reference.md
// §4.1.10 (BSI actuator tests).
//
// IMPORTANT: the RoutineControl IDs behind these tests are PSA-proprietary and
// NOT known here. This is a reference checklist only — it does NOT carry a
// runnable ID, precisely because firing the wrong actuator ID on a real car is
// unsafe. Discover the real ID on the vehicle (gsniff/raw while Diagbox runs
// the test, or documented sources), then run it with `actuator <id>` and, once
// confirmed, promote the entry with its ID.
#pragma once
#include <cstddef>

namespace psa {

struct ActuatorTest {
    const char* ecu;    // ECU family the test lives under
    const char* group;  // sub-menu grouping
    const char* name;   // human name as shown by Lexia3
};

// Only the tests Lexia3 actually enumerates are listed. Other ECUs (COMBINE,
// AIDE_STAT, ALARME...) expose an "Actuator Tests" menu whose items the
// reference capture didn't detail — add them here as they're confirmed.
inline constexpr ActuatorTest kActuatorCatalog[] = {
    {"BSI", "Lighting / Signalling", "Horn"},
    {"BSI", "Lighting / Signalling", "Dipped beam"},
    {"BSI", "Lighting / Signalling", "Main headlamps"},
    {"BSI", "Lighting / Signalling", "LH side lamps (vehicle + trailer)"},
    {"BSI", "Lighting / Signalling", "RH side lamps (vehicle + trailer)"},
    {"BSI", "Lighting / Signalling", "LH indicators (vehicle + trailer)"},
    {"BSI", "Lighting / Signalling", "RH indicators (vehicle + trailer)"},
    {"BSI", "Lighting / Signalling", "Rear fog lamps (vehicle + trailer)"},
    {"BSI", "Lighting / Signalling", "Interior lighting"},
};
inline constexpr size_t kActuatorCatalogCount =
    sizeof(kActuatorCatalog) / sizeof(kActuatorCatalog[0]);

} // namespace psa
