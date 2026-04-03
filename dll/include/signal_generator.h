#pragma once

#include "types.h"
#include "swing_detector.h"
#include "structure_analyzer.h"
#include "ob_tracker.h"
#include "fvg_detector.h"
#include "liquidity_mapper.h"

#include <vector>

// ============================================================================
// Per-Strategy Parameters — each strategy has its own tunable settings
// ============================================================================

struct StrategyParams {
    bool   enabled;
    int    lookback;          // how recent the structure event must be (bars)
    double min_score;         // minimum confluence score for THIS strategy
    int    max_zone_visits;   // max visits before zone is stale (0=disabled)
    bool   require_ote;       // require price in OTE zone
    bool   require_high_prob; // require high-probability OB (large body)
    bool   require_kill_zone; // require entry during kill zone
    double min_sl_points;     // min SL distance (price units, 0=use global)
    double max_sl_points;     // max SL distance (price units, 0=use global)

    StrategyParams()
        : enabled(true), lookback(50), min_score(0.30), max_zone_visits(0)
        , require_ote(false), require_high_prob(false), require_kill_zone(false)
        , min_sl_points(0.0), max_sl_points(0.0)
    {}
};

// ============================================================================
// Global config (shared across strategies)
// ============================================================================

struct SignalConfig {
    // Per-strategy params (5 strategies)
    StrategyParams ob_bounce;
    StrategyParams bos_continuation;
    StrategyParams choch_reversal;
    StrategyParams fvg_fill;
    StrategyParams liq_sweep;

    // Global SL buffer
    double sl_zone_buffer;

    // Global sweep lookback
    int sweep_lookback;

    // OTE Fibonacci levels
    double ote_fib_low;
    double ote_fib_high;

    // Kill Zone time windows (hours in broker time)
    int kz_asian_start_hour, kz_asian_end_hour;
    int kz_london_start_hour, kz_london_end_hour;
    int kz_newyork_start_hour, kz_newyork_end_hour;
    int kz_londonclose_start_hour, kz_londonclose_end_hour;

    // Global SL/TP limits (fallback if per-strategy is 0)
    double min_sl_distance;
    double max_sl_distance;
    double min_tp_distance;
    double max_tp_distance;
    double min_rr_ratio;
    int    max_tp_lookback;

    SignalConfig()
        : sl_zone_buffer(0.20), sweep_lookback(15)
        , ote_fib_low(0.62), ote_fib_high(0.79)
        , kz_asian_start_hour(20), kz_asian_end_hour(22)
        , kz_london_start_hour(2), kz_london_end_hour(5)
        , kz_newyork_start_hour(8), kz_newyork_end_hour(13)
        , kz_londonclose_start_hour(15), kz_londonclose_end_hour(17)
        , min_sl_distance(0.0), max_sl_distance(0.0)
        , min_tp_distance(0.0), max_tp_distance(0.0)
        , min_rr_ratio(0.0), max_tp_lookback(100)
    {}
};

// Signal candidate (internal)
struct SignalCandidate {
    Signal signal;
    double score;
    double raw_sl_dist;
    double raw_tp_dist;

    bool has_ob, has_fvg, has_bos, has_choch;
    bool in_premium_discount, in_ote_zone, has_liq_sweep;
    bool ob_fvg_overlap, in_kill_zone, zone_is_fresh, ob_high_prob;
    int  zone_visit_count;
};

class SignalGenerator {
public:
    SignalGenerator();
    void Init(const SignalConfig& config);

    Signal Generate(
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper,
        const SwingDetector& bias_swings);

    const Signal& GetLastSignal() const;
    double GetLastScore() const;
    const SignalConfig& GetConfig() const;
    void SetConfig(const SignalConfig& config);

private:
    SignalConfig config_;
    Signal last_signal_;
    double last_score_;
    int last_used_bos_index_;    // prevent same BOS generating multiple signals
    int last_used_choch_index_;  // prevent same CHoCH generating multiple signals
    const SwingDetector* bias_swings_; // HTF swings for TP targets (set per Generate call)

    // Helpers
    bool IsInDiscount(double price, const SwingDetector& swings) const;
    bool IsInPremium(double price, const SwingDetector& swings) const;
    bool IsInOTE(double price, int direction, const SwingDetector& swings) const;
    bool IsInKillZone(int64_t bar_time) const;
    int  GetHourFromTimestamp(int64_t ts) const;

    // SL/TP — uses per-strategy limits with global fallback
    double FindStructuralSL(int direction, double entry,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const OrderBlockTracker& ob_tracker, const FVGDetector& fvg_detector,
        const StrategyParams& sp) const;

    double FindStructuralTP(int direction, double entry,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const LiquidityMapper& liq_mapper) const;

    // Strategy scanners
    SignalCandidate ScanOBBounce(int dir, int bar_idx,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const;

    SignalCandidate ScanBOSContinuation(int dir, int bar_idx,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const;

    SignalCandidate ScanCHoCHReversal(int dir, int bar_idx,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const;

    SignalCandidate ScanFVGFill(int dir, int bar_idx,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const;

    SignalCandidate ScanLiqSweep(int dir, int bar_idx,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const;

    double CalculateScore(const SignalCandidate& candidate) const;

    void FillConfluence(SignalCandidate& c, double price, int direction, int bar_idx,
        const std::vector<Bar>& bars, const SwingDetector& swings,
        const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
        const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const;

    // Check per-strategy filters (OTE, high_prob, kill_zone, freshness)
    bool PassesStrategyFilters(const SignalCandidate& c, const StrategyParams& sp) const;

    SignalCandidate MakeEmpty() const;
};
