#pragma once

#include "types.h"
#include "swing_detector.h"
#include "structure_analyzer.h"
#include "ob_tracker.h"
#include "fvg_detector.h"
#include "liquidity_mapper.h"
#include "signal_generator.h"

#include <vector>
#include <mutex>

class SMCEngine {
public:
    SMCEngine();
    ~SMCEngine();

    void Init(int swing_length, int ob_lookback, double liq_range_pct,
              bool close_break, bool close_mitigation, bool join_fvg,
              int bias_swing_length = 0);

    // Global config
    void ConfigureGlobal(double sl_zone_buffer, int sweep_lookback,
                         double ote_fib_low, double ote_fib_high,
                         int kz_asian_start, int kz_asian_end,
                         int kz_london_start, int kz_london_end,
                         int kz_ny_start, int kz_ny_end,
                         int kz_londonclose_start, int kz_londonclose_end,
                         double min_sl, double max_sl,
                         double min_tp, double max_tp,
                         double min_rr, int max_tp_lookback);

    // Per-strategy config (strategy_id: 1=OB,2=BOS,3=CHoCH,4=FVG,5=LIQ)
    void ConfigureStrategy(int strategy_id, int enabled, int lookback,
                           double min_score, int max_zone_visits,
                           int require_ote, int require_high_prob,
                           int require_kill_zone,
                           double min_sl_pts, double max_sl_pts);

    void FeedBar(double open, double high, double low, double close,
                 int64_t volume, int64_t time);
    int Process();
    void Reset();

    // Signal queries
    int    GetSignalDirection() const;
    double GetSignalEntry() const;
    double GetSignalSL() const;
    double GetSignalTP() const;
    double GetSignalConfidence() const;
    int    GetSignalStrategy() const;

    // Swings
    int    GetSwingCount() const;
    int    GetSwingType(int idx) const;
    double GetSwingLevel(int idx) const;
    int    GetSwingBarIndex(int idx) const;

    // Structure
    int    GetStructureBreakCount() const;
    int    GetStructureType(int idx) const;
    int    GetStructureDirection(int idx) const;
    double GetStructureLevel(int idx) const;
    int    GetStructureBarIndex(int idx) const;
    int    GetCurrentTrend() const;
    int    GetBias() const;

    // Order Blocks
    int    GetOBCount() const;
    int    GetActiveOBCount(int direction) const;
    double GetOBTop(int idx) const;
    double GetOBBottom(int idx) const;
    int    GetOBDirection(int idx) const;
    int    GetOBBarIndex(int idx) const;
    bool   GetOBMitigated(int idx) const;
    int    GetOBVisitCount(int idx) const;
    bool   GetOBHighProb(int idx) const;

    // FVG
    int    GetFVGCount() const;
    int    GetActiveFVGCount(int direction) const;
    double GetFVGTop(int idx) const;
    double GetFVGBottom(int idx) const;
    double GetFVGMidpoint(int idx) const;
    int    GetFVGDirection(int idx) const;
    int    GetFVGBarIndex(int idx) const;
    bool   GetFVGMitigated(int idx) const;
    int    GetFVGVisitCount(int idx) const;

    // Liquidity
    int    GetLiqPoolCount() const;
    int    GetActiveLiqPoolCount(int direction) const;
    double GetLiqPoolLevel(int idx) const;
    int    GetLiqPoolDirection(int idx) const;
    int    GetLiqPoolCount_touches(int idx) const;
    bool   GetLiqPoolSwept(int idx) const;

    int GetBarCount() const;

private:
    static constexpr int MAX_BARS = 1500; // sliding window cap

    std::mutex          mtx_;
    std::vector<Bar>    bars_;
    SwingDetector       swing_detector_;
    SwingDetector       bias_swing_detector_;  // HTF bias (larger swings)
    StructureAnalyzer   structure_analyzer_;
    OrderBlockTracker   ob_tracker_;
    FVGDetector         fvg_detector_;
    LiquidityMapper     liq_mapper_;
    SignalGenerator     signal_generator_;
    Signal              current_signal_;
    bool initialized_;
};

// DLL Exports
#ifdef _WIN32
    #define DLLEXPORT extern "C" __declspec(dllexport)
    #define STDCALL   __stdcall
#else
    #define DLLEXPORT extern "C"
    #define STDCALL
#endif

DLLEXPORT void   STDCALL SMC_Init(int swing_length, int ob_lookback, double liq_range_pct,
                                   int close_break, int close_mitigation, int join_fvg,
                                   int bias_swing_length);
DLLEXPORT void   STDCALL SMC_ConfigureGlobal(double sl_zone_buffer, int sweep_lookback,
                                              double ote_fib_low, double ote_fib_high,
                                              int kz_as, int kz_ae, int kz_ls, int kz_le,
                                              int kz_ns, int kz_ne, int kz_lcs, int kz_lce,
                                              double min_sl, double max_sl,
                                              double min_tp, double max_tp,
                                              double min_rr, int max_tp_lookback);
DLLEXPORT void   STDCALL SMC_ConfigureStrategy(int strategy_id, int enabled, int lookback,
                                                double min_score, int max_zone_visits,
                                                int require_ote, int require_high_prob,
                                                int require_kill_zone,
                                                double min_sl_pts, double max_sl_pts);
DLLEXPORT void   STDCALL SMC_FeedBar(double open, double high, double low, double close,
                                      long long volume, long long time);
DLLEXPORT int    STDCALL SMC_Process();
DLLEXPORT void   STDCALL SMC_Reset();
DLLEXPORT void   STDCALL SMC_Deinit();

// Signal
DLLEXPORT int    STDCALL SMC_GetSignalDirection();
DLLEXPORT double STDCALL SMC_GetSignalEntry();
DLLEXPORT double STDCALL SMC_GetSignalSL();
DLLEXPORT double STDCALL SMC_GetSignalTP();
DLLEXPORT double STDCALL SMC_GetSignalConfidence();
DLLEXPORT int    STDCALL SMC_GetSignalStrategy();

// Swings
DLLEXPORT int    STDCALL SMC_GetSwingCount();
DLLEXPORT int    STDCALL SMC_GetSwingType(int idx);
DLLEXPORT double STDCALL SMC_GetSwingLevel(int idx);
DLLEXPORT int    STDCALL SMC_GetSwingBarIndex(int idx);

// Structure
DLLEXPORT int    STDCALL SMC_GetStructureBreakCount();
DLLEXPORT int    STDCALL SMC_GetStructureType(int idx);
DLLEXPORT int    STDCALL SMC_GetStructureDirection(int idx);
DLLEXPORT double STDCALL SMC_GetStructureLevel(int idx);
DLLEXPORT int    STDCALL SMC_GetStructureBarIndex(int idx);
DLLEXPORT int    STDCALL SMC_GetCurrentTrend();
DLLEXPORT int    STDCALL SMC_GetBias();

// Order Blocks
DLLEXPORT int    STDCALL SMC_GetOBCount();
DLLEXPORT int    STDCALL SMC_GetActiveOBCount(int direction);
DLLEXPORT double STDCALL SMC_GetOBTop(int idx);
DLLEXPORT double STDCALL SMC_GetOBBottom(int idx);
DLLEXPORT int    STDCALL SMC_GetOBDirection(int idx);
DLLEXPORT int    STDCALL SMC_GetOBBarIndex(int idx);
DLLEXPORT int    STDCALL SMC_GetOBMitigated(int idx);
DLLEXPORT int    STDCALL SMC_GetOBVisitCount(int idx);
DLLEXPORT int    STDCALL SMC_GetOBHighProb(int idx);

// FVG
DLLEXPORT int    STDCALL SMC_GetFVGCount();
DLLEXPORT int    STDCALL SMC_GetActiveFVGCount(int direction);
DLLEXPORT double STDCALL SMC_GetFVGTop(int idx);
DLLEXPORT double STDCALL SMC_GetFVGBottom(int idx);
DLLEXPORT double STDCALL SMC_GetFVGMidpoint(int idx);
DLLEXPORT int    STDCALL SMC_GetFVGDirection(int idx);
DLLEXPORT int    STDCALL SMC_GetFVGBarIndex(int idx);
DLLEXPORT int    STDCALL SMC_GetFVGMitigated(int idx);
DLLEXPORT int    STDCALL SMC_GetFVGVisitCount(int idx);

// Liquidity
DLLEXPORT int    STDCALL SMC_GetLiqPoolCount();
DLLEXPORT int    STDCALL SMC_GetActiveLiqPoolCount(int direction);
DLLEXPORT double STDCALL SMC_GetLiqPoolLevel(int idx);
DLLEXPORT int    STDCALL SMC_GetLiqPoolDirection(int idx);
DLLEXPORT int    STDCALL SMC_GetLiqPoolTouches(int idx);
DLLEXPORT int    STDCALL SMC_GetLiqPoolSwept(int idx);

DLLEXPORT int    STDCALL SMC_GetBarCount();
