// Guided CAN sniffer — implementation. Pure logic, no hardware access except
// the optional flash persistence (compiled out under HOST_TEST).
#include "psa/can_sniffer.hpp"
#include <cstdio>
#include <cstring>

namespace psa {

namespace {
const char* busName(Bus b) { return b == Bus::HighSpeed ? "HS" : "LS"; }
} // namespace

void CanSniffer::init() {
    slot_count_ = 0;
    table_full_warned_ = false;
    mode_ = Mode::Idle;
    expected_ = 0;
    last_mode_ = Mode::Idle;
    last_expected_ = 0;
    watching_ = false;
    scenario_ = nullptr;
    scenario_step_ = 0;
    learned_count_ = 0;
}

CanSniffer::Slot* CanSniffer::findOrCreate(Bus bus, const CanFrame& f) {
    for (uint8_t i = 0; i < slot_count_; ++i) {
        if (slots_[i].bus == bus && slots_[i].id == f.id) return &slots_[i];
    }
    if (slot_count_ >= kMaxSlots) {
        if (!table_full_warned_) {
            table_full_warned_ = true;
            printf("[GSNIFF] Slot tablosu dolu (%zu ID) - fazlasi yoksayiliyor.\n", kMaxSlots);
        }
        return nullptr;
    }
    Slot& s = slots_[slot_count_++];
    s.bus = bus;
    s.id = f.id;
    s.dlc = f.dlc;
    uint8_t dlc = f.dlc > 8 ? 8 : f.dlc;
    for (uint8_t b = 0; b < 8; ++b) {
        uint8_t v = (b < dlc) ? f.data[b] : 0;
        s.last[b] = v;
        s.base_val[b] = v;   // best-effort default for IDs first seen outside baseline
        s.minv[b] = v;
        s.maxv[b] = v;
        s.changes[b] = 0;
        s.rises[b] = 0;
        s.falls[b] = 0;
    }
    s.noisy_mask = 0;
    return &s;
}

void CanSniffer::resetWindowCounters() {
    for (uint8_t i = 0; i < slot_count_; ++i) {
        Slot& s = slots_[i];
        for (uint8_t b = 0; b < 8; ++b) {
            s.changes[b] = 0;
            s.rises[b] = 0;
            s.falls[b] = 0;
            s.minv[b] = s.last[b];
            s.maxv[b] = s.last[b];
        }
    }
}

void CanSniffer::feed(Bus bus, const CanFrame& f) {
    if (watching_ && bus == watch_bus_ && f.id == watch_id_) {
        printf("[GSNIFF] watch %s %03X:", busName(bus), f.id);
        for (uint8_t b = 0; b < f.dlc && b < 8; ++b) printf(" %02X", f.data[b]);
        printf("\n");
    }

    if (mode_ == Mode::Idle) return;

    Slot* s = findOrCreate(bus, f);
    if (!s) return;

    uint8_t dlc = f.dlc > 8 ? 8 : f.dlc;
    for (uint8_t b = 0; b < dlc; ++b) {
        uint8_t v = f.data[b];
        uint8_t old = s->last[b];
        if (v != old) {
            if (mode_ == Mode::Baseline) {
                s->noisy_mask |= static_cast<uint8_t>(1u << b);
            } else {
                s->changes[b]++;
                if (v > old) s->rises[b]++; else s->falls[b]++;
            }
            s->last[b] = v;
        }
        if (v < s->minv[b]) s->minv[b] = v;
        if (v > s->maxv[b]) s->maxv[b] = v;
    }
}

void CanSniffer::tick(uint64_t now_us) {
    if (mode_ != Mode::Baseline) return;
    if (now_us - phase_start_us_ < phase_duration_us_) return;
    finishBaseline();
}

void CanSniffer::beginBaseline(uint64_t now_us, uint64_t duration_us) {
    mode_ = Mode::Baseline;
    phase_start_us_ = now_us;
    phase_duration_us_ = duration_us;
    resetWindowCounters();
    printf("[GSNIFF] Baseline basladi (%llusn) - araci sabit tut, dokunma...\n",
           static_cast<unsigned long long>(duration_us / 1000000ULL));
}

void CanSniffer::finishBaseline() {
    for (uint8_t i = 0; i < slot_count_; ++i) {
        Slot& s = slots_[i];
        for (uint8_t b = 0; b < 8; ++b) s.base_val[b] = s.last[b];
    }
    uint16_t noisy_bytes = 0;
    for (uint8_t i = 0; i < slot_count_; ++i) {
        uint8_t m = slots_[i].noisy_mask;
        while (m) { noisy_bytes += (m & 1); m >>= 1; }
    }
    printf("[GSNIFF] Baseline tamam - %u ID izleniyor, %u bayt gurultulu (maskeli).\n",
           slot_count_, noisy_bytes);
    resetWindowCounters();
    mode_ = Mode::Idle;

    if (scenario_ != nullptr && scenario_step_ == 0) {
        startScenarioStep(0);
    }
}

void CanSniffer::beginCount(uint8_t expected) {
    mode_ = Mode::Count;
    expected_ = expected;
    resetWindowCounters();
    printf("[GSNIFF] Sayili yakalama basladi (beklenen=%u). Eylemi yap, sonra 'gsniff stop'.\n", expected);
}

void CanSniffer::beginHold() {
    mode_ = Mode::Hold;
    expected_ = 0;
    resetWindowCounters();
    printf("[GSNIFF] Hold yakalama basladi. Degeri SABIT tut, sonra 'gsniff stop'.\n");
}

void CanSniffer::beginSweep() {
    mode_ = Mode::Sweep;
    expected_ = 0;
    resetWindowCounters();
    printf("[GSNIFF] Sweep yakalama basladi. Buyuklugu yavasca ve TEK YONDE degistir, sonra 'gsniff stop'.\n");
}

void CanSniffer::stop() {
    if (mode_ == Mode::Baseline) {
        finishBaseline();
        return;
    }
    if (mode_ == Mode::Idle) {
        printf("[GSNIFF] Aktif yakalama yok.\n");
        return;
    }
    last_mode_ = mode_;
    last_expected_ = expected_;
    mode_ = Mode::Idle;
    report();
}

bool CanSniffer::matchScore(Mode mode, const Slot& s, uint8_t b, uint8_t expected, uint16_t* score) {
    if (s.noisy_mask & (1u << b)) return false;

    switch (mode) {
    case Mode::Count: {
        if (s.changes[b] == 0) return false;
        int d = static_cast<int>(s.changes[b]) - static_cast<int>(expected);
        *score = static_cast<uint16_t>(d < 0 ? -d : d);
        return true;
    }
    case Mode::Hold: {
        // minv/maxv include the pre-window baseline sample, so a value that only
        // ever rose (or only ever fell) from baseline would keep one bound pinned
        // to base_val — comparing the *current* held value is what actually means
        // "parked somewhere new", regardless of which direction it moved.
        if (s.last[b] == s.base_val[b]) return false;
        *score = s.changes[b]; // 0 = rock steady at the new value throughout the window
        return true;
    }
    case Mode::Sweep: {
        if (s.changes[b] < 2) return false;
        bool up = s.rises[b] > 0 && s.falls[b] == 0;
        bool down = s.falls[b] > 0 && s.rises[b] == 0;
        if (!up && !down) return false;
        uint16_t total = static_cast<uint16_t>(s.rises[b] + s.falls[b]);
        *score = total >= 250 ? 0 : static_cast<uint16_t>(250 - total);
        return true;
    }
    default:
        return false;
    }
}

uint8_t CanSniffer::collectTop(Mode mode, uint8_t expected, Candidate* out, uint8_t max_out) const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < slot_count_; ++i) {
        const Slot& s = slots_[i];
        for (uint8_t b = 0; b < 8; ++b) {
            uint16_t score;
            if (!matchScore(mode, s, b, expected, &score)) continue;

            if (count < max_out) {
                uint8_t pos = count;
                while (pos > 0 && out[pos - 1].score > score) {
                    out[pos] = out[pos - 1];
                    pos--;
                }
                out[pos] = Candidate{i, b, score};
                count++;
            } else if (score < out[max_out - 1].score) {
                uint8_t pos = max_out - 1;
                while (pos > 0 && out[pos - 1].score > score) {
                    out[pos] = out[pos - 1];
                    pos--;
                }
                out[pos] = Candidate{i, b, score};
            }
        }
    }
    return count;
}

void CanSniffer::printReportFor(Mode mode, uint8_t expected) const {
    static constexpr uint8_t kTopN = 10;
    Candidate top[kTopN];
    uint8_t n = collectTop(mode, expected, top, kTopN);
    if (n == 0) {
        printf("[GSNIFF] Aday bulunamadi (hepsi gurultulu ya da hic degismedi).\n");
        return;
    }
    for (uint8_t i = 0; i < n; ++i) {
        const Slot& s = slots_[top[i].slot];
        uint8_t b = top[i].byte;
        bool exact = top[i].score == 0;
        printf("[GSNIFF] %s %03X b%u chg=%u min=%u max=%u base=%u %s\n",
               busName(s.bus), s.id, b, s.changes[b], s.minv[b], s.maxv[b], s.base_val[b],
               exact ? "*" : "");
    }
}

bool CanSniffer::bestCandidate(Bus* bus, uint16_t* id, uint8_t* byte, uint16_t* score) const {
    if (last_mode_ == Mode::Idle) return false;
    Candidate top{};
    if (collectTop(last_mode_, last_expected_, &top, 1) == 0) return false;
    const Slot& s = slots_[top.slot];
    *bus = s.bus;
    *id = s.id;
    *byte = top.byte;
    *score = top.score;
    return true;
}

void CanSniffer::report() const {
    if (last_mode_ == Mode::Idle) {
        printf("[GSNIFF] Henuz bir yakalama tamamlanmadi.\n");
        return;
    }
    printReportFor(last_mode_, last_expected_);
}

void CanSniffer::clear() {
    slot_count_ = 0;
    table_full_warned_ = false;
    mode_ = Mode::Idle;
    last_mode_ = Mode::Idle;
    watching_ = false;
    scenario_ = nullptr;
    scenario_step_ = 0;
    learned_count_ = 0;
    printf("[GSNIFF] Tablo temizlendi.\n");
}

void CanSniffer::status() const {
    const char* mode_str = "idle";
    switch (mode_) {
        case Mode::Baseline: mode_str = "baseline"; break;
        case Mode::Count:    mode_str = "count";    break;
        case Mode::Hold:     mode_str = "hold";     break;
        case Mode::Sweep:    mode_str = "sweep";    break;
        default: break;
    }
    printf("[GSNIFF] mod=%s izlenen_id=%u ogrenilen=%u senaryo=%s\n",
           mode_str, slot_count_, learned_count_,
           scenario_ ? scenario_->name : "(yok)");
}

void CanSniffer::watch(Bus bus, uint16_t id) {
    watching_ = true;
    watch_bus_ = bus;
    watch_id_ = id;
    printf("[GSNIFF] watch acik: %s %03X\n", busName(bus), id);
}

void CanSniffer::watchOff() {
    watching_ = false;
    printf("[GSNIFF] watch kapatildi.\n");
}

void CanSniffer::startScenarioStep(uint8_t idx) {
    scenario_step_ = idx;
    resetWindowCounters();
    const Step& st = scenario_->steps[idx];
    mode_ = st.mode;
    expected_ = st.expected;
    printf("[GSNIFF] Adim %u/%u: %s\n", idx + 1, scenario_->count, st.prompt);
    printf("[GSNIFF]   Bitince: 'gsniff next' (bos ENTER da olur)\n");
}

void CanSniffer::beginRun(const Scenario* scenario, uint64_t now_us) {
    if (!scenario) {
        printf("[GSNIFF] Bilinmeyen senaryo.\n");
        return;
    }
    scenario_ = scenario;
    scenario_step_ = 0;
    learned_count_ = 0;
    printf("[GSNIFF] '%s' senaryosu basliyor.\n", scenario->name);
    beginBaseline(now_us);
}

void CanSniffer::nextStep(uint64_t now_us) {
    (void)now_us;
    if (scenario_ == nullptr) {
        printf("[GSNIFF] Aktif senaryo yok. 'gsniff run <isim>' ile basla.\n");
        return;
    }
    if (mode_ == Mode::Baseline) {
        printf("[GSNIFF] Baseline hala calisiyor, bekle.\n");
        return;
    }

    const Step& st = scenario_->steps[scenario_step_];
    Candidate best{};
    uint8_t n = collectTop(mode_, expected_, &best, 1);
    if (n > 0 && learned_count_ < kMaxLearned) {
        const Slot& s = slots_[best.slot];
        Learned& l = learned_[learned_count_++];
        std::snprintf(l.label, sizeof(l.label), "%s", st.label);
        l.bus = s.bus;
        l.id = s.id;
        l.byte = best.byte;
        l.score = best.score;
        printf("[GSNIFF] OGRENILDI %s = %s %03X b%u (chg=%u)\n",
               l.label, busName(l.bus), l.id, l.byte, s.changes[best.byte]);
    } else {
        printf("[GSNIFF] OGRENILDI %s = YOK (net aday bulunamadi)\n", st.label);
    }

    uint8_t next_idx = static_cast<uint8_t>(scenario_step_ + 1);
    if (next_idx < scenario_->count) {
        startScenarioStep(next_idx);
        return;
    }

    printf("[GSNIFF] '%s' senaryosu tamamlandi. Ogrenilen harita (%u):\n",
           scenario_->name, learned_count_);
    for (uint8_t i = 0; i < learned_count_; ++i) {
        const Learned& l = learned_[i];
        printf("[GSNIFF]   %-18s = %s %03X b%u\n", l.label, busName(l.bus), l.id, l.byte);
    }
    mode_ = Mode::Idle;
    scenario_ = nullptr;
}

#ifndef HOST_TEST
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/error.h"

namespace {
constexpr uint32_t kFlashMagic = 0x474E5331; // "GNS1"
constexpr uint32_t kStoreOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

struct StoreHeader {
    uint32_t magic;
    uint32_t count;
};

struct SaveCtx {
    const CanSniffer::Learned* entries;
    uint32_t count;
};

void doErase(void*) {
    flash_range_erase(kStoreOffset, FLASH_SECTOR_SIZE);
}

void doProgram(void* param) {
    auto* ctx = static_cast<SaveCtx*>(param);
    static uint8_t buf[FLASH_SECTOR_SIZE];
    std::memset(buf, 0, sizeof(buf));
    StoreHeader hdr{kFlashMagic, ctx->count};
    std::memcpy(buf, &hdr, sizeof(hdr));
    std::memcpy(buf + sizeof(hdr), ctx->entries, ctx->count * sizeof(CanSniffer::Learned));
    flash_range_erase(kStoreOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kStoreOffset, buf, sizeof(buf));
}
} // namespace

bool CanSniffer::save() const {
    SaveCtx ctx{learned_, learned_count_};
    int rc = flash_safe_execute(doProgram, &ctx, 1000);
    if (rc != PICO_OK) {
        printf("[GSNIFF] Flash yazma basarisiz (rc=%d).\n", rc);
        return false;
    }
    printf("[GSNIFF] %u ogrenilen sinyal flash'a kaydedildi.\n", learned_count_);
    return true;
}

bool CanSniffer::load() {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(XIP_BASE + kStoreOffset);
    StoreHeader hdr;
    std::memcpy(&hdr, p, sizeof(hdr));
    if (hdr.magic != kFlashMagic || hdr.count > kMaxLearned) {
        printf("[GSNIFF] Kayitli sinyal haritasi yok.\n");
        return false;
    }
    learned_count_ = static_cast<uint8_t>(hdr.count);
    std::memcpy(learned_, p + sizeof(hdr), learned_count_ * sizeof(Learned));
    printf("[GSNIFF] %u ogrenilen sinyal flash'tan yuklendi.\n", learned_count_);
    return true;
}
#endif // !HOST_TEST

} // namespace psa
