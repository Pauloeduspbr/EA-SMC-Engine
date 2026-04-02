#include "ob_tracker.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// OrderBlockTracker implementation
// Port of smc.py ob() — lines 376-570
// ============================================================================

OrderBlockTracker::OrderBlockTracker()
    : close_mitigation_(false)
{}

void OrderBlockTracker::Init(bool close_mitigation) {
    close_mitigation_ = close_mitigation;
    Reset();
}

void OrderBlockTracker::Reset() {
    obs_.clear();
}

void OrderBlockTracker::Calculate(const std::vector<Bar>& bars, const SwingDetector& swings) {
    Reset();
    if (bars.empty()) return;

    CalculateBullish(bars, swings);
    CalculateBearish(bars, swings);

    // Sort by index
    std::sort(obs_.begin(), obs_.end(),
        [](const OrderBlock& a, const OrderBlock& b) { return a.index < b.index; });
}

void OrderBlockTracker::CalculateBullish(const std::vector<Bar>& bars, const SwingDetector& swings) {
    int n = static_cast<int>(bars.size());

    // Collect swing high indices (sorted)
    std::vector<int> sh_indices;
    for (const auto& sp : swings.GetAll()) {
        if (sp.type == SWING_HIGH) sh_indices.push_back(sp.index);
    }

    if (sh_indices.empty()) return;

    // Track which swing highs have been crossed
    std::vector<bool> crossed(n, false);

    // Active bullish OBs for mitigation tracking
    std::vector<size_t> active;

    for (int i = 0; i < n; ++i) {
        // Update mitigation for active bullish OBs
        for (size_t k = 0; k < active.size(); ) {
            size_t idx = active[k];
            OrderBlock& ob = obs_[idx];

            if (ob.mitigated) {
                // Breaker check: if price high > OB top after mitigation → remove
                if (bars[i].high > ob.top) {
                    ob.direction = 0; // mark for removal
                    active.erase(active.begin() + k);
                    continue;
                }
            } else {
                // Visit tracking: price enters OB zone
                bool in_zone = (bars[i].low <= ob.top && bars[i].high >= ob.bottom);
                if (in_zone) {
                    if (i > 0) {
                        bool was_outside = (bars[i-1].low > ob.top || bars[i-1].high < ob.bottom);
                        if (was_outside) ob.visit_count++;
                    }
                }

                // Mitigation: body close through OB invalidates it
                // Wick-through alone does NOT mitigate on first visit (ICT: price
                // tests OB zone and bounces — the wick IS the entry confirmation)
                bool mit = false;
                double body_low = std::min(bars[i].open, bars[i].close);
                if (close_mitigation_ || ob.visit_count >= 1) {
                    // After first visit OR if close_mitigation enabled: body must break
                    mit = body_low < ob.bottom;
                } else {
                    // Before first visit: only full candle close below = mitigation
                    mit = bars[i].close < ob.bottom;
                }
                if (mit) {
                    ob.mitigated = true;
                    ob.mitigated_index = i;
                }
            }
            ++k;
        }

        // Find last swing high before current bar (binary search)
        auto it = std::lower_bound(sh_indices.begin(), sh_indices.end(), i);
        if (it == sh_indices.begin()) continue;
        --it;
        int last_sh_idx = *it;

        // Check if close breaks above the swing high
        if (bars[i].close > bars[last_sh_idx].high && !crossed[last_sh_idx]) {
            crossed[last_sh_idx] = true;

            // Find the candle with lowest low between swing high and current bar
            int ob_idx = i - 1;
            double ob_btm = bars[i - 1].low;   // OB bottom = candle low
            double ob_top = bars[i - 1].high;   // OB top = candle high

            if (i - last_sh_idx > 1) {
                int start = last_sh_idx + 1;
                int end = i; // exclusive
                if (end > start) {
                    double min_low = bars[start].low;
                    int min_idx = start;
                    for (int j = start + 1; j < end; ++j) {
                        if (bars[j].low <= min_low) { // <= to take last occurrence
                            min_low = bars[j].low;
                            min_idx = j;
                        }
                    }
                    ob_btm = bars[min_idx].low;
                    ob_top = bars[min_idx].high;
                    ob_idx = min_idx;
                }
            }

            // Calculate volume
            double vol_cur  = static_cast<double>(bars[i].volume);
            double vol_prev1 = (i >= 1) ? static_cast<double>(bars[i - 1].volume) : 0.0;
            double vol_prev2 = (i >= 2) ? static_cast<double>(bars[i - 2].volume) : 0.0;
            double ob_vol = vol_cur + vol_prev1 + vol_prev2;
            double low_vol = vol_prev2;
            double high_vol = vol_cur + vol_prev1;
            double max_vol = std::max(high_vol, low_vol);
            double pct = (max_vol != 0.0) ? (std::min(high_vol, low_vol) / max_vol * 100.0) : 100.0;

            OrderBlock ob{};
            ob.index = ob_idx;
            ob.top = ob_top;
            ob.bottom = ob_btm;
            ob.direction = 1;
            ob.ob_volume = ob_vol;
            ob.percentage = pct;
            ob.mitigated = false;
            ob.mitigated_index = 0;
            ob.time = bars[ob_idx].time;
            ob.visit_count = 0;
            // High prob: OB candle is large-bodied (body > 60% of range)
            double body = std::fabs(bars[ob_idx].close - bars[ob_idx].open);
            double range = bars[ob_idx].high - bars[ob_idx].low;
            ob.high_prob = (range > 0 && body / range > 0.6);

            active.push_back(obs_.size());
            obs_.push_back(ob);
        }
    }
}

void OrderBlockTracker::CalculateBearish(const std::vector<Bar>& bars, const SwingDetector& swings) {
    int n = static_cast<int>(bars.size());

    // Collect swing low indices (sorted)
    std::vector<int> sl_indices;
    for (const auto& sp : swings.GetAll()) {
        if (sp.type == SWING_LOW) sl_indices.push_back(sp.index);
    }

    if (sl_indices.empty()) return;

    std::vector<bool> crossed(n, false);
    std::vector<size_t> active;

    for (int i = 0; i < n; ++i) {
        // Update mitigation for active bearish OBs
        for (size_t k = 0; k < active.size(); ) {
            size_t idx = active[k];
            OrderBlock& ob = obs_[idx];

            if (ob.mitigated) {
                // Breaker: if price low < OB bottom after mitigation → remove
                if (bars[i].low < ob.bottom) {
                    ob.direction = 0;
                    active.erase(active.begin() + k);
                    continue;
                }
            } else {
                // Visit tracking
                bool in_zone = (bars[i].low <= ob.top && bars[i].high >= ob.bottom);
                if (in_zone && i > 0) {
                    bool was_outside = (bars[i-1].low > ob.top || bars[i-1].high < ob.bottom);
                    if (was_outside) ob.visit_count++;
                }

                // Mitigation: body close through OB invalidates it
                // Same ICT logic as bullish: wick-through on first visit is the entry
                bool mit = false;
                double body_high = std::max(bars[i].open, bars[i].close);
                if (close_mitigation_ || ob.visit_count >= 1) {
                    mit = body_high > ob.top;
                } else {
                    mit = bars[i].close > ob.top;
                }
                if (mit) {
                    ob.mitigated = true;
                    ob.mitigated_index = i;
                }
            }
            ++k;
        }

        // Find last swing low before current bar
        auto it = std::lower_bound(sl_indices.begin(), sl_indices.end(), i);
        if (it == sl_indices.begin()) continue;
        --it;
        int last_sl_idx = *it;

        // Check if close breaks below the swing low
        if (bars[i].close < bars[last_sl_idx].low && !crossed[last_sl_idx]) {
            crossed[last_sl_idx] = true;

            // Find the candle with highest high between swing low and current bar
            int ob_idx = i - 1;
            double ob_top = bars[i - 1].high;
            double ob_btm = bars[i - 1].low;

            if (i - last_sl_idx > 1) {
                int start = last_sl_idx + 1;
                int end = i;
                if (end > start) {
                    double max_high = bars[start].high;
                    int max_idx = start;
                    for (int j = start + 1; j < end; ++j) {
                        if (bars[j].high >= max_high) {
                            max_high = bars[j].high;
                            max_idx = j;
                        }
                    }
                    ob_top = bars[max_idx].high;
                    ob_btm = bars[max_idx].low;
                    ob_idx = max_idx;
                }
            }

            double vol_cur  = static_cast<double>(bars[i].volume);
            double vol_prev1 = (i >= 1) ? static_cast<double>(bars[i - 1].volume) : 0.0;
            double vol_prev2 = (i >= 2) ? static_cast<double>(bars[i - 2].volume) : 0.0;
            double ob_vol = vol_cur + vol_prev1 + vol_prev2;
            double low_vol = vol_cur + vol_prev1;
            double high_vol = vol_prev2;
            double max_vol = std::max(high_vol, low_vol);
            double pct = (max_vol != 0.0) ? (std::min(high_vol, low_vol) / max_vol * 100.0) : 100.0;

            OrderBlock ob{};
            ob.index = ob_idx;
            ob.top = ob_top;
            ob.bottom = ob_btm;
            ob.direction = -1;
            ob.ob_volume = ob_vol;
            ob.percentage = pct;
            ob.mitigated = false;
            ob.mitigated_index = 0;
            ob.time = bars[ob_idx].time;
            ob.visit_count = 0;
            double body = std::fabs(bars[ob_idx].close - bars[ob_idx].open);
            double range = bars[ob_idx].high - bars[ob_idx].low;
            ob.high_prob = (range > 0 && body / range > 0.6);

            active.push_back(obs_.size());
            obs_.push_back(ob);
        }
    }
}

// --- Getters ---

int OrderBlockTracker::GetTotalCount() const {
    return static_cast<int>(obs_.size());
}

const OrderBlock& OrderBlockTracker::Get(int idx) const {
    return obs_[idx];
}

const std::vector<OrderBlock>& OrderBlockTracker::GetAll() const {
    return obs_;
}

int OrderBlockTracker::GetActiveCount(int direction) const {
    int count = 0;
    for (const auto& ob : obs_) {
        if (ob.direction == 0) continue; // removed
        if (ob.mitigated) continue;
        if (direction == 0 || ob.direction == direction) ++count;
    }
    return count;
}

bool OrderBlockTracker::IsPriceInOB(double price, int direction) const {
    return FindOBAtPrice(price, direction) != nullptr;
}

std::vector<const OrderBlock*> OrderBlockTracker::GetActiveOBs(int direction) const {
    std::vector<const OrderBlock*> result;
    for (const auto& ob : obs_) {
        if (ob.direction == 0) continue;
        if (ob.mitigated) continue;
        if (direction == 0 || ob.direction == direction) {
            result.push_back(&ob);
        }
    }
    return result;
}

const OrderBlock* OrderBlockTracker::FindOBAtPrice(double price, int direction) const {
    // Search from most recent to oldest
    for (int i = static_cast<int>(obs_.size()) - 1; i >= 0; --i) {
        const auto& ob = obs_[i];
        if (ob.direction == 0 || ob.mitigated) continue;
        if (direction != 0 && ob.direction != direction) continue;
        if (price >= ob.bottom && price <= ob.top) {
            return &ob;
        }
    }
    return nullptr;
}
