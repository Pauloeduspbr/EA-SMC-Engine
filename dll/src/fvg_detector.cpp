#include "fvg_detector.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// FVGDetector implementation
// Port of smc.py fvg() — lines 56-134
// ============================================================================

FVGDetector::FVGDetector()
    : join_consecutive_(false)
{}

void FVGDetector::Init(bool join_consecutive) {
    join_consecutive_ = join_consecutive;
    Reset();
}

void FVGDetector::Reset() {
    fvgs_.clear();
}

void FVGDetector::Calculate(const std::vector<Bar>& bars) {
    Reset();

    int n = static_cast<int>(bars.size());
    if (n < 3) return;

    DetectFVGs(bars);

    if (join_consecutive_) {
        JoinConsecutive();
    }

    TrackMitigation(bars);
}

void FVGDetector::DetectFVGs(const std::vector<Bar>& bars) {
    int n = static_cast<int>(bars.size());

    // FVG detection: check bars 1..n-2 (need bar before and after)
    for (int i = 1; i < n - 1; ++i) {
        bool is_bullish_candle = bars[i].close > bars[i].open;
        bool is_bearish_candle = bars[i].close < bars[i].open;

        // Bullish FVG: previous high < next low AND current is bullish
        if (bars[i - 1].high < bars[i + 1].low && is_bullish_candle) {
            FairValueGap fvg{};
            fvg.index = i;
            fvg.top = bars[i + 1].low;         // next candle's low
            fvg.bottom = bars[i - 1].high;      // previous candle's high
            fvg.direction = 1;
            fvg.midpoint = (fvg.top + fvg.bottom) / 2.0;
            fvg.mitigated = false;
            fvg.mitigated_index = 0;
            fvg.time = bars[i].time;
            fvg.visit_count = 0;
            fvgs_.push_back(fvg);
        }
        // Bearish FVG: previous low > next high AND current is bearish
        else if (bars[i - 1].low > bars[i + 1].high && is_bearish_candle) {
            FairValueGap fvg{};
            fvg.index = i;
            fvg.top = bars[i - 1].low;          // previous candle's low
            fvg.bottom = bars[i + 1].high;       // next candle's high
            fvg.midpoint = (fvg.top + fvg.bottom) / 2.0;
            fvg.direction = -1;
            fvg.mitigated = false;
            fvg.mitigated_index = 0;
            fvg.time = bars[i].time;
            fvg.visit_count = 0;
            fvgs_.push_back(fvg);
        }
    }
}

void FVGDetector::JoinConsecutive() {
    if (fvgs_.size() < 2) return;

    // Join consecutive FVGs of same direction
    // Keep last one with max(top) and min(bottom)
    for (size_t i = 0; i + 1 < fvgs_.size(); ) {
        if (fvgs_[i].direction == fvgs_[i + 1].direction &&
            fvgs_[i + 1].index == fvgs_[i].index + 1)
        {
            // Merge into i+1
            fvgs_[i + 1].top = std::max(fvgs_[i].top, fvgs_[i + 1].top);
            fvgs_[i + 1].bottom = std::min(fvgs_[i].bottom, fvgs_[i + 1].bottom);
            // Remove i
            fvgs_.erase(fvgs_.begin() + i);
        } else {
            ++i;
        }
    }
}

void FVGDetector::TrackMitigation(const std::vector<Bar>& bars) {
    int n = static_cast<int>(bars.size());

    for (auto& fvg : fvgs_) {
        if (fvg.mitigated) continue;

        // Recalculate midpoint after possible JoinConsecutive
        fvg.midpoint = (fvg.top + fvg.bottom) / 2.0;

        int start = fvg.index + 2;
        bool in_zone_prev = false;

        for (int j = start; j < n; ++j) {
            // Check if price enters the zone (for visit counting)
            bool in_zone = false;
            if (fvg.direction == 1) {
                in_zone = (bars[j].low <= fvg.top && bars[j].high >= fvg.bottom);
            } else {
                in_zone = (bars[j].high >= fvg.bottom && bars[j].low <= fvg.top);
            }

            // Count visit: price enters zone after being outside
            if (in_zone && !in_zone_prev) {
                fvg.visit_count++;
            }
            in_zone_prev = in_zone;

            // Mitigation: close-based (ICT: wick into FVG = entry, not invalidation)
            // Bullish FVG mitigated when candle CLOSES below bottom
            // Bearish FVG mitigated when candle CLOSES above top
            bool mitigated = false;
            if (fvg.direction == 1) {
                mitigated = bars[j].close < fvg.bottom;
            } else {
                mitigated = bars[j].close > fvg.top;
            }

            if (mitigated) {
                fvg.mitigated = true;
                fvg.mitigated_index = j;
                break;
            }
        }
    }
}

// --- Getters ---

int FVGDetector::GetTotalCount() const {
    return static_cast<int>(fvgs_.size());
}

const FairValueGap& FVGDetector::Get(int idx) const {
    return fvgs_[idx];
}

const std::vector<FairValueGap>& FVGDetector::GetAll() const {
    return fvgs_;
}

int FVGDetector::GetActiveCount(int direction) const {
    int count = 0;
    for (const auto& fvg : fvgs_) {
        if (fvg.mitigated) continue;
        if (direction == 0 || fvg.direction == direction) ++count;
    }
    return count;
}

bool FVGDetector::IsPriceInFVG(double price, int direction) const {
    return FindFVGAtPrice(price, direction) != nullptr;
}

std::vector<const FairValueGap*> FVGDetector::GetActiveFVGs(int direction) const {
    std::vector<const FairValueGap*> result;
    for (const auto& fvg : fvgs_) {
        if (fvg.mitigated) continue;
        if (direction == 0 || fvg.direction == direction) {
            result.push_back(&fvg);
        }
    }
    return result;
}

const FairValueGap* FVGDetector::FindFVGAtPrice(double price, int direction) const {
    for (int i = static_cast<int>(fvgs_.size()) - 1; i >= 0; --i) {
        const auto& fvg = fvgs_[i];
        if (fvg.mitigated) continue;
        if (direction != 0 && fvg.direction != direction) continue;
        if (price >= fvg.bottom && price <= fvg.top) {
            return &fvg;
        }
    }
    return nullptr;
}
