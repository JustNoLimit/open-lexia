// Guided ("commanded") CAN sniffer — correlates a known physical action (N button
// presses, a held setpoint, a slow sweep) with the CAN bytes that move in lockstep,
// so unknown broadcast signals can be identified without a factory reference.
// Reference: docs/psa_can_reference.md section 2 (broadcast ID tables).
#pragma once
#include <cstdint>
#include <cstddef>
#include "psa/can_manager.hpp"
#include "psa/isotp.hpp"

namespace psa {

class CanSniffer {
public:
    enum class Mode : uint8_t { Idle, Baseline, Count, Hold, Sweep };

    // One step of a guided scenario: a prompt to show the user, the capture mode
    // that step's action implies, and (for Count) how many times the action repeats.
    struct Step {
        const char* prompt;
        Mode        mode;
        uint8_t     expected;
        const char* label;
    };
    struct Scenario {
        const char* name;
        const Step* steps;
        uint8_t     count;
    };

    struct Learned {
        char     label[24];
        Bus      bus;
        uint16_t id;
        uint8_t  byte;
        uint16_t score; // 0 = perfect signature match
    };

    static constexpr size_t   kMaxSlots            = 64;
    static constexpr size_t   kMaxLearned           = 32;
    static constexpr uint64_t kDefaultBaselineUs    = 3'000'000;

    void init();

    // Feed one CAN frame observed while a window (baseline/count/hold/sweep) is
    // active, or while a `watch` is armed. No-op otherwise.
    void feed(Bus bus, const CanFrame& f);

    // Call every main-loop iteration; ends the baseline window on its deadline.
    void tick(uint64_t now_us);

    void beginBaseline(uint64_t now_us, uint64_t duration_us = kDefaultBaselineUs);
    void beginCount(uint8_t expected);
    void beginHold();
    void beginSweep();
    void stop();     // ends the active ad-hoc window and prints the report
    void report() const;
    void clear();
    void status() const;

    void watch(Bus bus, uint16_t id);
    void watchOff();

    void beginRun(const Scenario* scenario, uint64_t now_us);
    void nextStep(uint64_t now_us);

    bool active() const { return mode_ != Mode::Idle; }
    bool scenarioActive() const { return scenario_ != nullptr; }
    bool isWatching() const { return watching_; }
    Mode mode() const { return mode_; }

    // Best-scoring candidate for the last completed ad-hoc window (after stop()).
    // Returns false if no window has completed yet or nothing qualified.
    bool bestCandidate(Bus* bus, uint16_t* id, uint8_t* byte, uint16_t* score) const;

    uint8_t        learnedCount() const { return learned_count_; }
    const Learned& learnedAt(uint8_t i) const { return learned_[i]; }

#ifndef HOST_TEST
    bool save() const;
    bool load();
#endif

private:
    struct Slot {
        Bus      bus;
        uint16_t id;
        uint8_t  dlc;
        uint8_t  last[8];
        uint8_t  base_val[8];   // value the byte parked at when baseline ended
        uint8_t  changes[8];    // transitions seen in the current window
        uint8_t  rises[8];
        uint8_t  falls[8];
        uint8_t  minv[8];
        uint8_t  maxv[8];
        uint8_t  noisy_mask;    // bit b set => byte b moved on its own during baseline
    };

    struct Candidate {
        uint8_t  slot;
        uint8_t  byte;
        uint16_t score;
    };

    Slot     slots_[kMaxSlots];
    uint8_t  slot_count_ = 0;
    bool     table_full_warned_ = false;

    Mode     mode_ = Mode::Idle;
    uint8_t  expected_ = 0;
    uint64_t phase_start_us_ = 0;
    uint64_t phase_duration_us_ = 0;

    Mode     last_mode_ = Mode::Idle;   // window mode `report()` describes after stop()
    uint8_t  last_expected_ = 0;

    bool     watching_ = false;
    Bus      watch_bus_ = Bus::HighSpeed;
    uint16_t watch_id_ = 0;

    const Scenario* scenario_ = nullptr;
    uint8_t         scenario_step_ = 0;
    Learned         learned_[kMaxLearned];
    uint8_t         learned_count_ = 0;

    Slot* findOrCreate(Bus bus, const CanFrame& f);
    void  resetWindowCounters();
    void  finishBaseline();
    void  startScenarioStep(uint8_t idx);
    void  printReportFor(Mode mode, uint8_t expected) const;
    // Fills `out` (capacity max_out) with the best-scoring candidates for the
    // given mode/expected signature, ascending by score (0 = best). Returns count.
    uint8_t collectTop(Mode mode, uint8_t expected, Candidate* out, uint8_t max_out) const;
    // True if byte `b` of `s` is a candidate under `mode`; fills `score`
    // (0 = perfect match, larger = worse). Noisy (baseline-unstable) bytes never qualify.
    static bool matchScore(Mode mode, const Slot& s, uint8_t b, uint8_t expected, uint16_t* score);
};

// --- Guided scenarios (pure data — add a subsystem by adding a table) --------

inline constexpr CanSniffer::Step kClimateSteps[] = {
    {"Sofor sicakligi: AZALT'a 5 kez bas", CanSniffer::Mode::Count, 5, "drv_temp_down"},
    {"Sofor sicakligi: ARTTIR'a 5 kez bas", CanSniffer::Mode::Count, 5, "drv_temp_up"},
    {"Yolcu sicakligi: AZALT'a 5 kez bas", CanSniffer::Mode::Count, 5, "pass_temp_down"},
    {"Yolcu sicakligi: ARTTIR'a 5 kez bas", CanSniffer::Mode::Count, 5, "pass_temp_up"},
    {"Fan hizi: ARTTIR'a 4 kez bas", CanSniffer::Mode::Count, 4, "fan_up"},
    {"Fan hizi: AZALT'a 4 kez bas", CanSniffer::Mode::Count, 4, "fan_down"},
    {"AUTO'ya 1 kez bas", CanSniffer::Mode::Count, 1, "auto_toggle"},
    {"A/C ac/kapa 1 kez bas", CanSniffer::Mode::Count, 1, "ac_toggle"},
    {"Hava yonunu 3 kez degistir", CanSniffer::Mode::Count, 3, "air_direction"},
    {"Devirdaim (recycle) ac/kapa 1 kez bas", CanSniffer::Mode::Count, 1, "recycle"},
};
inline constexpr CanSniffer::Scenario kClimateScenario{
    "climate", kClimateSteps, sizeof(kClimateSteps) / sizeof(kClimateSteps[0])
};

inline constexpr const CanSniffer::Scenario* kAllScenarios[] = { &kClimateScenario };
inline constexpr size_t kScenarioCount = sizeof(kAllScenarios) / sizeof(kAllScenarios[0]);

// Case-insensitive lookup by name (mirrors DiagShell::cmdConnect's ECU match).
inline const CanSniffer::Scenario* findScenario(const char* name) {
    if (!name || !*name) return nullptr;
    for (size_t i = 0; i < kScenarioCount; ++i) {
        const char* a = name;
        const char* b = kAllScenarios[i]->name;
        bool match = true;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (*a - 'A' + 'a') : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (*b - 'A' + 'a') : *b;
            if (ca != cb) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') return kAllScenarios[i];
    }
    return nullptr;
}

} // namespace psa
