#include "signal_generator.h"
#include <algorithm>
#include <cmath>
#include <ctime>

SignalGenerator::SignalGenerator() : last_score_(0.0) { last_signal_ = Signal{}; }
void SignalGenerator::Init(const SignalConfig& config) { config_ = config; last_signal_ = Signal{}; last_score_ = 0.0; }
const Signal& SignalGenerator::GetLastSignal() const { return last_signal_; }
double SignalGenerator::GetLastScore() const { return last_score_; }
const SignalConfig& SignalGenerator::GetConfig() const { return config_; }
void SignalGenerator::SetConfig(const SignalConfig& config) { config_ = config; }

// ============================================================================
// Generate — scans all enabled strategies with per-strategy params
// ============================================================================

Signal SignalGenerator::Generate(
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper)
{
    last_signal_ = Signal{}; last_score_ = 0.0;
    int n = static_cast<int>(bars.size());
    if (n < 50) return last_signal_;

    TrendDirection trend = structure.GetCurrentTrend();
    if (trend == TREND_NONE) return last_signal_;
    int direction = static_cast<int>(trend);
    int bar_idx = n - 1;

    std::vector<SignalCandidate> candidates;

    if (config_.ob_bounce.enabled) {
        auto c = ScanOBBounce(direction, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);
        if (c.signal.direction != 0) candidates.push_back(c);
    }
    if (config_.bos_continuation.enabled) {
        auto c = ScanBOSContinuation(direction, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);
        if (c.signal.direction != 0) candidates.push_back(c);
    }
    if (config_.choch_reversal.enabled) {
        // CHoCH reversal: scan BOTH directions because a CHoCH signals
        // a trend change — the new direction may not yet be the current trend
        auto c1 = ScanCHoCHReversal(direction, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);
        if (c1.signal.direction != 0) candidates.push_back(c1);
        auto c2 = ScanCHoCHReversal(-direction, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);
        if (c2.signal.direction != 0) candidates.push_back(c2);
    }
    if (config_.fvg_fill.enabled) {
        auto c = ScanFVGFill(direction, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);
        if (c.signal.direction != 0) candidates.push_back(c);
    }
    if (config_.liq_sweep.enabled) {
        auto c = ScanLiqSweep(direction, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);
        if (c.signal.direction != 0) candidates.push_back(c);
    }

    SignalCandidate best = MakeEmpty();
    for (auto& c : candidates) {
        c.score = CalculateScore(c);
        if (c.score > best.score) best = c;
    }

    if (best.signal.direction != 0) {
        best.signal.confidence = best.score;
        best.signal.bar_index = bar_idx;
        last_signal_ = best.signal;
        last_score_ = best.score;
    }
    return last_signal_;
}

// ============================================================================
// Helpers
// ============================================================================

bool SignalGenerator::IsInDiscount(double price, const SwingDetector& swings) const {
    SwingPoint h = swings.GetLastHigh(), l = swings.GetLastLow();
    if (h.index < 0 || l.index < 0) return false;
    return price < (h.level + l.level) / 2.0;
}

bool SignalGenerator::IsInPremium(double price, const SwingDetector& swings) const {
    SwingPoint h = swings.GetLastHigh(), l = swings.GetLastLow();
    if (h.index < 0 || l.index < 0) return false;
    return price > (h.level + l.level) / 2.0;
}

bool SignalGenerator::IsInOTE(double price, int direction, const SwingDetector& swings) const {
    SwingPoint h = swings.GetLastHigh(), l = swings.GetLastLow();
    if (h.index < 0 || l.index < 0) return false;
    double range = h.level - l.level;
    if (range <= 0) return false;
    if (direction == 1) {
        return price >= (h.level - range * config_.ote_fib_high) &&
               price <= (h.level - range * config_.ote_fib_low);
    } else {
        return price >= (l.level + range * config_.ote_fib_low) &&
               price <= (l.level + range * config_.ote_fib_high);
    }
}

int SignalGenerator::GetHourFromTimestamp(int64_t ts) const {
    time_t t = static_cast<time_t>(ts);
    struct tm* tm_info = gmtime(&t);
    return tm_info ? tm_info->tm_hour : -1;
}

bool SignalGenerator::IsInKillZone(int64_t bar_time) const {
    int hour = GetHourFromTimestamp(bar_time);
    if (hour < 0) return false;
    auto in_range = [](int h, int s, int e) -> bool {
        if (s == e) return false;
        return (s < e) ? (h >= s && h < e) : (h >= s || h < e);
    };
    return in_range(hour, config_.kz_asian_start_hour, config_.kz_asian_end_hour) ||
           in_range(hour, config_.kz_london_start_hour, config_.kz_london_end_hour) ||
           in_range(hour, config_.kz_newyork_start_hour, config_.kz_newyork_end_hour) ||
           in_range(hour, config_.kz_londonclose_start_hour, config_.kz_londonclose_end_hour);
}

// ============================================================================
// Structural SL — per-strategy min/max with global fallback
// ============================================================================

double SignalGenerator::FindStructuralSL(
    int direction, double entry, const std::vector<Bar>& bars,
    const SwingDetector& swings, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const StrategyParams& sp) const
{
    double sl = 0.0;
    if (direction == 1) {
        // Nearest swing low below entry
        for (int i = swings.GetCount() - 1; i >= 0; --i) {
            const auto& s = swings.Get(i);
            if (s.type == SWING_LOW && s.level < entry) {
                sl = s.level;
                break;
            }
        }
        // OB bottom if gives wider protection
        const OrderBlock* ob = ob_tracker.FindOBAtPrice(entry, 1);
        if (ob && (sl == 0 || ob->bottom < sl)) sl = ob->bottom;

        if (sl > 0) sl -= (entry - sl) * config_.sl_zone_buffer;
    } else {
        for (int i = swings.GetCount() - 1; i >= 0; --i) {
            const auto& s = swings.Get(i);
            if (s.type == SWING_HIGH && s.level > entry) {
                sl = s.level;
                break;
            }
        }
        const OrderBlock* ob = ob_tracker.FindOBAtPrice(entry, -1);
        if (ob && (sl == 0 || ob->top > sl)) sl = ob->top;

        if (sl > 0) sl += (sl - entry) * config_.sl_zone_buffer;
    }

    if (sl == 0.0) return 0.0;

    double dist = std::fabs(entry - sl);

    // Per-strategy min SL (fallback to global)
    double min_sl = (sp.min_sl_points > 0) ? sp.min_sl_points : config_.min_sl_distance;
    if (min_sl > 0 && dist < min_sl) {
        sl = (direction == 1) ? entry - min_sl : entry + min_sl;
        dist = min_sl;
    }

    // Per-strategy max SL (fallback to global) — REJECT if too far
    double max_sl = (sp.max_sl_points > 0) ? sp.max_sl_points : config_.max_sl_distance;
    if (max_sl > 0 && dist > max_sl) return 0.0;

    return sl;
}

// ============================================================================
// Structural TP
// ============================================================================

double SignalGenerator::FindStructuralTP(
    int direction, double entry, const std::vector<Bar>& bars,
    const SwingDetector& swings, const LiquidityMapper& liq_mapper) const
{
    int n = static_cast<int>(bars.size());
    double min_tp = config_.min_tp_distance;
    double tp = 0.0;

    // Progressive lookback: try configured, then 2x, then all bars
    // This ensures we always find a structural target if one exists
    int lookbacks[] = { config_.max_tp_lookback, config_.max_tp_lookback * 2, n };

    for (int attempt = 0; attempt < 3 && tp == 0.0; ++attempt) {
        int min_bar = n - 1 - lookbacks[attempt];
        if (min_bar < 0) min_bar = 0;
        double best_dist = 1e18;

        if (direction == 1) {
            for (int i = swings.GetCount() - 1; i >= 0; --i) {
                const auto& s = swings.Get(i);
                if (s.index < min_bar) break;
                if (s.type == SWING_HIGH && s.level > entry) {
                    double d = s.level - entry;
                    if (min_tp > 0 && d < min_tp) continue;
                    if (d < best_dist) { best_dist = d; tp = s.level; }
                }
            }
        } else {
            for (int i = swings.GetCount() - 1; i >= 0; --i) {
                const auto& s = swings.Get(i);
                if (s.index < min_bar) break;
                if (s.type == SWING_LOW && s.level < entry) {
                    double d = entry - s.level;
                    if (min_tp > 0 && d < min_tp) continue;
                    if (d < best_dist) { best_dist = d; tp = s.level; }
                }
            }
        }
    }

    // Also check liquidity pools (not limited by lookback)
    if (direction == 1) {
        const LiquidityPool* pool = liq_mapper.FindNearest(entry, 1);
        if (pool && pool->level > entry) {
            double d = pool->level - entry;
            if ((min_tp <= 0 || d >= min_tp) && (tp == 0.0 || d < std::fabs(tp - entry)))
                tp = pool->level;
        }
    } else {
        const LiquidityPool* pool = liq_mapper.FindNearest(entry, -1);
        if (pool && pool->level < entry) {
            double d = entry - pool->level;
            if ((min_tp <= 0 || d >= min_tp) && (tp == 0.0 || d < std::fabs(tp - entry)))
                tp = pool->level;
        }
    }

    if (tp == 0.0) return 0.0;

    // Max TP cap
    if (config_.max_tp_distance > 0) {
        double d = std::fabs(tp - entry);
        if (d > config_.max_tp_distance)
            tp = (direction == 1) ? entry + config_.max_tp_distance : entry - config_.max_tp_distance;
    }
    return tp;
}

// ============================================================================
// Scoring
// ============================================================================

double SignalGenerator::CalculateScore(const SignalCandidate& c) const {
    double score = 0.0;
    if (c.has_ob)               score += 0.15;
    if (c.has_fvg)              score += 0.15;
    if (c.has_bos)              score += 0.10;
    if (c.has_choch)            score += 0.10;
    if (c.in_premium_discount)  score += 0.08;
    if (c.in_ote_zone)          score += 0.12;
    if (c.has_liq_sweep)        score += 0.10;
    if (c.ob_fvg_overlap)       score += 0.05;
    if (c.in_kill_zone)         score += 0.08;
    if (c.zone_is_fresh)        score += 0.05;
    if (c.ob_high_prob)         score += 0.05;
    return std::min(1.0, std::max(0.0, score));
}

void SignalGenerator::FillConfluence(
    SignalCandidate& c, double price, int direction, int bar_idx,
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const
{
    const OrderBlock* ob = ob_tracker.FindOBAtPrice(price, direction);
    const FairValueGap* fvg = fvg_detector.FindFVGAtPrice(price, direction);
    c.has_ob = (ob != nullptr);
    c.has_fvg = (fvg != nullptr);
    c.ob_fvg_overlap = (ob && fvg);
    c.in_premium_discount = (direction == 1) ? IsInDiscount(price, swings) : IsInPremium(price, swings);
    c.in_ote_zone = IsInOTE(price, direction, swings);
    c.has_liq_sweep = liq_mapper.WasRecentlySwepted(-direction, config_.sweep_lookback, bar_idx);
    c.in_kill_zone = (bar_idx < (int)bars.size()) ? IsInKillZone(bars[bar_idx].time) : false;
    int visits = 0;
    if (ob) visits = std::max(visits, ob->visit_count);
    if (fvg) visits = std::max(visits, fvg->visit_count);
    c.zone_is_fresh = (visits <= 1);
    c.zone_visit_count = visits;
    c.ob_high_prob = (ob && ob->high_prob);
}

// ============================================================================
// Per-strategy filter check
// ============================================================================

bool SignalGenerator::PassesStrategyFilters(const SignalCandidate& c, const StrategyParams& sp) const {
    double score = CalculateScore(c);
    if (score < sp.min_score) return false;
    if (sp.require_ote && !c.in_ote_zone) return false;
    if (sp.require_high_prob && !c.ob_high_prob) return false;
    if (sp.require_kill_zone && !c.in_kill_zone) return false;
    // max_zone_visits is checked per-scanner (OB/FVG have their own checks)
    // This is a final guard using the zone_visit_count stored in the candidate
    if (sp.max_zone_visits > 0 && c.zone_visit_count > sp.max_zone_visits) return false;
    return true;
}

SignalCandidate SignalGenerator::MakeEmpty() const {
    SignalCandidate c{};
    c.signal = Signal{};
    c.score = 0.0; c.raw_sl_dist = 0.0; c.raw_tp_dist = 0.0;
    c.has_ob = c.has_fvg = c.has_bos = c.has_choch = false;
    c.in_premium_discount = c.in_ote_zone = c.has_liq_sweep = false;
    c.ob_fvg_overlap = c.in_kill_zone = c.zone_is_fresh = c.ob_high_prob = false;
    c.zone_visit_count = 0;
    return c;
}

// ============================================================================
// Build candidate + validate RR
// ============================================================================

static bool Build(SignalCandidate& c, int dir, StrategyType strat,
                  double entry, double sl, double tp, int bar_idx, double min_rr) {
    if (sl == 0.0 || tp == 0.0) return false;
    if (dir == 1) { if (sl >= entry || tp <= entry) return false; }
    else { if (sl <= entry || tp >= entry) return false; }
    double sl_d = std::fabs(entry - sl), tp_d = std::fabs(tp - entry);
    if (min_rr > 0.0 && sl_d > 0.0 && tp_d / sl_d < min_rr) return false;
    c.signal.direction = dir; c.signal.strategy = strat;
    c.signal.entry_price = entry; c.signal.stop_loss = sl; c.signal.take_profit = tp;
    c.signal.bar_index = bar_idx; c.raw_sl_dist = sl_d; c.raw_tp_dist = tp_d;
    return true;
}

// ============================================================================
// STRATEGY 1: OB Bounce
// ============================================================================

SignalCandidate SignalGenerator::ScanOBBounce(int dir, int bar_idx,
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const
{
    auto c = MakeEmpty();
    const auto& sp = config_.ob_bounce;
    double price = bars[bar_idx].close;

    const OrderBlock* ob = ob_tracker.FindOBAtPrice(price, dir);
    if (!ob) return c;
    if ((bar_idx - ob->index) > sp.lookback * 2) return c;

    // Per-strategy zone freshness check
    if (sp.max_zone_visits > 0 && ob->visit_count > sp.max_zone_visits) return c;

    double sl = FindStructuralSL(dir, price, bars, swings, ob_tracker, fvg_detector, sp);
    double tp = FindStructuralTP(dir, price, bars, swings, liq_mapper);
    if (!Build(c, dir, STRAT_OB_BOUNCE, price, sl, tp, bar_idx, config_.min_rr_ratio)) return MakeEmpty();

    // Check if current trend is from BOS or CHoCH
    {
        StructureBreak lb = structure.GetLastBOS();
        StructureBreak lc = structure.GetLastCHoCH();
        if (lb.index >= 0 && static_cast<int>(lb.direction) == dir)
            c.has_bos = true;
        if (lc.index >= 0 && static_cast<int>(lc.direction) == dir)
            c.has_choch = true;
    }
    FillConfluence(c, price, dir, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);

    if (!PassesStrategyFilters(c, sp)) return MakeEmpty();
    return c;
}

// ============================================================================
// STRATEGY 2: BOS Continuation
// ============================================================================

SignalCandidate SignalGenerator::ScanBOSContinuation(int dir, int bar_idx,
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const
{
    auto c = MakeEmpty();
    const auto& sp = config_.bos_continuation;
    double price = bars[bar_idx].close;

    StructureBreak last_bos = structure.GetLastBOS();
    if (last_bos.index < 0) return c;
    if (static_cast<int>(last_bos.direction) != dir) return c;
    if ((bar_idx - last_bos.index) > sp.lookback) return c;

    const OrderBlock* ob = ob_tracker.FindOBAtPrice(price, dir);
    const FairValueGap* fvg = fvg_detector.FindFVGAtPrice(price, dir);
    if (!ob && !fvg) return c;

    double sl = FindStructuralSL(dir, price, bars, swings, ob_tracker, fvg_detector, sp);
    double tp = FindStructuralTP(dir, price, bars, swings, liq_mapper);
    if (!Build(c, dir, STRAT_BOS_CONTINUATION, price, sl, tp, bar_idx, config_.min_rr_ratio)) return MakeEmpty();

    c.has_bos = true;
    FillConfluence(c, price, dir, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);

    if (!PassesStrategyFilters(c, sp)) return MakeEmpty();
    return c;
}

// ============================================================================
// STRATEGY 3: CHoCH Reversal
// ============================================================================

SignalCandidate SignalGenerator::ScanCHoCHReversal(int dir, int bar_idx,
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const
{
    auto c = MakeEmpty();
    const auto& sp = config_.choch_reversal;
    double price = bars[bar_idx].close;

    StructureBreak last_choch = structure.GetLastCHoCH();
    if (last_choch.index < 0) return c;
    if (static_cast<int>(last_choch.direction) != dir) return c;
    if ((bar_idx - last_choch.index) > sp.lookback) return c;

    const OrderBlock* ob = ob_tracker.FindOBAtPrice(price, dir);
    const FairValueGap* fvg = fvg_detector.FindFVGAtPrice(price, dir);
    if (!ob && !fvg) return c;

    double sl = FindStructuralSL(dir, price, bars, swings, ob_tracker, fvg_detector, sp);
    double tp = FindStructuralTP(dir, price, bars, swings, liq_mapper);
    if (!Build(c, dir, STRAT_CHOCH_REVERSAL, price, sl, tp, bar_idx, config_.min_rr_ratio)) return MakeEmpty();

    c.has_choch = true;
    FillConfluence(c, price, dir, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);

    if (!PassesStrategyFilters(c, sp)) return MakeEmpty();
    return c;
}

// ============================================================================
// STRATEGY 4: FVG Fill
// ============================================================================

SignalCandidate SignalGenerator::ScanFVGFill(int dir, int bar_idx,
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const
{
    auto c = MakeEmpty();
    const auto& sp = config_.fvg_fill;
    double price = bars[bar_idx].close;

    if (structure.GetCurrentTrend() != static_cast<TrendDirection>(dir)) return c;
    const FairValueGap* fvg = fvg_detector.FindFVGAtPrice(price, dir);
    if (!fvg) return c;
    if ((bar_idx - fvg->index) > sp.lookback) return c;
    if (sp.max_zone_visits > 0 && fvg->visit_count > sp.max_zone_visits) return c;

    double sl = FindStructuralSL(dir, price, bars, swings, ob_tracker, fvg_detector, sp);
    double tp = FindStructuralTP(dir, price, bars, swings, liq_mapper);
    if (!Build(c, dir, STRAT_FVG_FILL, price, sl, tp, bar_idx, config_.min_rr_ratio)) return MakeEmpty();

    // Check if current trend is from BOS or CHoCH
    StructureBreak last_bos = structure.GetLastBOS();
    StructureBreak last_choch = structure.GetLastCHoCH();
    if (last_bos.index >= 0 && static_cast<int>(last_bos.direction) == dir)
        c.has_bos = true;
    if (last_choch.index >= 0 && static_cast<int>(last_choch.direction) == dir)
        c.has_choch = true;
    FillConfluence(c, price, dir, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);

    if (!PassesStrategyFilters(c, sp)) return MakeEmpty();
    return c;
}

// ============================================================================
// STRATEGY 5: Liquidity Sweep
// ============================================================================

SignalCandidate SignalGenerator::ScanLiqSweep(int dir, int bar_idx,
    const std::vector<Bar>& bars, const SwingDetector& swings,
    const StructureAnalyzer& structure, const OrderBlockTracker& ob_tracker,
    const FVGDetector& fvg_detector, const LiquidityMapper& liq_mapper) const
{
    auto c = MakeEmpty();
    const auto& sp = config_.liq_sweep;
    double price = bars[bar_idx].close;

    if (!liq_mapper.WasRecentlySwepted(-dir, config_.sweep_lookback, bar_idx)) return c;

    StructureBreak last_choch = structure.GetLastCHoCH();
    StructureBreak last_bos = structure.GetLastBOS();
    bool has_struct = false, is_choch = false;
    if (last_choch.index >= 0 && static_cast<int>(last_choch.direction) == dir
        && (bar_idx - last_choch.index) <= sp.lookback) { has_struct = true; is_choch = true; }
    if (last_bos.index >= 0 && static_cast<int>(last_bos.direction) == dir
        && (bar_idx - last_bos.index) <= sp.lookback) { has_struct = true; }
    if (!has_struct) return c;

    // After sweep, price displaces rapidly. Entry confirmation:
    // price must be in OB/FVG zone OR near a recent OB/FVG (within SL range)
    const OrderBlock* ob = ob_tracker.FindOBAtPrice(price, dir);
    const FairValueGap* fvg = fvg_detector.FindFVGAtPrice(price, dir);
    bool near_ob = false;
    if (!ob) {
        // Check if there's a nearby active OB in the right direction
        for (const auto* active_ob : ob_tracker.GetActiveOBs(dir)) {
            double ob_mid = (active_ob->top + active_ob->bottom) / 2.0;
            double dist = std::fabs(price - ob_mid);
            double zone_h = active_ob->top - active_ob->bottom;
            // Accept if price is within 2x the OB zone height
            if (dist <= zone_h * 2.0) { ob = active_ob; near_ob = true; break; }
        }
    }
    if (!ob && !fvg) return c;

    double sl = FindStructuralSL(dir, price, bars, swings, ob_tracker, fvg_detector, sp);
    double tp = FindStructuralTP(dir, price, bars, swings, liq_mapper);
    if (!Build(c, dir, STRAT_LIQ_SWEEP, price, sl, tp, bar_idx, config_.min_rr_ratio)) return MakeEmpty();

    c.has_bos = !is_choch; c.has_choch = is_choch; c.has_liq_sweep = true;
    FillConfluence(c, price, dir, bar_idx, bars, swings, structure, ob_tracker, fvg_detector, liq_mapper);

    if (!PassesStrategyFilters(c, sp)) return MakeEmpty();
    return c;
}
