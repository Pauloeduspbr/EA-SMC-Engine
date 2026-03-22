#include "structure_analyzer.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// StructureAnalyzer implementation
// Port of smc.py bos_choch() — lines 222-373
// ============================================================================

StructureAnalyzer::StructureAnalyzer()
    : close_break_(true), current_trend_(TREND_NONE)
{}

void StructureAnalyzer::Init(bool close_break) {
    close_break_ = close_break;
    Reset();
}

void StructureAnalyzer::Reset() {
    breaks_.clear();
    current_trend_ = TREND_NONE;
    bar_bos_.clear();
    bar_choch_.clear();
    bar_level_.clear();
}

void StructureAnalyzer::Calculate(const std::vector<Bar>& bars, const SwingDetector& swings) {
    Reset();

    int n = static_cast<int>(bars.size());
    if (n == 0) return;

    bar_bos_.resize(n, 0);
    bar_choch_.resize(n, 0);
    bar_level_.resize(n, 0.0);

    const auto& swing_list = swings.GetAll();
    if (swing_list.size() < 4) return;

    // Track order of levels and types as we see them
    std::vector<double> level_order;
    std::vector<int>    hl_order;    // 1 = high, -1 = low
    std::vector<int>    positions;   // bar indices of swings

    for (const auto& sp : swing_list) {
        level_order.push_back(sp.level);
        hl_order.push_back(static_cast<int>(sp.type));
        positions.push_back(sp.index);

        size_t sz = level_order.size();
        if (sz < 4) continue;

        // Get last 4 entries
        int hl0 = hl_order[sz - 4];
        int hl1 = hl_order[sz - 3];
        int hl2 = hl_order[sz - 2];
        int hl3 = hl_order[sz - 1];

        double lv0 = level_order[sz - 4];
        double lv1 = level_order[sz - 3];
        double lv2 = level_order[sz - 2];
        double lv3 = level_order[sz - 1];

        int detect_bar = positions[sz - 2]; // Python places at last_positions[-2]

        // --- Bullish BOS ---
        // Pattern: [-1, 1, -1, 1] where lv0 < lv2 < lv1 < lv3
        // (higher lows + higher highs = uptrend continuation)
        if (hl0 == -1 && hl1 == 1 && hl2 == -1 && hl3 == 1) {
            if (lv0 < lv2 && lv2 < lv1 && lv1 < lv3) {
                bar_bos_[detect_bar] = 1;
                bar_level_[detect_bar] = lv1; // level of the previous high
            }
        }

        // --- Bearish BOS ---
        // Pattern: [1, -1, 1, -1] where lv0 > lv2 > lv1 > lv3
        // (lower highs + lower lows = downtrend continuation)
        if (hl0 == 1 && hl1 == -1 && hl2 == 1 && hl3 == -1) {
            if (lv0 > lv2 && lv2 > lv1 && lv1 > lv3) {
                bar_bos_[detect_bar] = -1;
                bar_level_[detect_bar] = lv1; // level of the previous low
            }
        }

        // --- Bullish CHoCH ---
        // Pattern: [-1, 1, -1, 1] where lv3 > lv1 > lv0 > lv2
        // (was making lower lows, now broke above previous high)
        if (hl0 == -1 && hl1 == 1 && hl2 == -1 && hl3 == 1) {
            if (lv3 > lv1 && lv1 > lv0 && lv0 > lv2) {
                bar_choch_[detect_bar] = 1;
                bar_level_[detect_bar] = lv1;
            }
        }

        // --- Bearish CHoCH ---
        // Pattern: [1, -1, 1, -1] where lv3 < lv1 < lv0 < lv2
        // (was making higher highs, now broke below previous low)
        if (hl0 == 1 && hl1 == -1 && hl2 == 1 && hl3 == -1) {
            if (lv3 < lv1 && lv1 < lv0 && lv0 < lv2) {
                bar_choch_[detect_bar] = -1;
                bar_level_[detect_bar] = lv1;
            }
        }
    }

    // Find bars that broke the level and remove unbroken
    FindBrokenIndices(bars);
    RemoveUnbroken();

    // Build breaks_ list and determine trend
    for (int i = 0; i < n; ++i) {
        if (bar_bos_[i] != 0 || bar_choch_[i] != 0) {
            StructureBreak sb{};
            sb.index = i;
            sb.time = bars[i].time;
            sb.level = bar_level_[i];

            if (bar_bos_[i] != 0) {
                sb.type = STRUCT_BOS;
                sb.direction = (bar_bos_[i] == 1) ? TREND_BULLISH : TREND_BEARISH;
            } else {
                sb.type = STRUCT_CHOCH;
                sb.direction = (bar_choch_[i] == 1) ? TREND_BULLISH : TREND_BEARISH;
            }

            // Find broken index (first bar that exceeded the level)
            sb.broken_index = -1;
            for (int j = i + 2; j < n; ++j) {
                bool broken = false;
                if (sb.direction == TREND_BULLISH) {
                    broken = close_break_ ? (bars[j].close > sb.level)
                                          : (bars[j].high > sb.level);
                } else {
                    broken = close_break_ ? (bars[j].close < sb.level)
                                          : (bars[j].low < sb.level);
                }
                if (broken) {
                    sb.broken_index = j;
                    break;
                }
            }

            breaks_.push_back(sb);
        }
    }

    DetermineTrend();
}

void StructureAnalyzer::FindBrokenIndices(const std::vector<Bar>& bars) {
    int n = static_cast<int>(bars.size());

    // For each BOS/CHoCH, find the bar that broke the level
    // Also remove overlapping breaks (Python logic: if k < i and broken[k] >= j, remove k)
    std::vector<int> broken(n, 0);

    for (int i = 0; i < n; ++i) {
        if (bar_bos_[i] == 0 && bar_choch_[i] == 0) continue;

        double level = bar_level_[i];
        int dir = (bar_bos_[i] != 0) ? bar_bos_[i] : bar_choch_[i];

        for (int j = i + 2; j < n; ++j) {
            bool b = false;
            if (dir > 0) {
                b = close_break_ ? (bars[j].close > level) : (bars[j].high > level);
            } else {
                b = close_break_ ? (bars[j].close < level) : (bars[j].low < level);
            }
            if (b) {
                broken[i] = j;

                // Remove earlier breaks that ended after this one
                for (int k = 0; k < i; ++k) {
                    if ((bar_bos_[k] != 0 || bar_choch_[k] != 0) && broken[k] >= j) {
                        bar_bos_[k] = 0;
                        bar_choch_[k] = 0;
                        bar_level_[k] = 0.0;
                    }
                }
                break;
            }
        }
    }

    // Store broken indices for RemoveUnbroken
    for (int i = 0; i < n; ++i) {
        if ((bar_bos_[i] != 0 || bar_choch_[i] != 0) && broken[i] == 0) {
            bar_bos_[i] = 0;
            bar_choch_[i] = 0;
            bar_level_[i] = 0.0;
        }
    }
}

void StructureAnalyzer::RemoveUnbroken() {
    // Already handled in FindBrokenIndices
}

void StructureAnalyzer::DetermineTrend() {
    current_trend_ = TREND_NONE;
    if (!breaks_.empty()) {
        current_trend_ = breaks_.back().direction;
    }
}

// --- Getters ---

int StructureAnalyzer::GetBreakCount() const {
    return static_cast<int>(breaks_.size());
}

const StructureBreak& StructureAnalyzer::Get(int idx) const {
    return breaks_[idx];
}

const std::vector<StructureBreak>& StructureAnalyzer::GetAll() const {
    return breaks_;
}

TrendDirection StructureAnalyzer::GetCurrentTrend() const {
    return current_trend_;
}

StructureBreak StructureAnalyzer::GetLastBOS() const {
    for (int i = static_cast<int>(breaks_.size()) - 1; i >= 0; --i) {
        if (breaks_[i].type == STRUCT_BOS) return breaks_[i];
    }
    return StructureBreak{-1, STRUCT_NONE, TREND_NONE, 0.0, -1, 0};
}

StructureBreak StructureAnalyzer::GetLastCHoCH() const {
    for (int i = static_cast<int>(breaks_.size()) - 1; i >= 0; --i) {
        if (breaks_[i].type == STRUCT_CHOCH) return breaks_[i];
    }
    return StructureBreak{-1, STRUCT_NONE, TREND_NONE, 0.0, -1, 0};
}

bool StructureAnalyzer::HasBOSAt(int bar_index) const {
    if (bar_index < 0 || bar_index >= static_cast<int>(bar_bos_.size())) return false;
    return bar_bos_[bar_index] != 0;
}

bool StructureAnalyzer::HasCHoCHAt(int bar_index) const {
    if (bar_index < 0 || bar_index >= static_cast<int>(bar_choch_.size())) return false;
    return bar_choch_[bar_index] != 0;
}
