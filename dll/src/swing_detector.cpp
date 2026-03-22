#include "swing_detector.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// SwingDetector implementation
// Port of smc.py swing_highs_lows() — line 137-219
// ============================================================================

SwingDetector::SwingDetector()
    : swing_length_(50)
{}

void SwingDetector::Init(int swing_length) {
    swing_length_ = swing_length;
    Reset();
}

void SwingDetector::Reset() {
    swings_.clear();
    bar_swing_type_.clear();
    bar_swing_level_.clear();
}

void SwingDetector::Calculate(const std::vector<Bar>& bars) {
    Reset();

    int n = static_cast<int>(bars.size());
    if (n < swing_length_ * 2 + 1) return;

    // The Python code does: swing_length *= 2, then uses rolling(swing_length)
    // with shift(-(swing_length//2)). This means we look swing_length_ bars
    // on each side of the current bar.
    int half = swing_length_;  // original swing_length (before *2 in Python)

    bar_swing_type_.resize(n, SWING_NONE);
    bar_swing_level_.resize(n, 0.0);

    // Step 1: Identify raw swing points
    // swing high: bar[i].high is the max of bars[i-half .. i+half]
    // swing low:  bar[i].low  is the min of bars[i-half .. i+half]
    for (int i = half; i < n - half; ++i) {
        double max_high = bars[i].high;
        double min_low  = bars[i].low;
        bool is_highest = true;
        bool is_lowest  = true;

        for (int j = i - half; j <= i + half; ++j) {
            if (j == i) continue;
            if (bars[j].high >= max_high) { // >= matches Python rolling.max behavior
                if (bars[j].high > max_high || j > i) {
                    is_highest = false;
                }
            }
            if (bars[j].low <= min_low) {
                if (bars[j].low < min_low || j > i) {
                    is_lowest = false;
                }
            }
        }

        // Swing high takes priority if both are true (matches Python np.where order)
        if (is_highest) {
            bar_swing_type_[i] = SWING_HIGH;
            bar_swing_level_[i] = bars[i].high;
        } else if (is_lowest) {
            bar_swing_type_[i] = SWING_LOW;
            bar_swing_level_[i] = bars[i].low;
        }
    }

    // Step 2: Deduplicate consecutive same-type swings
    Deduplicate(bars);

    // Step 3: Force alternation (ensure H-L-H-L pattern)
    ForceAlternation(bars);

    // Step 4: Build the swings_ list from bar arrays
    BuildSwingList(bars);
}

void SwingDetector::Deduplicate(const std::vector<Bar>& bars) {
    // Same logic as Python: keep iterating until no more consecutive duplicates
    bool changed = true;
    while (changed) {
        changed = false;

        // Collect positions of all swing points
        std::vector<int> positions;
        for (int i = 0; i < static_cast<int>(bar_swing_type_.size()); ++i) {
            if (bar_swing_type_[i] != SWING_NONE)
                positions.push_back(i);
        }

        if (positions.size() < 2) break;

        for (size_t k = 0; k + 1 < positions.size(); ++k) {
            int curr_pos = positions[k];
            int next_pos = positions[k + 1];
            SwingType curr_type = bar_swing_type_[curr_pos];
            SwingType next_type = bar_swing_type_[next_pos];

            if (curr_type == next_type) {
                if (curr_type == SWING_HIGH) {
                    // Keep the higher one
                    if (bars[curr_pos].high < bars[next_pos].high) {
                        bar_swing_type_[curr_pos] = SWING_NONE;
                        bar_swing_level_[curr_pos] = 0.0;
                    } else {
                        bar_swing_type_[next_pos] = SWING_NONE;
                        bar_swing_level_[next_pos] = 0.0;
                    }
                } else { // SWING_LOW
                    // Keep the lower one
                    if (bars[curr_pos].low > bars[next_pos].low) {
                        bar_swing_type_[curr_pos] = SWING_NONE;
                        bar_swing_level_[curr_pos] = 0.0;
                    } else {
                        bar_swing_type_[next_pos] = SWING_NONE;
                        bar_swing_level_[next_pos] = 0.0;
                    }
                }
                changed = true;
            }
        }
    }
}

void SwingDetector::ForceAlternation(const std::vector<Bar>& bars) {
    int n = static_cast<int>(bar_swing_type_.size());
    if (n == 0) return;

    // Collect swing positions
    std::vector<int> positions;
    for (int i = 0; i < n; ++i) {
        if (bar_swing_type_[i] != SWING_NONE)
            positions.push_back(i);
    }

    if (positions.empty()) return;

    // Python forces: if first swing is high, prepend a low at index 0
    //                if first swing is low, prepend a high at index 0
    SwingType first_type = bar_swing_type_[positions[0]];
    if (first_type == SWING_HIGH) {
        bar_swing_type_[0] = SWING_LOW;
        bar_swing_level_[0] = bars[0].low;
    } else if (first_type == SWING_LOW) {
        bar_swing_type_[0] = SWING_HIGH;
        bar_swing_level_[0] = bars[0].high;
    }

    // Python forces: if last swing is low, append a high at last index
    //                if last swing is high, append a low at last index
    // Recollect positions
    positions.clear();
    for (int i = 0; i < n; ++i) {
        if (bar_swing_type_[i] != SWING_NONE)
            positions.push_back(i);
    }

    if (!positions.empty()) {
        SwingType last_type = bar_swing_type_[positions.back()];
        int last_idx = n - 1;
        if (last_type == SWING_LOW) {
            bar_swing_type_[last_idx] = SWING_HIGH;
            bar_swing_level_[last_idx] = bars[last_idx].high;
        } else if (last_type == SWING_HIGH) {
            bar_swing_type_[last_idx] = SWING_LOW;
            bar_swing_level_[last_idx] = bars[last_idx].low;
        }
    }
}

void SwingDetector::BuildSwingList(const std::vector<Bar>& bars) {
    swings_.clear();
    for (int i = 0; i < static_cast<int>(bar_swing_type_.size()); ++i) {
        if (bar_swing_type_[i] != SWING_NONE) {
            SwingPoint sp{};
            sp.index = i;
            sp.level = bar_swing_level_[i];
            sp.type  = bar_swing_type_[i];
            sp.time  = (i < static_cast<int>(bars.size())) ? bars[i].time : 0;
            swings_.push_back(sp);
        }
    }
}

// --- Getters ---

int SwingDetector::GetCount() const {
    return static_cast<int>(swings_.size());
}

const SwingPoint& SwingDetector::Get(int idx) const {
    return swings_[idx];
}

const std::vector<SwingPoint>& SwingDetector::GetAll() const {
    return swings_;
}

SwingPoint SwingDetector::GetLastHigh() const {
    for (int i = static_cast<int>(swings_.size()) - 1; i >= 0; --i) {
        if (swings_[i].type == SWING_HIGH) return swings_[i];
    }
    return SwingPoint{-1, 0.0, SWING_NONE, 0};
}

SwingPoint SwingDetector::GetLastLow() const {
    for (int i = static_cast<int>(swings_.size()) - 1; i >= 0; --i) {
        if (swings_[i].type == SWING_LOW) return swings_[i];
    }
    return SwingPoint{-1, 0.0, SWING_NONE, 0};
}

SwingType SwingDetector::GetBarSwingType(int bar_index) const {
    if (bar_index < 0 || bar_index >= static_cast<int>(bar_swing_type_.size()))
        return SWING_NONE;
    return bar_swing_type_[bar_index];
}

double SwingDetector::GetBarSwingLevel(int bar_index) const {
    if (bar_index < 0 || bar_index >= static_cast<int>(bar_swing_level_.size()))
        return 0.0;
    return bar_swing_level_[bar_index];
}
