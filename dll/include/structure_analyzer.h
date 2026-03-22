#pragma once

#include "types.h"
#include "swing_detector.h"
#include <vector>

// ============================================================================
// StructureAnalyzer — Detects BOS (Break of Structure) and CHoCH
// Port of smc.py bos_choch()
//
// Algorithm:
//   Tracks last 4 swing points in order.
//   BOS bullish:  [-1,1,-1,1] where L0 < L2 < H1 < H3 (HH + HL)
//   BOS bearish:  [1,-1,1,-1] where H0 > H2 > L1 > L3 (LH + LL)
//   CHoCH bullish: [-1,1,-1,1] where H3 > H1 > H0 > H2
//   CHoCH bearish: [1,-1,1,-1] where L3 < L1 < L0 < L2
//   Then finds the bar that actually broke the level.
// ============================================================================

class StructureAnalyzer {
public:
    StructureAnalyzer();

    void Init(bool close_break = true);
    void Reset();

    // Full calculation from swings + bars
    void Calculate(const std::vector<Bar>& bars, const SwingDetector& swings);

    // Results
    int                              GetBreakCount() const;
    const StructureBreak&            Get(int idx) const;
    const std::vector<StructureBreak>& GetAll() const;

    // Current trend (derived from latest BOS/CHoCH)
    TrendDirection GetCurrentTrend() const;

    // Last BOS / CHoCH
    StructureBreak GetLastBOS() const;
    StructureBreak GetLastCHoCH() const;

    // Per-bar queries
    bool   HasBOSAt(int bar_index) const;
    bool   HasCHoCHAt(int bar_index) const;

private:
    bool close_break_;
    std::vector<StructureBreak> breaks_;
    TrendDirection current_trend_;

    // Per-bar arrays
    std::vector<int> bar_bos_;    // 0, 1, -1
    std::vector<int> bar_choch_;  // 0, 1, -1
    std::vector<double> bar_level_;

    void FindBrokenIndices(const std::vector<Bar>& bars);
    void RemoveUnbroken();
    void DetermineTrend();
};
