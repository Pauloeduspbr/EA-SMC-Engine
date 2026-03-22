#pragma once

#include "types.h"
#include "swing_detector.h"
#include <vector>

// ============================================================================
// OrderBlockTracker — Detects and tracks Order Blocks with mitigation
// Port of smc.py ob()
//
// Algorithm:
//   Bullish OB: when close breaks above last swing high,
//     find the candle with lowest low between swing high and break bar.
//     That candle's [low, high] defines the OB zone.
//   Bearish OB: when close breaks below last swing low,
//     find the candle with highest high between swing low and break bar.
//   Mitigation: bullish OB mitigated when price low < OB bottom.
//     If mitigated and then price high > OB top → breaker → remove OB.
// ============================================================================

class OrderBlockTracker {
public:
    OrderBlockTracker();

    void Init(bool close_mitigation = false);
    void Reset();

    // Full calculation
    void Calculate(const std::vector<Bar>& bars, const SwingDetector& swings);

    // Results — all OBs (including mitigated)
    int                            GetTotalCount() const;
    const OrderBlock&              Get(int idx) const;
    const std::vector<OrderBlock>& GetAll() const;

    // Active (unmitigated) OBs
    int  GetActiveCount(int direction = 0) const;  // 0=all, 1=bullish, -1=bearish
    bool IsPriceInOB(double price, int direction) const;

    // Get specific active OB by direction, ordered by recency
    std::vector<const OrderBlock*> GetActiveOBs(int direction) const;

    // Check if a price is inside any active OB zone
    const OrderBlock* FindOBAtPrice(double price, int direction) const;

private:
    bool close_mitigation_;
    std::vector<OrderBlock> obs_;

    void CalculateBullish(const std::vector<Bar>& bars, const SwingDetector& swings);
    void CalculateBearish(const std::vector<Bar>& bars, const SwingDetector& swings);
};
