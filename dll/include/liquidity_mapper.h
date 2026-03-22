#pragma once

#include "types.h"
#include "swing_detector.h"
#include <vector>

// ============================================================================
// LiquidityMapper — Identifies liquidity pools (equal highs/lows) and sweeps
// Port of smc.py liquidity()
//
// Algorithm:
//   Groups swing points that are within range_percent of each other.
//   Buy-side liquidity: 2+ swing highs at similar level.
//   Sell-side liquidity: 2+ swing lows at similar level.
//   Swept: price exceeds the pool's range boundary.
// ============================================================================

class LiquidityMapper {
public:
    LiquidityMapper();

    void Init(double range_percent = 0.01);
    void Reset();

    // Full calculation
    void Calculate(const std::vector<Bar>& bars, const SwingDetector& swings);

    // Results
    int                               GetTotalCount() const;
    const LiquidityPool&              Get(int idx) const;
    const std::vector<LiquidityPool>& GetAll() const;

    // Active (unswept) pools
    int GetActiveCount(int direction = 0) const;

    // Get active pools by direction
    std::vector<const LiquidityPool*> GetActivePools(int direction) const;

    // Check if any pool was recently swept
    bool WasRecentlySwepted(int direction, int within_bars = 10, int current_bar = -1) const;

    // Find nearest pool
    const LiquidityPool* FindNearest(double price, int direction) const;

private:
    double range_percent_;
    std::vector<LiquidityPool> pools_;
};
