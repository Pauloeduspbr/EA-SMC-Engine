#pragma once

#include "types.h"
#include <vector>

// ============================================================================
// SwingDetector — Identifies Swing Highs and Swing Lows
// Port of smc.py swing_highs_lows()
//
// Algorithm:
//   1. A swing high = highest high in window of swing_length bars each side
//   2. A swing low  = lowest low in window of swing_length bars each side
//   3. Deduplicate: consecutive same-type swings keep the more extreme one
//   4. Force alternation: H-L-H-L pattern
// ============================================================================

class SwingDetector {
public:
    SwingDetector();

    void Init(int swing_length);
    void Reset();

    // Feed historical bars (full recalculation)
    void Calculate(const std::vector<Bar>& bars);

    // Results
    int                          GetCount() const;
    const SwingPoint&            Get(int idx) const;
    const std::vector<SwingPoint>& GetAll() const;

    // Convenience
    SwingPoint GetLastHigh() const;
    SwingPoint GetLastLow() const;

    // Per-bar swing type (NaN-like: SWING_NONE for non-swing bars)
    SwingType GetBarSwingType(int bar_index) const;
    double    GetBarSwingLevel(int bar_index) const;

private:
    int swing_length_;
    std::vector<SwingPoint> swings_;

    // Per-bar arrays (same size as bars)
    std::vector<SwingType> bar_swing_type_;
    std::vector<double>    bar_swing_level_;

    void Deduplicate(const std::vector<Bar>& bars);
    void ForceAlternation(const std::vector<Bar>& bars);
    void BuildSwingList(const std::vector<Bar>& bars);
};
