#pragma once

#include "types.h"
#include <vector>

// ============================================================================
// FVGDetector — Detects Fair Value Gaps and tracks mitigation
// Port of smc.py fvg()
//
// Algorithm:
//   Bullish FVG: bar[i-1].high < bar[i+1].low AND bar[i] is bullish
//     Top = bar[i+1].low, Bottom = bar[i-1].high
//   Bearish FVG: bar[i-1].low > bar[i+1].high AND bar[i] is bearish
//     Top = bar[i-1].low, Bottom = bar[i+1].high
//   Mitigation: bullish FVG mitigated when price low <= top
//     bearish FVG mitigated when price high >= bottom
//   Optional: join consecutive FVGs of same type
// ============================================================================

class FVGDetector {
public:
    FVGDetector();

    void Init(bool join_consecutive = false);
    void Reset();

    // Full calculation
    void Calculate(const std::vector<Bar>& bars);

    // Results
    int                               GetTotalCount() const;
    const FairValueGap&               Get(int idx) const;
    const std::vector<FairValueGap>&  GetAll() const;

    // Active (unmitigated) FVGs
    int  GetActiveCount(int direction = 0) const;  // 0=all, 1=bullish, -1=bearish
    bool IsPriceInFVG(double price, int direction) const;

    // Get active FVGs by direction
    std::vector<const FairValueGap*> GetActiveFVGs(int direction) const;

    // Find FVG at price
    const FairValueGap* FindFVGAtPrice(double price, int direction) const;

private:
    bool join_consecutive_;
    std::vector<FairValueGap> fvgs_;

    void DetectFVGs(const std::vector<Bar>& bars);
    void JoinConsecutive();
    void TrackMitigation(const std::vector<Bar>& bars);
};
