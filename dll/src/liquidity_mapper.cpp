#include "liquidity_mapper.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// LiquidityMapper implementation
// Port of smc.py liquidity() — lines 572-698
// ============================================================================

LiquidityMapper::LiquidityMapper()
    : range_percent_(0.01)
{}

void LiquidityMapper::Init(double range_percent) {
    range_percent_ = range_percent;
    Reset();
}

void LiquidityMapper::Reset() {
    pools_.clear();
}

void LiquidityMapper::Calculate(const std::vector<Bar>& bars, const SwingDetector& swings) {
    Reset();

    if (bars.empty()) return;

    // Calculate pip range based on overall high-low range
    double max_high = bars[0].high;
    double min_low  = bars[0].low;
    for (const auto& bar : bars) {
        if (bar.high > max_high) max_high = bar.high;
        if (bar.low < min_low)   min_low = bar.low;
    }
    double pip_range = (max_high - min_low) * range_percent_;

    int n = static_cast<int>(bars.size());
    const auto& all_swings = swings.GetAll();

    // Separate swing highs and lows
    std::vector<int> bull_indices; // swing high indices (buy-side liquidity)
    std::vector<int> bear_indices; // swing low indices (sell-side liquidity)

    for (size_t i = 0; i < all_swings.size(); ++i) {
        if (all_swings[i].type == SWING_HIGH) bull_indices.push_back(static_cast<int>(i));
        if (all_swings[i].type == SWING_LOW)  bear_indices.push_back(static_cast<int>(i));
    }

    // Track which swing indices have been used
    std::vector<bool> used(all_swings.size(), false);

    // --- Buy-side liquidity (equal highs) ---
    for (size_t bi = 0; bi < bull_indices.size(); ++bi) {
        int si = bull_indices[bi]; // swing index in all_swings
        if (used[si]) continue;

        const SwingPoint& sp = all_swings[si];
        double level = sp.level;
        double range_low  = level - pip_range;
        double range_high = level + pip_range;

        std::vector<double> group_levels;
        group_levels.push_back(level);
        int group_start = sp.index;
        int group_end   = sp.index;

        // Find swept index: first bar after this swing where high >= range_high
        int swept = 0;
        int bar_start = sp.index + 1;
        if (bar_start < n) {
            for (int j = bar_start; j < n; ++j) {
                if (bars[j].high >= range_high) {
                    swept = j;
                    break;
                }
            }
        }

        // Group nearby swing highs
        for (size_t bj = bi + 1; bj < bull_indices.size(); ++bj) {
            int sj = bull_indices[bj];
            if (used[sj]) continue;

            const SwingPoint& sp2 = all_swings[sj];

            // Stop if past swept
            if (swept > 0 && sp2.index >= swept) break;

            if (sp2.level >= range_low && sp2.level <= range_high) {
                group_levels.push_back(sp2.level);
                group_end = sp2.index;
                used[sj] = true;
            }
        }

        // Only create pool if 2+ swings grouped
        if (group_levels.size() >= 2) {
            double avg = 0.0;
            for (double lv : group_levels) avg += lv;
            avg /= group_levels.size();

            LiquidityPool pool{};
            pool.start_index = group_start;
            pool.end_index = group_end;
            pool.level = avg;
            pool.direction = 1; // buy-side (highs)
            pool.count = static_cast<int>(group_levels.size());
            pool.swept = (swept > 0);
            pool.swept_index = swept;

            pools_.push_back(pool);
        }
    }

    // Reset used flags for bearish
    std::fill(used.begin(), used.end(), false);

    // --- Sell-side liquidity (equal lows) ---
    for (size_t bi = 0; bi < bear_indices.size(); ++bi) {
        int si = bear_indices[bi];
        if (used[si]) continue;

        const SwingPoint& sp = all_swings[si];
        double level = sp.level;
        double range_low  = level - pip_range;
        double range_high = level + pip_range;

        std::vector<double> group_levels;
        group_levels.push_back(level);
        int group_start = sp.index;
        int group_end   = sp.index;

        // Find swept index: first bar after this swing where low <= range_low
        int swept = 0;
        int bar_start = sp.index + 1;
        if (bar_start < n) {
            for (int j = bar_start; j < n; ++j) {
                if (bars[j].low <= range_low) {
                    swept = j;
                    break;
                }
            }
        }

        // Group nearby swing lows
        for (size_t bj = bi + 1; bj < bear_indices.size(); ++bj) {
            int sj = bear_indices[bj];
            if (used[sj]) continue;

            const SwingPoint& sp2 = all_swings[sj];

            if (swept > 0 && sp2.index >= swept) break;

            if (sp2.level >= range_low && sp2.level <= range_high) {
                group_levels.push_back(sp2.level);
                group_end = sp2.index;
                used[sj] = true;
            }
        }

        if (group_levels.size() >= 2) {
            double avg = 0.0;
            for (double lv : group_levels) avg += lv;
            avg /= group_levels.size();

            LiquidityPool pool{};
            pool.start_index = group_start;
            pool.end_index = group_end;
            pool.level = avg;
            pool.direction = -1; // sell-side (lows)
            pool.count = static_cast<int>(group_levels.size());
            pool.swept = (swept > 0);
            pool.swept_index = swept;

            pools_.push_back(pool);
        }
    }

    // Sort by start index
    std::sort(pools_.begin(), pools_.end(),
        [](const LiquidityPool& a, const LiquidityPool& b) {
            return a.start_index < b.start_index;
        });
}

// --- Getters ---

int LiquidityMapper::GetTotalCount() const {
    return static_cast<int>(pools_.size());
}

const LiquidityPool& LiquidityMapper::Get(int idx) const {
    return pools_[idx];
}

const std::vector<LiquidityPool>& LiquidityMapper::GetAll() const {
    return pools_;
}

int LiquidityMapper::GetActiveCount(int direction) const {
    int count = 0;
    for (const auto& p : pools_) {
        if (p.swept) continue;
        if (direction == 0 || p.direction == direction) ++count;
    }
    return count;
}

std::vector<const LiquidityPool*> LiquidityMapper::GetActivePools(int direction) const {
    std::vector<const LiquidityPool*> result;
    for (const auto& p : pools_) {
        if (p.swept) continue;
        if (direction == 0 || p.direction == direction) {
            result.push_back(&p);
        }
    }
    return result;
}

bool LiquidityMapper::WasRecentlySwepted(int direction, int within_bars, int current_bar) const {
    for (int i = static_cast<int>(pools_.size()) - 1; i >= 0; --i) {
        const auto& p = pools_[i];
        if (!p.swept) continue;
        if (direction != 0 && p.direction != direction) continue;
        if (current_bar >= 0 && (current_bar - p.swept_index) > within_bars) continue;
        return true;
    }
    return false;
}

const LiquidityPool* LiquidityMapper::FindNearest(double price, int direction) const {
    const LiquidityPool* nearest = nullptr;
    double min_dist = 1e18;

    for (const auto& p : pools_) {
        if (p.swept) continue;
        if (direction != 0 && p.direction != direction) continue;
        double dist = std::fabs(price - p.level);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = &p;
        }
    }
    return nearest;
}
