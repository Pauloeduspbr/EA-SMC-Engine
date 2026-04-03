#include "smc_engine.h"
#include <cstring>

static SMCEngine g_engine;

SMCEngine::SMCEngine() : initialized_(false) { std::memset(&current_signal_, 0, sizeof(Signal)); }
SMCEngine::~SMCEngine() {}

void SMCEngine::Init(int swing_length, int ob_lookback, double liq_range_pct,
                     bool close_break, bool close_mitigation, bool join_fvg,
                     int bias_swing_length) {
    std::lock_guard<std::mutex> lock(mtx_);
    bars_.clear();
    bars_.reserve(10000);
    swing_detector_.Init(swing_length);
    // Bias swing detector: uses larger swings for HTF structure
    // Default: 3x the entry swing length (e.g., swing=3 -> bias=9)
    int bsl = (bias_swing_length > 0) ? bias_swing_length : swing_length * 3;
    bias_swing_detector_.Init(bsl);
    structure_analyzer_.Init(close_break);
    ob_tracker_.Init(close_mitigation);
    fvg_detector_.Init(join_fvg);
    liq_mapper_.Init(liq_range_pct);
    SignalConfig cfg;
    signal_generator_.Init(cfg);
    std::memset(&current_signal_, 0, sizeof(Signal));
    initialized_ = true;
}

void SMCEngine::ConfigureGlobal(double sl_zone_buffer, int sweep_lookback,
                                 double ote_fib_low, double ote_fib_high,
                                 int kz_as, int kz_ae, int kz_ls, int kz_le,
                                 int kz_ns, int kz_ne, int kz_lcs, int kz_lce,
                                 double min_sl, double max_sl,
                                 double min_tp, double max_tp,
                                 double min_rr, int max_tp_lookback) {
    std::lock_guard<std::mutex> lock(mtx_);
    SignalConfig cfg = signal_generator_.GetConfig();
    cfg.sl_zone_buffer = sl_zone_buffer;
    cfg.sweep_lookback = sweep_lookback;
    cfg.ote_fib_low = ote_fib_low;
    cfg.ote_fib_high = ote_fib_high;
    cfg.kz_asian_start_hour = kz_as; cfg.kz_asian_end_hour = kz_ae;
    cfg.kz_london_start_hour = kz_ls; cfg.kz_london_end_hour = kz_le;
    cfg.kz_newyork_start_hour = kz_ns; cfg.kz_newyork_end_hour = kz_ne;
    cfg.kz_londonclose_start_hour = kz_lcs; cfg.kz_londonclose_end_hour = kz_lce;
    cfg.min_sl_distance = min_sl; cfg.max_sl_distance = max_sl;
    cfg.min_tp_distance = min_tp; cfg.max_tp_distance = max_tp;
    cfg.min_rr_ratio = min_rr; cfg.max_tp_lookback = max_tp_lookback;
    signal_generator_.SetConfig(cfg);
}

void SMCEngine::ConfigureStrategy(int strategy_id, int enabled, int lookback,
                                   double min_score, int max_zone_visits,
                                   int require_ote, int require_high_prob,
                                   int require_kill_zone,
                                   double min_sl_pts, double max_sl_pts) {
    std::lock_guard<std::mutex> lock(mtx_);
    SignalConfig cfg = signal_generator_.GetConfig();
    StrategyParams sp;
    sp.enabled = (enabled != 0);
    sp.lookback = lookback;
    sp.min_score = min_score;
    sp.max_zone_visits = max_zone_visits;
    sp.require_ote = (require_ote != 0);
    sp.require_high_prob = (require_high_prob != 0);
    sp.require_kill_zone = (require_kill_zone != 0);
    sp.min_sl_points = min_sl_pts;
    sp.max_sl_points = max_sl_pts;

    switch (strategy_id) {
        case 1: cfg.ob_bounce = sp; break;
        case 2: cfg.bos_continuation = sp; break;
        case 3: cfg.choch_reversal = sp; break;
        case 4: cfg.fvg_fill = sp; break;
        case 5: cfg.liq_sweep = sp; break;
    }
    signal_generator_.SetConfig(cfg);
}

void SMCEngine::FeedBar(double open, double high, double low, double close,
                        int64_t volume, int64_t time) {
    std::lock_guard<std::mutex> lock(mtx_);
    Bar bar{}; bar.open=open; bar.high=high; bar.low=low; bar.close=close; bar.volume=volume; bar.time=time;
    bars_.push_back(bar);

    // Sliding window: trim oldest bars to keep processing O(MAX_BARS)
    if (static_cast<int>(bars_.size()) > MAX_BARS) {
        int excess = static_cast<int>(bars_.size()) - MAX_BARS;
        bars_.erase(bars_.begin(), bars_.begin() + excess);
    }
}

int SMCEngine::Process() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!initialized_ || bars_.size() < 20) return 0;
    swing_detector_.Calculate(bars_);
    bias_swing_detector_.Calculate(bars_);
    structure_analyzer_.Calculate(bars_, swing_detector_);
    structure_analyzer_.CalculateBias(bias_swing_detector_);
    ob_tracker_.Calculate(bars_, swing_detector_);
    fvg_detector_.Calculate(bars_);
    liq_mapper_.Calculate(bars_, swing_detector_);
    current_signal_ = signal_generator_.Generate(bars_, swing_detector_, structure_analyzer_, ob_tracker_, fvg_detector_, liq_mapper_, bias_swing_detector_);
    return static_cast<int>(bars_.size());
}

void SMCEngine::Reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    bars_.clear(); swing_detector_.Reset(); structure_analyzer_.Reset();
    ob_tracker_.Reset(); fvg_detector_.Reset(); liq_mapper_.Reset();
    std::memset(&current_signal_, 0, sizeof(Signal));
}

// --- Signal ---
int    SMCEngine::GetSignalDirection()  const { return current_signal_.direction; }
double SMCEngine::GetSignalEntry()      const { return current_signal_.entry_price; }
double SMCEngine::GetSignalSL()         const { return current_signal_.stop_loss; }
double SMCEngine::GetSignalTP()         const { return current_signal_.take_profit; }
double SMCEngine::GetSignalConfidence() const { return current_signal_.confidence; }
int    SMCEngine::GetSignalStrategy()   const { return static_cast<int>(current_signal_.strategy); }

// --- Swings ---
int    SMCEngine::GetSwingCount() const { return swing_detector_.GetCount(); }
int    SMCEngine::GetSwingType(int i) const { return (i>=0&&i<swing_detector_.GetCount()) ? static_cast<int>(swing_detector_.Get(i).type) : 0; }
double SMCEngine::GetSwingLevel(int i) const { return (i>=0&&i<swing_detector_.GetCount()) ? swing_detector_.Get(i).level : 0.0; }
int    SMCEngine::GetSwingBarIndex(int i) const { return (i>=0&&i<swing_detector_.GetCount()) ? swing_detector_.Get(i).index : -1; }

// --- Structure ---
int    SMCEngine::GetStructureBreakCount() const { return structure_analyzer_.GetBreakCount(); }
int    SMCEngine::GetStructureType(int i) const { return (i>=0&&i<structure_analyzer_.GetBreakCount()) ? static_cast<int>(structure_analyzer_.Get(i).type) : 0; }
int    SMCEngine::GetStructureDirection(int i) const { return (i>=0&&i<structure_analyzer_.GetBreakCount()) ? static_cast<int>(structure_analyzer_.Get(i).direction) : 0; }
double SMCEngine::GetStructureLevel(int i) const { return (i>=0&&i<structure_analyzer_.GetBreakCount()) ? structure_analyzer_.Get(i).level : 0.0; }
int    SMCEngine::GetStructureBarIndex(int i) const { return (i>=0&&i<structure_analyzer_.GetBreakCount()) ? structure_analyzer_.Get(i).index : -1; }
int    SMCEngine::GetCurrentTrend() const { return static_cast<int>(structure_analyzer_.GetCurrentTrend()); }
int    SMCEngine::GetBias() const { return static_cast<int>(structure_analyzer_.GetBias()); }

// --- OB ---
int    SMCEngine::GetOBCount() const { return ob_tracker_.GetTotalCount(); }
int    SMCEngine::GetActiveOBCount(int d) const { return ob_tracker_.GetActiveCount(d); }
double SMCEngine::GetOBTop(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).top : 0.0; }
double SMCEngine::GetOBBottom(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).bottom : 0.0; }
int    SMCEngine::GetOBDirection(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).direction : 0; }
int    SMCEngine::GetOBBarIndex(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).index : -1; }
bool   SMCEngine::GetOBMitigated(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).mitigated : false; }
int    SMCEngine::GetOBVisitCount(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).visit_count : 0; }
bool   SMCEngine::GetOBHighProb(int i) const { return (i>=0&&i<ob_tracker_.GetTotalCount()) ? ob_tracker_.Get(i).high_prob : false; }

// --- FVG ---
int    SMCEngine::GetFVGCount() const { return fvg_detector_.GetTotalCount(); }
int    SMCEngine::GetActiveFVGCount(int d) const { return fvg_detector_.GetActiveCount(d); }
double SMCEngine::GetFVGTop(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).top : 0.0; }
double SMCEngine::GetFVGBottom(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).bottom : 0.0; }
double SMCEngine::GetFVGMidpoint(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).midpoint : 0.0; }
int    SMCEngine::GetFVGDirection(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).direction : 0; }
int    SMCEngine::GetFVGBarIndex(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).index : -1; }
bool   SMCEngine::GetFVGMitigated(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).mitigated : false; }
int    SMCEngine::GetFVGVisitCount(int i) const { return (i>=0&&i<fvg_detector_.GetTotalCount()) ? fvg_detector_.Get(i).visit_count : 0; }

// --- Liquidity ---
int    SMCEngine::GetLiqPoolCount() const { return liq_mapper_.GetTotalCount(); }
int    SMCEngine::GetActiveLiqPoolCount(int d) const { return liq_mapper_.GetActiveCount(d); }
double SMCEngine::GetLiqPoolLevel(int i) const { return (i>=0&&i<liq_mapper_.GetTotalCount()) ? liq_mapper_.Get(i).level : 0.0; }
int    SMCEngine::GetLiqPoolDirection(int i) const { return (i>=0&&i<liq_mapper_.GetTotalCount()) ? liq_mapper_.Get(i).direction : 0; }
int    SMCEngine::GetLiqPoolCount_touches(int i) const { return (i>=0&&i<liq_mapper_.GetTotalCount()) ? liq_mapper_.Get(i).count : 0; }
bool   SMCEngine::GetLiqPoolSwept(int i) const { return (i>=0&&i<liq_mapper_.GetTotalCount()) ? liq_mapper_.Get(i).swept : false; }
int    SMCEngine::GetBarCount() const { return static_cast<int>(bars_.size()); }

// ============================================================================
// DLL Exports — try/catch on every function
// ============================================================================

DLLEXPORT void STDCALL SMC_Init(int sl, int ol, double lr, int cb, int cm, int jf, int bsl) {
    try { g_engine.Init(sl, ol, lr, cb!=0, cm!=0, jf!=0, bsl); } catch (...) {}
}

DLLEXPORT void STDCALL SMC_ConfigureGlobal(double szb, int swl, double ofl, double ofh,
    int kas, int kae, int kls, int kle, int kns, int kne, int klcs, int klce,
    double minsl, double maxsl, double mintp, double maxtp, double minrr, int mtpl) {
    try { g_engine.ConfigureGlobal(szb,swl,ofl,ofh,kas,kae,kls,kle,kns,kne,klcs,klce,minsl,maxsl,mintp,maxtp,minrr,mtpl); } catch (...) {}
}

DLLEXPORT void STDCALL SMC_ConfigureStrategy(int sid, int en, int lb, double ms, int mzv,
    int rote, int rhp, int rkz, double minslp, double maxslp) {
    try { g_engine.ConfigureStrategy(sid,en,lb,ms,mzv,rote,rhp,rkz,minslp,maxslp); } catch (...) {}
}

DLLEXPORT void STDCALL SMC_FeedBar(double o, double h, double l, double c, long long v, long long t) {
    try { g_engine.FeedBar(o,h,l,c,static_cast<int64_t>(v),static_cast<int64_t>(t)); } catch (...) {}
}

DLLEXPORT int  STDCALL SMC_Process() { try { return g_engine.Process(); } catch (...) { return -1; } }
DLLEXPORT void STDCALL SMC_Reset()   { try { g_engine.Reset(); } catch (...) {} }
DLLEXPORT void STDCALL SMC_Deinit()  { try { g_engine.Reset(); } catch (...) {} }

DLLEXPORT int    STDCALL SMC_GetSignalDirection()  { try { return g_engine.GetSignalDirection(); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetSignalEntry()      { try { return g_engine.GetSignalEntry(); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetSignalSL()         { try { return g_engine.GetSignalSL(); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetSignalTP()         { try { return g_engine.GetSignalTP(); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetSignalConfidence() { try { return g_engine.GetSignalConfidence(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetSignalStrategy()   { try { return g_engine.GetSignalStrategy(); } catch (...) { return 0; } }

DLLEXPORT int    STDCALL SMC_GetSwingCount()         { try { return g_engine.GetSwingCount(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetSwingType(int i)     { try { return g_engine.GetSwingType(i); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetSwingLevel(int i)    { try { return g_engine.GetSwingLevel(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetSwingBarIndex(int i)  { try { return g_engine.GetSwingBarIndex(i); } catch (...) { return -1; } }

DLLEXPORT int    STDCALL SMC_GetStructureBreakCount()      { try { return g_engine.GetStructureBreakCount(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetStructureType(int i)       { try { return g_engine.GetStructureType(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetStructureDirection(int i)   { try { return g_engine.GetStructureDirection(i); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetStructureLevel(int i)      { try { return g_engine.GetStructureLevel(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetStructureBarIndex(int i)    { try { return g_engine.GetStructureBarIndex(i); } catch (...) { return -1; } }
DLLEXPORT int    STDCALL SMC_GetCurrentTrend()             { try { return g_engine.GetCurrentTrend(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetBias()                    { try { return g_engine.GetBias(); } catch (...) { return 0; } }

DLLEXPORT int    STDCALL SMC_GetOBCount()                  { try { return g_engine.GetOBCount(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetActiveOBCount(int d)       { try { return g_engine.GetActiveOBCount(d); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetOBTop(int i)               { try { return g_engine.GetOBTop(i); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetOBBottom(int i)            { try { return g_engine.GetOBBottom(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetOBDirection(int i)         { try { return g_engine.GetOBDirection(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetOBBarIndex(int i)          { try { return g_engine.GetOBBarIndex(i); } catch (...) { return -1; } }
DLLEXPORT int    STDCALL SMC_GetOBMitigated(int i)         { try { return g_engine.GetOBMitigated(i)?1:0; } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetOBVisitCount(int i)        { try { return g_engine.GetOBVisitCount(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetOBHighProb(int i)          { try { return g_engine.GetOBHighProb(i)?1:0; } catch (...) { return 0; } }

DLLEXPORT int    STDCALL SMC_GetFVGCount()                 { try { return g_engine.GetFVGCount(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetActiveFVGCount(int d)      { try { return g_engine.GetActiveFVGCount(d); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetFVGTop(int i)              { try { return g_engine.GetFVGTop(i); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetFVGBottom(int i)           { try { return g_engine.GetFVGBottom(i); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetFVGMidpoint(int i)         { try { return g_engine.GetFVGMidpoint(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetFVGDirection(int i)        { try { return g_engine.GetFVGDirection(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetFVGBarIndex(int i)         { try { return g_engine.GetFVGBarIndex(i); } catch (...) { return -1; } }
DLLEXPORT int    STDCALL SMC_GetFVGMitigated(int i)        { try { return g_engine.GetFVGMitigated(i)?1:0; } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetFVGVisitCount(int i)       { try { return g_engine.GetFVGVisitCount(i); } catch (...) { return 0; } }

DLLEXPORT int    STDCALL SMC_GetLiqPoolCount()             { try { return g_engine.GetLiqPoolCount(); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetActiveLiqPoolCount(int d)  { try { return g_engine.GetActiveLiqPoolCount(d); } catch (...) { return 0; } }
DLLEXPORT double STDCALL SMC_GetLiqPoolLevel(int i)        { try { return g_engine.GetLiqPoolLevel(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetLiqPoolDirection(int i)    { try { return g_engine.GetLiqPoolDirection(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetLiqPoolTouches(int i)      { try { return g_engine.GetLiqPoolCount_touches(i); } catch (...) { return 0; } }
DLLEXPORT int    STDCALL SMC_GetLiqPoolSwept(int i)        { try { return g_engine.GetLiqPoolSwept(i)?1:0; } catch (...) { return 0; } }

DLLEXPORT int    STDCALL SMC_GetBarCount()                 { try { return g_engine.GetBarCount(); } catch (...) { return 0; } }
