#pragma once

#include <cstdint>
#include <cfloat>

// ============================================================================
// SMC Engine — Core Types
// ============================================================================

#pragma pack(push, 1)

// --- Bar data (OHLCV) ---
struct Bar {
    double open;
    double high;
    double low;
    double close;
    int64_t volume;
    int64_t time;   // unix timestamp
};

// --- Swing Point ---
enum SwingType : int {
    SWING_NONE = 0,
    SWING_HIGH = 1,
    SWING_LOW  = -1
};

struct SwingPoint {
    int       index;    // bar index in the series
    double    level;    // price level (high for SH, low for SL)
    SwingType type;
    int64_t   time;
};

// --- Market Structure ---
enum StructureType : int {
    STRUCT_NONE  = 0,
    STRUCT_BOS   = 1,   // Break of Structure (continuation)
    STRUCT_CHOCH = 2    // Change of Character (reversal)
};

enum TrendDirection : int {
    TREND_NONE    = 0,
    TREND_BULLISH = 1,
    TREND_BEARISH = -1
};

struct StructureBreak {
    int            index;         // bar where detected
    StructureType  type;          // BOS or CHoCH
    TrendDirection direction;     // bullish or bearish
    double         level;         // price level broken
    int            broken_index;  // bar that actually broke the level
    int64_t        time;
};

// --- Order Block ---
struct OrderBlock {
    int       index;         // bar index of the OB candle
    double    top;
    double    bottom;
    int       direction;     // 1 = bullish (demand), -1 = bearish (supply)
    double    ob_volume;     // cumulative volume
    double    percentage;    // volume balance %
    bool      mitigated;
    int       mitigated_index;
    int64_t   time;
    int       visit_count;   // how many times price touched this zone (freshness)
    bool      high_prob;     // true = liquidity sweep + BOS preceded this OB
};

// --- Fair Value Gap ---
struct FairValueGap {
    int       index;         // bar index (middle candle)
    double    top;
    double    bottom;
    double    midpoint;      // consequent encroachment (CE) = (top+bottom)/2
    int       direction;     // 1 = bullish, -1 = bearish
    bool      mitigated;
    int       mitigated_index;
    int64_t   time;
    int       visit_count;   // freshness tracking
};

// --- Liquidity Pool ---
struct LiquidityPool {
    int       start_index;   // first swing in the group
    int       end_index;     // last swing in the group
    double    level;         // average level
    int       direction;     // 1 = buy-side (highs), -1 = sell-side (lows)
    int       count;         // number of touches
    bool      swept;
    int       swept_index;   // bar that swept it
};

// --- Kill Zone (session time window) ---
enum KillZoneType : int {
    KZ_NONE         = 0,
    KZ_ASIAN        = 1,
    KZ_LONDON       = 2,
    KZ_NEW_YORK     = 3,
    KZ_LONDON_CLOSE = 4
};

// --- Signal ---
enum StrategyType : int {
    STRAT_NONE            = 0,
    STRAT_OB_BOUNCE       = 1,
    STRAT_BOS_CONTINUATION = 2,
    STRAT_CHOCH_REVERSAL  = 3,
    STRAT_FVG_FILL        = 4,
    STRAT_LIQ_SWEEP       = 5
};

struct Signal {
    int            direction;    // 1 = buy, -1 = sell, 0 = none
    StrategyType   strategy;
    double         entry_price;
    double         stop_loss;
    double         take_profit;
    double         confidence;   // 0.0 to 1.0
    int            bar_index;    // bar where signal generated
};

// --- Engine Statistics (for OnTester) ---
struct EngineStats {
    int total_signals;
    int buy_signals;
    int sell_signals;
    int active_ob_count;
    int active_fvg_count;
    int active_liq_count;
    int swing_count;
    int bos_count;
    int choch_count;
};

#pragma pack(pop)
