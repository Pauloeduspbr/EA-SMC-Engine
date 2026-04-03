//+------------------------------------------------------------------+
//|                                                      EA_SMC.mq5  |
//|                        Smart Money Concepts EA — DLL Bridge       |
//|                        All strategy logic in smc_engine.dll       |
//+------------------------------------------------------------------+
#property copyright "SMC Engine"
#property version   "1.00"
#property description "Smart Money Concepts EA"
#property description "Strategies: OB Bounce, BOS Continuation, CHoCH Reversal, FVG Fill, Liquidity Sweep"
#property description "All detection & signals computed in C++ DLL"

#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>
#include <Trade\SymbolInfo.mqh>

//+------------------------------------------------------------------+
//| DLL IMPORTS                                                       |
//+------------------------------------------------------------------+
#import "smc_engine.dll"
   // Lifecycle
   void   SMC_Init(int swing_length, int ob_lookback, double liq_range_pct,
                    int close_break, int close_mitigation, int join_fvg,
                    int bias_swing_length);
   void   SMC_ConfigureGlobal(double sl_zone_buffer, int sweep_lookback,
                               double ote_fib_low, double ote_fib_high,
                               int kz_as, int kz_ae, int kz_ls, int kz_le,
                               int kz_ns, int kz_ne, int kz_lcs, int kz_lce,
                               double min_sl, double max_sl,
                               double min_tp, double max_tp,
                               double min_rr, int max_tp_lookback);
   void   SMC_ConfigureStrategy(int strategy_id, int enabled, int lookback,
                                 double min_score, int max_zone_visits,
                                 int require_ote, int require_high_prob,
                                 int require_kill_zone,
                                 double min_sl_pts, double max_sl_pts);
   void   SMC_FeedBar(double open, double high, double low, double close,
                       long volume, long time);
   int    SMC_Process();
   void   SMC_Reset();
   void   SMC_Deinit();

   // Signal
   int    SMC_GetSignalDirection();
   double SMC_GetSignalEntry();
   double SMC_GetSignalSL();
   double SMC_GetSignalTP();
   double SMC_GetSignalConfidence();
   int    SMC_GetSignalStrategy();

   // Swings
   int    SMC_GetSwingCount();
   int    SMC_GetSwingType(int idx);
   double SMC_GetSwingLevel(int idx);
   int    SMC_GetSwingBarIndex(int idx);

   // Structure
   int    SMC_GetStructureBreakCount();
   int    SMC_GetStructureType(int idx);
   int    SMC_GetStructureDirection(int idx);
   double SMC_GetStructureLevel(int idx);
   int    SMC_GetStructureBarIndex(int idx);
   int    SMC_GetCurrentTrend();
   int    SMC_GetBias();

   // Order Blocks
   int    SMC_GetOBCount();
   int    SMC_GetActiveOBCount(int direction);
   double SMC_GetOBTop(int idx);
   double SMC_GetOBBottom(int idx);
   int    SMC_GetOBDirection(int idx);
   int    SMC_GetOBBarIndex(int idx);
   int    SMC_GetOBMitigated(int idx);
   int    SMC_GetOBVisitCount(int idx);
   int    SMC_GetOBHighProb(int idx);

   // FVG
   int    SMC_GetFVGCount();
   int    SMC_GetActiveFVGCount(int direction);
   double SMC_GetFVGTop(int idx);
   double SMC_GetFVGBottom(int idx);
   double SMC_GetFVGMidpoint(int idx);
   int    SMC_GetFVGDirection(int idx);
   int    SMC_GetFVGBarIndex(int idx);
   int    SMC_GetFVGMitigated(int idx);
   int    SMC_GetFVGVisitCount(int idx);

   // Liquidity
   int    SMC_GetLiqPoolCount();
   int    SMC_GetActiveLiqPoolCount(int direction);
   double SMC_GetLiqPoolLevel(int idx);
   int    SMC_GetLiqPoolDirection(int idx);
   int    SMC_GetLiqPoolTouches(int idx);
   int    SMC_GetLiqPoolSwept(int idx);

   // Stats
   int    SMC_GetBarCount();
#import

//+------------------------------------------------------------------+
//| ENUMS                                                             |
//+------------------------------------------------------------------+
enum ENUM_MARKET_TYPE
{
   MARKET_AUTO,            // Auto Detect
   MARKET_FOREX,           // Forex
   MARKET_B3_INDICE,       // B3 - Mini Indice (WIN)
   MARKET_B3_DOLAR         // B3 - Mini Dolar (WDO)
};

//+------------------------------------------------------------------+
//| INPUT PARAMETERS                                                  |
//+------------------------------------------------------------------+
input group "============ GENERAL ============"
input ulong          InpMagic          = 202601;          // Magic Number
input ENUM_MARKET_TYPE InpMarketType   = MARKET_AUTO;     // Market Type
input string         InpComment        = "SMC";           // Order Comment
input int            InpHistoryBars    = 1000;            // Historical Bars to Feed on Init

input group "============ SMC DETECTION ============"
input int            InpSwingLength    = 5;               // Swing Detection Length
input int            InpBiasSwingLength = 0;              // HTF Bias Swing Length (0=auto 3x)
input int            InpOBLookback     = 50;              // Order Block Lookback (bars)
input double         InpLiqRangePct    = 0.01;            // Liquidity Range (% of price range)
input bool           InpCloseBreak     = true;            // BOS/CHoCH on Close (vs High/Low)
input bool           InpCloseMitigation = false;          // OB Mitigation on Close
input bool           InpJoinFVG        = false;           // Join Consecutive FVGs

input group "============ GLOBAL SIGNAL SETTINGS ============"
input double         InpSLZoneBuffer   = 0.20;            // SL Buffer (% of zone height)
input int            InpSweepLookback  = 15;              // Sweep Lookback (bars)
input int            InpMinSLPoints    = 150;             // Global Min SL Distance (points)
input int            InpMaxSLPoints    = 1000;            // Global Max SL Distance (points, 0=unlimited)
input int            InpMinTPPoints    = 300;             // Global Min TP Distance (points)
input int            InpMaxTPPoints    = 0;               // Global Max TP Distance (points, 0=unlimited)
input double         InpMinRR          = 0.0;             // Global Min TP/SL Ratio (0=disabled)
input int            InpMaxTPLookback  = 100;             // Max TP Lookback (bars)

input group "============ BOS CONTINUATION ============"
input bool           InpBOS_Enable     = true;            // Enable BOS Continuation
input int            InpBOS_Lookback   = 50;              // BOS Lookback (bars)
input double         InpBOS_MinScore   = 0.40;            // BOS Min Score
input int            InpBOS_MaxVisits  = 0;               // BOS Max Zone Visits (0=off)
input bool           InpBOS_ReqOTE     = false;           // BOS Require OTE Zone
input bool           InpBOS_ReqHP      = false;           // BOS Require High-Prob OB
input bool           InpBOS_ReqKZ      = false;           // BOS Require Kill Zone
input int            InpBOS_MaxSL      = 0;               // BOS Max SL (points, 0=use global)

input group "============ OB BOUNCE ============"
input bool           InpOB_Enable      = true;            // Enable OB Bounce
input int            InpOB_Lookback    = 30;              // OB Lookback (bars, shorter)
input double         InpOB_MinScore    = 0.40;            // OB Min Score (higher = stricter)
input int            InpOB_MaxVisits   = 1;               // OB Max Zone Visits (1=fresh only)
input bool           InpOB_ReqOTE      = false;           // OB Require OTE Zone
input bool           InpOB_ReqHP       = false;           // OB Require High-Prob OB
input bool           InpOB_ReqKZ       = false;           // OB Require Kill Zone
input int            InpOB_MaxSL       = 500;             // OB Max SL (points, tight)

input group "============ CHoCH REVERSAL ============"
input bool           InpCH_Enable      = true;            // Enable CHoCH Reversal
input int            InpCH_Lookback    = 50;              // CHoCH Lookback (bars, post-reversal pullback)
input double         InpCH_MinScore    = 0.40;            // CHoCH Min Score
input int            InpCH_MaxVisits   = 1;               // CHoCH Max Zone Visits
input bool           InpCH_ReqOTE      = false;           // CHoCH Require OTE Zone
input bool           InpCH_ReqHP       = false;           // CHoCH Require High-Prob OB
input bool           InpCH_ReqKZ       = false;           // CHoCH Require Kill Zone
input int            InpCH_MaxSL       = 500;             // CHoCH Max SL (points, tight)

input group "============ FVG FILL ============"
input bool           InpFVG_Enable     = true;            // Enable FVG Fill
input int            InpFVG_Lookback   = 30;              // FVG Lookback (bars)
input double         InpFVG_MinScore   = 0.40;            // FVG Min Score
input int            InpFVG_MaxVisits  = 1;               // FVG Max Zone Visits (fresh only)
input bool           InpFVG_ReqOTE     = false;           // FVG Require OTE Zone
input bool           InpFVG_ReqHP      = false;           // FVG Require High-Prob OB
input bool           InpFVG_ReqKZ      = false;           // FVG Require Kill Zone
input int            InpFVG_MaxSL      = 500;             // FVG Max SL (points)

input group "============ LIQUIDITY SWEEP ============"
input bool           InpLIQ_Enable     = true;            // Enable Liquidity Sweep
input int            InpLIQ_Lookback   = 30;              // LIQ Lookback (bars)
input double         InpLIQ_MinScore   = 0.35;            // LIQ Min Score
input int            InpLIQ_MaxVisits  = 0;               // LIQ Max Zone Visits (0=off)
input bool           InpLIQ_ReqOTE     = false;           // LIQ Require OTE Zone
input bool           InpLIQ_ReqHP      = false;           // LIQ Require High-Prob OB
input bool           InpLIQ_ReqKZ      = false;           // LIQ Require Kill Zone
input int            InpLIQ_MaxSL      = 0;               // LIQ Max SL (points, 0=use global)

input group "============ ICT OTE (Fibonacci) ============"
input double         InpOTEFibLow      = 0.62;            // OTE Fib Lower Bound
input double         InpOTEFibHigh     = 0.79;            // OTE Fib Upper Bound

input group "============ KILL ZONES (Broker Time Hours) ============"
input int            InpKZAsianStart   = 20;              // Asian KZ Start Hour
input int            InpKZAsianEnd     = 22;              // Asian KZ End Hour
input int            InpKZLondonStart  = 2;               // London KZ Start Hour
input int            InpKZLondonEnd    = 5;               // London KZ End Hour
input int            InpKZNYStart      = 8;               // New York KZ Start Hour
input int            InpKZNYEnd        = 13;              // New York KZ End Hour
input int            InpKZLCStart      = 15;              // London Close KZ Start Hour
input int            InpKZLCEnd        = 17;              // London Close KZ End Hour

input group "============ RISK MANAGEMENT ============"
input double         InpRiskPercent    = 1.0;             // Risk % per Trade
input double         InpMaxLot         = 10.0;            // Maximum Lot Size
input int            InpMaxPositions   = 1;               // Max Simultaneous Positions
input int            InpMaxTradesDay   = 5;               // Max Trades per Day
input int            InpSlippage       = 30;              // Max Slippage (points)

input group "============ BREAK EVEN ============"
input bool           InpUseBE          = false;           // Use Break Even
input int            InpBETrigger      = 500;             // BE Trigger (points profit)
input int            InpBEStep         = 50;              // BE Step (points above entry)

input group "============ TRAILING STOP ============"
input bool           InpUseTS          = false;           // Use Trailing Stop
input int            InpTSTrigger      = 500;             // TS Trigger (points, >= 2x Step)
input int            InpTSStep         = 200;             // TS Step (points distance from price)

input group "============ TIME FILTER ============"
input bool           InpUseTimeFilter  = false;           // Use Time Filter
input string         InpTimeStart      = "02:00";         // Start Time (HH:MM)
input string         InpTimeEnd        = "22:00";         // End Time (HH:MM)

input group "============ DAY FILTER ============"
input bool           InpSunday         = false;           // Trade Sunday
input bool           InpMonday         = true;            // Trade Monday
input bool           InpTuesday        = true;            // Trade Tuesday
input bool           InpWednesday      = true;            // Trade Wednesday
input bool           InpThursday       = true;            // Trade Thursday
input bool           InpFriday         = true;            // Trade Friday
input bool           InpSaturday       = false;           // Trade Saturday

input group "============ VISUAL ============"
input bool           InpDrawOB         = true;            // Draw Order Blocks
input bool           InpDrawFVG        = true;            // Draw FVG Zones
input bool           InpDrawSwings     = true;            // Draw Swing Points
input bool           InpDrawStructure  = true;            // Draw BOS/CHoCH Lines
input bool           InpShowPanel      = true;            // Show Info Panel

//+------------------------------------------------------------------+
//| GLOBAL VARIABLES                                                  |
//+------------------------------------------------------------------+
CTrade         g_trade;
CPositionInfo  g_posInfo;
CSymbolInfo    g_symInfo;

double g_tickSize;
double g_tickValue;
double g_point;
int    g_digits;
double g_lotStep;
double g_lotMin;
double g_lotMax;
ENUM_MARKET_TYPE g_marketType;

int    g_tradesToday;
datetime g_lastTradeDate;
datetime g_lastBarTime;
// Performance tracking
int    g_totalTrades;
double g_totalProfit;
double g_totalLoss;
double g_maxDrawdown;
double g_peakBalance;

// Pipeline stats (CLAUDE.md Section 5.3 — observability)
int    g_statSignals;          // total signals generated by DLL
int    g_statBlocked;          // signals blocked by filters
int    g_statBlockMaxPos;      // blocked by max positions
int    g_statBlockDayFilter;   // blocked by day filter
int    g_statBlockTimeFilter;  // blocked by time filter
int    g_statBlockMaxTrades;   // blocked by max trades/day
int    g_statTradesExecuted;   // successfully opened
int    g_statTP;               // closed by take profit
int    g_statSL;               // closed by stop loss
// Per-strategy counters
int    g_statBOS, g_statOB, g_statCH, g_statFVG, g_statLIQ;

//+------------------------------------------------------------------+
//| Expert initialization function                                    |
//+------------------------------------------------------------------+
int OnInit()
{
   // --- Symbol info ---
   if(!g_symInfo.Name(_Symbol))
   {
      Print("ERROR: Cannot get symbol info for ", _Symbol);
      return INIT_FAILED;
   }

   g_tickSize  = g_symInfo.TickSize();
   g_tickValue = g_symInfo.TickValue();
   g_point     = g_symInfo.Point();
   g_digits    = g_symInfo.Digits();
   g_lotStep   = g_symInfo.LotsStep();
   g_lotMin    = g_symInfo.LotsMin();
   g_lotMax    = g_symInfo.LotsMax();

   if(g_tickSize == 0 || g_tickValue == 0)
   {
      Print("ERROR: TickSize or TickValue is zero");
      return INIT_FAILED;
   }

   g_marketType = DetectMarketType();

   // --- Trade object ---
   g_trade.SetExpertMagicNumber(InpMagic);
   g_trade.SetDeviationInPoints(InpSlippage);
   g_trade.SetTypeFilling(DetectFillingMode());

   // --- Initialize DLL ---
   SMC_Init(InpSwingLength, InpOBLookback, InpLiqRangePct,
            InpCloseBreak ? 1 : 0, InpCloseMitigation ? 1 : 0, InpJoinFVG ? 1 : 0,
            InpBiasSwingLength);

   // --- Configure global signal parameters ---
   double minSLDist = InpMinSLPoints * g_tickSize;
   double maxSLDist = (InpMaxSLPoints > 0) ? InpMaxSLPoints * g_tickSize : 0.0;
   double minTPDist = InpMinTPPoints * g_tickSize;
   double maxTPDist = (InpMaxTPPoints > 0) ? InpMaxTPPoints * g_tickSize : 0.0;

   SMC_ConfigureGlobal(
      InpSLZoneBuffer, InpSweepLookback,
      InpOTEFibLow, InpOTEFibHigh,
      InpKZAsianStart, InpKZAsianEnd, InpKZLondonStart, InpKZLondonEnd,
      InpKZNYStart, InpKZNYEnd, InpKZLCStart, InpKZLCEnd,
      minSLDist, maxSLDist, minTPDist, maxTPDist, InpMinRR, InpMaxTPLookback);

   // --- Configure each strategy individually ---
   // OB Bounce (id=1)
   SMC_ConfigureStrategy(1, InpOB_Enable?1:0, InpOB_Lookback, InpOB_MinScore,
      InpOB_MaxVisits, InpOB_ReqOTE?1:0, InpOB_ReqHP?1:0, InpOB_ReqKZ?1:0,
      0.0, (InpOB_MaxSL>0)?InpOB_MaxSL*g_tickSize:0.0);

   // BOS Continuation (id=2)
   SMC_ConfigureStrategy(2, InpBOS_Enable?1:0, InpBOS_Lookback, InpBOS_MinScore,
      InpBOS_MaxVisits, InpBOS_ReqOTE?1:0, InpBOS_ReqHP?1:0, InpBOS_ReqKZ?1:0,
      0.0, (InpBOS_MaxSL>0)?InpBOS_MaxSL*g_tickSize:0.0);

   // CHoCH Reversal (id=3)
   SMC_ConfigureStrategy(3, InpCH_Enable?1:0, InpCH_Lookback, InpCH_MinScore,
      InpCH_MaxVisits, InpCH_ReqOTE?1:0, InpCH_ReqHP?1:0, InpCH_ReqKZ?1:0,
      0.0, (InpCH_MaxSL>0)?InpCH_MaxSL*g_tickSize:0.0);

   // FVG Fill (id=4)
   SMC_ConfigureStrategy(4, InpFVG_Enable?1:0, InpFVG_Lookback, InpFVG_MinScore,
      InpFVG_MaxVisits, InpFVG_ReqOTE?1:0, InpFVG_ReqHP?1:0, InpFVG_ReqKZ?1:0,
      0.0, (InpFVG_MaxSL>0)?InpFVG_MaxSL*g_tickSize:0.0);

   // Liquidity Sweep (id=5)
   SMC_ConfigureStrategy(5, InpLIQ_Enable?1:0, InpLIQ_Lookback, InpLIQ_MinScore,
      InpLIQ_MaxVisits, InpLIQ_ReqOTE?1:0, InpLIQ_ReqHP?1:0, InpLIQ_ReqKZ?1:0,
      0.0, (InpLIQ_MaxSL>0)?InpLIQ_MaxSL*g_tickSize:0.0);

   // Validate BE/TS proportions
   if(InpUseBE && InpBETrigger <= InpBEStep)
   {
      Print("WARNING: BE Trigger (", InpBETrigger, ") must be > BE Step (", InpBEStep, "). BE disabled.");
   }
   if(InpUseTS && InpTSTrigger < 2 * InpTSStep)
   {
      Print("WARNING: TS Trigger (", InpTSTrigger, ") should be >= 2x TS Step (", InpTSStep, "). Adjusting.");
   }

   // --- Feed historical bars ---
   FeedHistoricalBars();

   // --- Init tracking ---
   g_tradesToday = 0;
   g_lastTradeDate = 0;
   g_lastBarTime = 0;
   g_totalTrades = 0;
   g_totalProfit = 0;
   g_totalLoss = 0;
   g_maxDrawdown = 0;
   g_peakBalance = AccountInfoDouble(ACCOUNT_BALANCE);

   // Pipeline stats
   g_statSignals = g_statBlocked = 0;
   g_statBlockMaxPos = g_statBlockDayFilter = g_statBlockTimeFilter = g_statBlockMaxTrades = 0;
   g_statTradesExecuted = g_statTP = g_statSL = 0;
   g_statBOS = g_statOB = g_statCH = g_statFVG = g_statLIQ = 0;

   Print("==============================================");
   Print("  SMC EA v1.0 — Initialized");
   Print("  Symbol: ", _Symbol, " | Market: ", EnumToString(g_marketType));
   Print("  Magic: ", InpMagic);
   Print("  Strategies: ",
         (InpOB_Enable ? "OB " : ""), (InpBOS_Enable ? "BOS " : ""),
         (InpCH_Enable ? "CHoCH " : ""), (InpFVG_Enable ? "FVG " : ""),
         (InpLIQ_Enable ? "LIQ " : ""));
   Print("  Risk: ", InpRiskPercent, "%");
   Print("  Bars fed: ", SMC_GetBarCount());
   Print("  Swings: ", SMC_GetSwingCount(), " | OBs: ", SMC_GetOBCount(),
         " | FVGs: ", SMC_GetFVGCount(), " | Liq: ", SMC_GetLiqPoolCount());
   Print("  Trend: ", SMC_GetCurrentTrend() == 1 ? "BULLISH" :
                       SMC_GetCurrentTrend() == -1 ? "BEARISH" : "NONE");
   Print("==============================================");

   return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization                                           |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   SMC_Deinit();

   // Clean chart objects
   if(InpDrawOB || InpDrawFVG || InpDrawSwings || InpDrawStructure)
      ObjectsDeleteAll(0, "SMC_");

   if(InpShowPanel)
      ObjectsDeleteAll(0, "PANEL_");

   // Pipeline stats summary (CLAUDE.md Section 5.3)
   double wr = (g_statTradesExecuted > 0) ? (double)g_statTP / g_statTradesExecuted * 100.0 : 0;
   Print("==============================================");
   Print("  SMC EA — PIPELINE STATS");
   Print("  Signals: ", g_statSignals, " | Blocked: ", g_statBlocked,
         " | Executed: ", g_statTradesExecuted);
   Print("  BLOCKS: MaxPos=", g_statBlockMaxPos, " DayFilter=", g_statBlockDayFilter,
         " TimeFilter=", g_statBlockTimeFilter, " MaxTrades=", g_statBlockMaxTrades);
   Print("  STRATEGY: BOS=", g_statBOS, " OB=", g_statOB,
         " CHoCH=", g_statCH, " FVG=", g_statFVG, " LIQ=", g_statLIQ);
   Print("  EXITS: TP=", g_statTP, " SL=", g_statSL,
         " WR=", DoubleToString(wr, 1), "%");
   Print("  MaxDD=", DoubleToString(g_maxDrawdown, 2), "%");
   Print("==============================================");

   Print("SMC EA deinitialized. Reason: ", reason);
}

//+------------------------------------------------------------------+
//| Trade transaction handler — track TP/SL exits                     |
//+------------------------------------------------------------------+
void OnTradeTransaction(const MqlTradeTransaction &trans,
                        const MqlTradeRequest &request,
                        const MqlTradeResult &result)
{
   if(trans.type != TRADE_TRANSACTION_DEAL_ADD) return;

   ulong dealTicket = trans.deal;
   if(dealTicket == 0) return;

   if(HistoryDealSelect(dealTicket))
   {
      long dealEntry = HistoryDealGetInteger(dealTicket, DEAL_ENTRY);
      long dealMagic = HistoryDealGetInteger(dealTicket, DEAL_MAGIC);

      if(dealEntry == DEAL_ENTRY_OUT && dealMagic == InpMagic)
      {
         long dealReason = HistoryDealGetInteger(dealTicket, DEAL_REASON);
         if(dealReason == DEAL_REASON_TP) g_statTP++;
         else if(dealReason == DEAL_REASON_SL) g_statSL++;
      }
   }
}

//+------------------------------------------------------------------+
//| Expert tick function                                              |
//+------------------------------------------------------------------+
void OnTick()
{
   // --- New bar check ---
   datetime currentBarTime = iTime(_Symbol, PERIOD_CURRENT, 0);
   bool isNewBar = (currentBarTime != g_lastBarTime);

   if(isNewBar)
   {
      g_lastBarTime = currentBarTime;

      // Feed the COMPLETED bar (bar[1]) to DLL
      double o = iOpen(_Symbol, PERIOD_CURRENT, 1);
      double h = iHigh(_Symbol, PERIOD_CURRENT, 1);
      double l = iLow(_Symbol, PERIOD_CURRENT, 1);
      double c = iClose(_Symbol, PERIOD_CURRENT, 1);
      long   v = iVolume(_Symbol, PERIOD_CURRENT, 1);
      long   t = (long)iTime(_Symbol, PERIOD_CURRENT, 1);

      SMC_FeedBar(o, h, l, c, v, t);
      SMC_Process();

      // --- Check for signal ---
      int dir = SMC_GetSignalDirection();
      if(dir != 0)
      {
         double entry = SMC_GetSignalEntry();
         double sl    = SMC_GetSignalSL();
         double tp    = SMC_GetSignalTP();
         double conf  = SMC_GetSignalConfidence();
         int    strat = SMC_GetSignalStrategy();

         g_statSignals++;

         // Diagnostic log
         Print("SIGNAL: ", (dir == 1 ? "BUY" : "SELL"),
               " | Strat: ", StrategyName(strat),
               " | Score: ", DoubleToString(conf, 2),
               " | Entry: ", DoubleToString(entry, g_digits),
               " | SL: ", DoubleToString(sl, g_digits),
               " (", DoubleToString(MathAbs(entry-sl)/g_point, 0), " pts)",
               " | TP: ", DoubleToString(tp, g_digits),
               " (", DoubleToString(MathAbs(tp-entry)/g_point, 0), " pts)");

         if(CanOpenTrade())
            ExecuteSignal(dir, entry, sl, tp, conf, strat);
         else
         {
            g_statBlocked++;
            if(!IsDayAllowed())
            {  g_statBlockDayFilter++; Print("SIGNAL BLOCKED: Day filter"); }
            else if(InpUseTimeFilter && !IsTimeAllowed())
            {  g_statBlockTimeFilter++; Print("SIGNAL BLOCKED: Time filter"); }
            else if(CountPositions() >= InpMaxPositions)
            {  g_statBlockMaxPos++; Print("SIGNAL BLOCKED: Max positions"); }
            else if(g_tradesToday >= InpMaxTradesDay)
            {  g_statBlockMaxTrades++; Print("SIGNAL BLOCKED: Max trades/day"); }
            else
               Print("SIGNAL BLOCKED: Unknown");
         }
      }

      // --- Draw visual objects ---
      if(!MQLInfoInteger(MQL_TESTER) || MQLInfoInteger(MQL_VISUAL_MODE))
      {
         if(InpDrawOB || InpDrawFVG || InpDrawSwings || InpDrawStructure)
            DrawSMCObjects();
         if(InpShowPanel)
            DrawPanel();
      }
   }

   // --- Position management (every tick) ---
   ManagePositions();

   // --- Track drawdown ---
   TrackPerformance();
}

//+------------------------------------------------------------------+
//| Feed historical bars to DLL on init                               |
//+------------------------------------------------------------------+
void FeedHistoricalBars()
{
   int barsToFeed = MathMin(Bars(_Symbol, PERIOD_CURRENT), InpHistoryBars);

   // Feed from oldest to newest (bar[n-1] to bar[1])
   for(int i = barsToFeed - 1; i >= 1; i--)
   {
      double o = iOpen(_Symbol, PERIOD_CURRENT, i);
      double h = iHigh(_Symbol, PERIOD_CURRENT, i);
      double l = iLow(_Symbol, PERIOD_CURRENT, i);
      double c = iClose(_Symbol, PERIOD_CURRENT, i);
      long   v = iVolume(_Symbol, PERIOD_CURRENT, i);
      long   t = (long)iTime(_Symbol, PERIOD_CURRENT, i);

      SMC_FeedBar(o, h, l, c, v, t);
   }

   SMC_Process();
}

//+------------------------------------------------------------------+
//| Check if we can open a new trade                                  |
//+------------------------------------------------------------------+
bool CanOpenTrade()
{
   // Day filter
   if(!IsDayAllowed()) return false;

   // Time filter
   if(InpUseTimeFilter && !IsTimeAllowed()) return false;

   // Max positions
   if(CountPositions() >= InpMaxPositions) return false;

   // Max trades per day
   ResetDailyCounter();
   if(g_tradesToday >= InpMaxTradesDay) return false;

   return true;
}

//+------------------------------------------------------------------+
//| Execute trading signal from DLL                                   |
//+------------------------------------------------------------------+
void ExecuteSignal(int direction, double entry, double sl, double tp,
                   double confidence, int strategy)
{
   g_symInfo.RefreshRates();

   ENUM_ORDER_TYPE orderType;
   double price;

   if(direction == 1)
   {
      orderType = ORDER_TYPE_BUY;
      price = g_symInfo.Ask();
   }
   else
   {
      orderType = ORDER_TYPE_SELL;
      price = g_symInfo.Bid();
   }

   // --- Calculate lot size ---
   double slDist = MathAbs(price - sl);
   double lots = CalculateLotSize(slDist);

   // --- Normalize SL/TP ---
   sl = NormalizeDouble(sl, g_digits);
   tp = NormalizeDouble(tp, g_digits);

   // --- Validate stops against broker minimum ---
   int stopsLevel = (int)SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL);
   double minDist = stopsLevel * g_point;
   int spread = (int)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);
   double spreadDist = spread * g_point;

   // Add spread to minimum distance for safety
   double safeMinDist = minDist + spreadDist;

   if(direction == 1)
   {
      if(price - sl < safeMinDist || tp - price < safeMinDist)
      {
         Print("REJECT: Stops too close to price. SL_dist=", DoubleToString((price-sl)/g_point, 0),
               " TP_dist=", DoubleToString((tp-price)/g_point, 0),
               " MinDist=", DoubleToString(safeMinDist/g_point, 0), " pts");
         return;
      }
   }
   else
   {
      if(sl - price < safeMinDist || price - tp < safeMinDist)
      {
         Print("REJECT: Stops too close to price. SL_dist=", DoubleToString((sl-price)/g_point, 0),
               " TP_dist=", DoubleToString((price-tp)/g_point, 0),
               " MinDist=", DoubleToString(safeMinDist/g_point, 0), " pts");
         return;
      }
   }

   sl = NormalizeDouble(sl, g_digits);
   tp = NormalizeDouble(tp, g_digits);

   // --- Build comment ---
   string stratName = StrategyName(strategy);
   string comment = InpComment + "_" + stratName + "_" + DoubleToString(confidence, 2);

   // --- Execute ---
   if(g_trade.PositionOpen(_Symbol, orderType, lots, price, sl, tp, comment))
   {
      g_tradesToday++;
      g_totalTrades++;
      g_statTradesExecuted++;
      // Per-strategy counter
      if(strategy == 1) g_statOB++;
      else if(strategy == 2) g_statBOS++;
      else if(strategy == 3) g_statCH++;
      else if(strategy == 4) g_statFVG++;
      else if(strategy == 5) g_statLIQ++;
      Print("TRADE: ", (direction == 1 ? "BUY" : "SELL"),
            " | Strategy: ", stratName,
            " | Score: ", DoubleToString(confidence, 2),
            " | Lot: ", lots,
            " | Entry: ", price,
            " | SL: ", sl, " | TP: ", tp);
   }
   else
   {
      Print("ERROR: ", g_trade.ResultRetcode(), " - ", g_trade.ResultRetcodeDescription());
   }
}

//+------------------------------------------------------------------+
//| Calculate lot size — universal tick_value method                   |
//+------------------------------------------------------------------+
double CalculateLotSize(double slDistance)
{
   double balance = AccountInfoDouble(ACCOUNT_BALANCE);
   double riskAmount = balance * InpRiskPercent / 100.0;

   // Universal formula: lots = risk / ((sl_dist / tick_size) * tick_value)
   double slTicks = slDistance / g_tickSize;
   double lots = riskAmount / (slTicks * g_tickValue);

   // B3 special handling
   if(g_marketType == MARKET_B3_INDICE)
   {
      lots = MathRound(lots);
      lots = MathMax(lots, 1.0);
   }
   else if(g_marketType == MARKET_B3_DOLAR)
   {
      lots = MathRound(lots);
      lots = MathMax(lots, 1.0);
   }

   // Normalize to lot step
   lots = MathFloor(lots / g_lotStep) * g_lotStep;
   lots = MathMax(lots, g_lotMin);
   lots = MathMin(lots, g_lotMax);
   lots = MathMin(lots, InpMaxLot);

   return NormalizeDouble(lots, 2);
}

//+------------------------------------------------------------------+
//| Manage open positions — Break Even + Trailing Stop                |
//+------------------------------------------------------------------+
void ManagePositions()
{
   // Universal conversion: points * tick_size = price distance
   double beTriggerDist = InpBETrigger * g_tickSize;
   double beStepDist    = InpBEStep * g_tickSize;
   double tsTriggerDist = InpTSTrigger * g_tickSize;
   double tsStepDist    = InpTSStep * g_tickSize;

   // Validate proportions at runtime
   bool beValid = InpUseBE && (InpBETrigger > InpBEStep);
   bool tsValid = InpUseTS && (InpTSTrigger >= 2 * InpTSStep);

   for(int i = PositionsTotal() - 1; i >= 0; i--)
   {
      if(!g_posInfo.SelectByIndex(i)) continue;
      if(g_posInfo.Symbol() != _Symbol) continue;
      if(g_posInfo.Magic() != InpMagic) continue;

      ulong ticket = g_posInfo.Ticket();
      double openPrice = g_posInfo.PriceOpen();
      double currentSL = g_posInfo.StopLoss();
      double currentTP = g_posInfo.TakeProfit();
      double currentPrice = g_posInfo.PriceCurrent();
      bool isBuy = (g_posInfo.PositionType() == POSITION_TYPE_BUY);

      // Profit in price units (not points)
      double profitDist = isBuy ? (currentPrice - openPrice) : (openPrice - currentPrice);

      // --- Break Even (executes ONCE per position) ---
      if(beValid)
      {
         bool beAlreadyDone = false;
         if(isBuy)
            beAlreadyDone = (currentSL >= openPrice);
         else
            beAlreadyDone = (currentSL > 0 && currentSL <= openPrice);

         if(!beAlreadyDone && profitDist >= beTriggerDist)
         {
            double newSL;
            if(isBuy)
               newSL = openPrice + beStepDist;
            else
               newSL = openPrice - beStepDist;

            newSL = NormalizeDouble(newSL, g_digits);

            if(g_trade.PositionModify(ticket, newSL, currentTP))
               Print("BE: ticket #", ticket, " SL moved to ", newSL,
                     " (entry ", openPrice, " + ", DoubleToString(beStepDist, g_digits), ")");
         }
      }

      // --- Trailing Stop (continuous, with throttling 75%) ---
      if(tsValid && profitDist >= tsTriggerDist)
      {
         double newSL;
         if(isBuy)
         {
            newSL = currentPrice - tsStepDist;
            newSL = NormalizeDouble(newSL, g_digits);

            // Throttling: only update if price moved >= 75% of step since last SL
            double slImprovement = newSL - currentSL;
            if(slImprovement >= tsStepDist * 0.75)
            {
               if(g_trade.PositionModify(ticket, newSL, currentTP))
                  Print("TS: ticket #", ticket, " SL trailed to ", newSL);
            }
         }
         else
         {
            newSL = currentPrice + tsStepDist;
            newSL = NormalizeDouble(newSL, g_digits);

            double slImprovement = currentSL - newSL;
            if(currentSL == 0 || slImprovement >= tsStepDist * 0.75)
            {
               if(g_trade.PositionModify(ticket, newSL, currentTP))
                  Print("TS: ticket #", ticket, " SL trailed to ", newSL);
            }
         }
      }
   }
}

//+------------------------------------------------------------------+
//| OnTester — custom metric for optimization                         |
//+------------------------------------------------------------------+
double OnTester()
{
   double profit = TesterStatistics(STAT_PROFIT);
   double trades = TesterStatistics(STAT_TRADES);
   double pf     = TesterStatistics(STAT_PROFIT_FACTOR);
   double dd     = TesterStatistics(STAT_EQUITY_DDREL_PERCENT);

   // Custom metric: Profit Factor * sqrt(trades) * (1 - DD/100)
   // Rewards: high PF, many trades, low drawdown
   if(trades < 10 || pf <= 0) return 0;

   double metric = pf * MathSqrt(trades) * (1.0 - dd / 100.0);
   return metric;
}

//+------------------------------------------------------------------+
//| HELPER FUNCTIONS                                                  |
//+------------------------------------------------------------------+

// --- Detect market type ---
ENUM_MARKET_TYPE DetectMarketType()
{
   if(InpMarketType != MARKET_AUTO) return InpMarketType;
   string sym = _Symbol;
   if(StringFind(sym, "WIN") >= 0 || StringFind(sym, "IND") >= 0) return MARKET_B3_INDICE;
   if(StringFind(sym, "WDO") >= 0 || StringFind(sym, "DOL") >= 0) return MARKET_B3_DOLAR;
   return MARKET_FOREX;
}

// --- Detect filling mode ---
ENUM_ORDER_TYPE_FILLING DetectFillingMode()
{
   long fillMode = SymbolInfoInteger(_Symbol, SYMBOL_FILLING_MODE);
   if((fillMode & SYMBOL_FILLING_FOK) != 0) return ORDER_FILLING_FOK;
   if((fillMode & SYMBOL_FILLING_IOC) != 0) return ORDER_FILLING_IOC;
   return ORDER_FILLING_RETURN;
}

// --- Count our positions ---
int CountPositions()
{
   int count = 0;
   for(int i = PositionsTotal() - 1; i >= 0; i--)
   {
      if(g_posInfo.SelectByIndex(i))
         if(g_posInfo.Symbol() == _Symbol && g_posInfo.Magic() == InpMagic)
            count++;
   }
   return count;
}

// --- Daily counter ---
void ResetDailyCounter()
{
   MqlDateTime dt;
   TimeToStruct(TimeCurrent(), dt);
   datetime today = StringToTime(IntegerToString(dt.year) + "." +
                                  IntegerToString(dt.mon) + "." +
                                  IntegerToString(dt.day));
   if(today != g_lastTradeDate)
   {
      g_tradesToday = 0;
      g_lastTradeDate = today;
   }
}

// --- Time filter ---
bool IsTimeAllowed()
{
   if(!InpUseTimeFilter) return true;
   MqlDateTime dt;
   TimeToStruct(TimeCurrent(), dt);
   int mins = dt.hour * 60 + dt.min;
   int startMins = TimeStringToMinutes(InpTimeStart);
   int endMins   = TimeStringToMinutes(InpTimeEnd);
   if(startMins <= endMins)
      return (mins >= startMins && mins <= endMins);
   else  // overnight wrap-around (e.g. 22:00-06:00)
      return (mins >= startMins || mins <= endMins);
}

int TimeStringToMinutes(string timeStr)
{
   string parts[];
   int count = StringSplit(timeStr, ':', parts);
   if(count >= 2)
      return (int)StringToInteger(parts[0]) * 60 + (int)StringToInteger(parts[1]);
   return 0;
}

// --- Day filter ---
bool IsDayAllowed()
{
   MqlDateTime dt;
   TimeToStruct(TimeCurrent(), dt);
   switch(dt.day_of_week)
   {
      case 0: return InpSunday;
      case 1: return InpMonday;
      case 2: return InpTuesday;
      case 3: return InpWednesday;
      case 4: return InpThursday;
      case 5: return InpFriday;
      case 6: return InpSaturday;
   }
   return false;
}

// --- Strategy name ---
string StrategyName(int strategy)
{
   switch(strategy)
   {
      case 1: return "OB_Bounce";
      case 2: return "BOS_Cont";
      case 3: return "CHoCH_Rev";
      case 4: return "FVG_Fill";
      case 5: return "Liq_Sweep";
   }
   return "Unknown";
}

// --- Track performance for drawdown ---
void TrackPerformance()
{
   double balance = AccountInfoDouble(ACCOUNT_BALANCE);
   if(balance > g_peakBalance) g_peakBalance = balance;
   double dd = (g_peakBalance - balance) / g_peakBalance * 100.0;
   if(dd > g_maxDrawdown) g_maxDrawdown = dd;
}

//+------------------------------------------------------------------+
//| VISUAL — Draw SMC objects on chart                                |
//+------------------------------------------------------------------+
void DrawSMCObjects()
{
   ObjectsDeleteAll(0, "SMC_");

   int totalBars = Bars(_Symbol, PERIOD_CURRENT);
   int barsFed = SMC_GetBarCount();
   // Only draw objects from the last N bars visible on chart
   int maxDrawBars = 200;

   // --- Draw Swing Points (last 30, arrows at the swing bar) ---
   if(InpDrawSwings)
   {
      int swingCount = SMC_GetSwingCount();
      int drawn = 0;
      for(int i = swingCount - 1; i >= 0 && drawn < 30; i--)
      {
         int barIdx = SMC_GetSwingBarIndex(i);
         int barsAgo = barsFed - 1 - barIdx;
         if(barsAgo < 0 || barsAgo >= totalBars || barsAgo > maxDrawBars) continue;

         int swType = SMC_GetSwingType(i);
         double level = SMC_GetSwingLevel(i);
         datetime t = iTime(_Symbol, PERIOD_CURRENT, barsAgo);

         string name = "SMC_SW_" + IntegerToString(i);
         ObjectCreate(0, name, OBJ_ARROW, 0, t, level);
         ObjectSetInteger(0, name, OBJPROP_ARROWCODE, (swType == 1) ? 217 : 218);
         ObjectSetInteger(0, name, OBJPROP_COLOR, (swType == 1) ? clrDodgerBlue : clrOrangeRed);
         ObjectSetInteger(0, name, OBJPROP_WIDTH, 1);
         drawn++;
      }
   }

   // --- Draw Order Blocks (max 8 active, rectangle from OB bar to +20 bars) ---
   if(InpDrawOB)
   {
      int obCount = SMC_GetOBCount();
      int drawn = 0;
      for(int i = obCount - 1; i >= 0 && drawn < 8; i--)
      {
         if(SMC_GetOBMitigated(i)) continue;
         if(SMC_GetOBDirection(i) == 0) continue;

         int barIdx = SMC_GetOBBarIndex(i);
         int barsAgo = barsFed - 1 - barIdx;
         if(barsAgo < 0 || barsAgo >= totalBars || barsAgo > maxDrawBars) continue;

         double top = SMC_GetOBTop(i);
         double bottom = SMC_GetOBBottom(i);
         int dir = SMC_GetOBDirection(i);

         datetime t1 = iTime(_Symbol, PERIOD_CURRENT, barsAgo);
         // Extend rectangle 20 bars forward from OB, capped at current bar
         int endBarsAgo = MathMax(0, barsAgo - 20);
         datetime t2 = iTime(_Symbol, PERIOD_CURRENT, endBarsAgo);

         string name = "SMC_OB_" + IntegerToString(i);
         ObjectCreate(0, name, OBJ_RECTANGLE, 0, t1, top, t2, bottom);
         ObjectSetInteger(0, name, OBJPROP_COLOR, (dir == 1) ? C'0,100,0' : C'139,0,0');
         ObjectSetInteger(0, name, OBJPROP_FILL, true);
         ObjectSetInteger(0, name, OBJPROP_BACK, true);
         ObjectSetInteger(0, name, OBJPROP_WIDTH, 1);

         // Label at start of OB
         string lblName = name + "_lbl";
         string lblText = (dir == 1) ? "OB+" : "OB-";
         ObjectCreate(0, lblName, OBJ_TEXT, 0, t1, (dir == 1) ? bottom : top);
         ObjectSetString(0, lblName, OBJPROP_TEXT, lblText);
         ObjectSetInteger(0, lblName, OBJPROP_COLOR, (dir == 1) ? clrLime : clrRed);
         ObjectSetInteger(0, lblName, OBJPROP_FONTSIZE, 7);
         drawn++;
      }
   }

   // --- Draw FVG zones (max 8 active, thin rectangles, limited width) ---
   if(InpDrawFVG)
   {
      int fvgCount = SMC_GetFVGCount();
      int drawn = 0;
      for(int i = fvgCount - 1; i >= 0 && drawn < 8; i--)
      {
         if(SMC_GetFVGMitigated(i)) continue;

         int barIdx = SMC_GetFVGBarIndex(i);
         int barsAgo = barsFed - 1 - barIdx;
         if(barsAgo < 0 || barsAgo >= totalBars || barsAgo > maxDrawBars) continue;

         double top = SMC_GetFVGTop(i);
         double bottom = SMC_GetFVGBottom(i);
         int dir = SMC_GetFVGDirection(i);

         datetime t1 = iTime(_Symbol, PERIOD_CURRENT, barsAgo);
         int endBarsAgo = MathMax(0, barsAgo - 15);
         datetime t2 = iTime(_Symbol, PERIOD_CURRENT, endBarsAgo);

         string name = "SMC_FVG_" + IntegerToString(i);
         ObjectCreate(0, name, OBJ_RECTANGLE, 0, t1, top, t2, bottom);
         ObjectSetInteger(0, name, OBJPROP_COLOR, (dir == 1) ? C'0,128,128' : C'200,100,0');
         ObjectSetInteger(0, name, OBJPROP_FILL, true);
         ObjectSetInteger(0, name, OBJPROP_BACK, true);
         ObjectSetInteger(0, name, OBJPROP_WIDTH, 1);

         // Midpoint line (Consequent Encroachment)
         double mid = SMC_GetFVGMidpoint(i);
         if(mid > 0)
         {
            string midName = name + "_ce";
            ObjectCreate(0, midName, OBJ_TREND, 0, t1, mid, t2, mid);
            ObjectSetInteger(0, midName, OBJPROP_COLOR, clrWhite);
            ObjectSetInteger(0, midName, OBJPROP_STYLE, STYLE_DOT);
            ObjectSetInteger(0, midName, OBJPROP_WIDTH, 1);
            ObjectSetInteger(0, midName, OBJPROP_BACK, true);
            ObjectSetInteger(0, midName, OBJPROP_RAY_RIGHT, false);
         }
         drawn++;
      }
   }

   // --- Draw BOS/CHoCH (max 10, limited-length lines, label at start) ---
   if(InpDrawStructure)
   {
      int breakCount = SMC_GetStructureBreakCount();
      int drawn = 0;
      for(int i = breakCount - 1; i >= 0 && drawn < 10; i--)
      {
         int barIdx = SMC_GetStructureBarIndex(i);
         int barsAgo = barsFed - 1 - barIdx;
         if(barsAgo < 0 || barsAgo >= totalBars || barsAgo > maxDrawBars) continue;

         int sType = SMC_GetStructureType(i);
         int sDir  = SMC_GetStructureDirection(i);
         double level = SMC_GetStructureLevel(i);

         datetime t1 = iTime(_Symbol, PERIOD_CURRENT, barsAgo);
         int endBarsAgo = MathMax(0, barsAgo - 30);
         datetime t2 = iTime(_Symbol, PERIOD_CURRENT, endBarsAgo);

         string name = "SMC_STR_" + IntegerToString(i);
         color clr;
         ENUM_LINE_STYLE style;

         if(sType == 1) { clr = (sDir == 1) ? clrLimeGreen : clrCrimson; style = STYLE_SOLID; }
         else           { clr = (sDir == 1) ? clrGold : clrMagenta;      style = STYLE_DASH;  }

         ObjectCreate(0, name, OBJ_TREND, 0, t1, level, t2, level);
         ObjectSetInteger(0, name, OBJPROP_COLOR, clr);
         ObjectSetInteger(0, name, OBJPROP_STYLE, style);
         ObjectSetInteger(0, name, OBJPROP_WIDTH, 1);
         ObjectSetInteger(0, name, OBJPROP_RAY_RIGHT, false);

         string label = (sType == 1 ? "BOS" : "CHoCH");
         string labelName = name + "_lbl";
         ObjectCreate(0, labelName, OBJ_TEXT, 0, t1, level);
         ObjectSetString(0, labelName, OBJPROP_TEXT, label);
         ObjectSetInteger(0, labelName, OBJPROP_COLOR, clr);
         ObjectSetInteger(0, labelName, OBJPROP_FONTSIZE, 7);
         drawn++;
      }
   }

   // --- Draw Liquidity Pools (dotted lines with touch count) ---
   {
      int liqCount = SMC_GetLiqPoolCount();
      int drawn = 0;
      for(int i = liqCount - 1; i >= 0 && drawn < 6; i--)
      {
         if(SMC_GetLiqPoolSwept(i)) continue;

         double level = SMC_GetLiqPoolLevel(i);
         int lDir = SMC_GetLiqPoolDirection(i);
         int touches = SMC_GetLiqPoolTouches(i);

         // Draw at current price area
         datetime t1 = iTime(_Symbol, PERIOD_CURRENT, MathMin(50, totalBars - 1));
         datetime t2 = iTime(_Symbol, PERIOD_CURRENT, 0);

         string name = "SMC_LIQ_" + IntegerToString(i);
         color clr = (lDir == 1) ? clrDeepSkyBlue : clrOrange;

         ObjectCreate(0, name, OBJ_TREND, 0, t1, level, t2, level);
         ObjectSetInteger(0, name, OBJPROP_COLOR, clr);
         ObjectSetInteger(0, name, OBJPROP_STYLE, STYLE_DOT);
         ObjectSetInteger(0, name, OBJPROP_WIDTH, 1);
         ObjectSetInteger(0, name, OBJPROP_RAY_RIGHT, false);

         // Label with direction and touch count
         string lblName = name + "_lbl";
         string lblText = (lDir == 1 ? "BSL" : "SSL") + "(" + IntegerToString(touches) + ")";
         ObjectCreate(0, lblName, OBJ_TEXT, 0, t2, level);
         ObjectSetString(0, lblName, OBJPROP_TEXT, lblText);
         ObjectSetInteger(0, lblName, OBJPROP_COLOR, clr);
         ObjectSetInteger(0, lblName, OBJPROP_FONTSIZE, 7);
         drawn++;
      }
   }

   // --- Draw OTE Zone (Fibonacci 0.62-0.79 of last swing range) ---
   {
      int trend = SMC_GetCurrentTrend();
      if(trend != 0)
      {
         // Find last swing high and low for OTE calculation
         int swCount = SMC_GetSwingCount();
         double lastHigh = 0, lastLow = 0;
         bool foundHigh = false, foundLow = false;

         for(int i = swCount - 1; i >= 0; i--)
         {
            if(!foundHigh && SMC_GetSwingType(i) == 1)
            {
               lastHigh = SMC_GetSwingLevel(i);
               foundHigh = true;
            }
            if(!foundLow && SMC_GetSwingType(i) == -1)
            {
               lastLow = SMC_GetSwingLevel(i);
               foundLow = true;
            }
            if(foundHigh && foundLow) break;
         }

         if(foundHigh && foundLow && lastHigh > lastLow)
         {
            double range = lastHigh - lastLow;
            double oteTop, oteBot;

            if(trend == 1) // BUY: OTE is in discount
            {
               oteBot = lastHigh - range * InpOTEFibHigh;
               oteTop = lastHigh - range * InpOTEFibLow;
            }
            else // SELL: OTE is in premium
            {
               oteBot = lastLow + range * InpOTEFibLow;
               oteTop = lastLow + range * InpOTEFibHigh;
            }

            datetime t1 = iTime(_Symbol, PERIOD_CURRENT, MathMin(30, totalBars - 1));
            datetime t2 = iTime(_Symbol, PERIOD_CURRENT, 0);

            string name = "SMC_OTE";
            ObjectCreate(0, name, OBJ_RECTANGLE, 0, t1, oteTop, t2, oteBot);
            ObjectSetInteger(0, name, OBJPROP_COLOR, C'75,0,130');
            ObjectSetInteger(0, name, OBJPROP_FILL, true);
            ObjectSetInteger(0, name, OBJPROP_BACK, true);

            // OTE label
            ObjectCreate(0, name + "_lbl", OBJ_TEXT, 0, t1, (oteTop + oteBot) / 2.0);
            ObjectSetString(0, name + "_lbl", OBJPROP_TEXT, "OTE");
            ObjectSetInteger(0, name + "_lbl", OBJPROP_COLOR, C'180,130,255');
            ObjectSetInteger(0, name + "_lbl", OBJPROP_FONTSIZE, 8);
         }
      }
   }
}

//+------------------------------------------------------------------+
//| Draw info panel                                                   |
//+------------------------------------------------------------------+
void DrawPanel()
{
   ObjectsDeleteAll(0, "PANEL_");

   int bias = SMC_GetBias();
   string biasStr = (bias == 1) ? "BULLISH" : (bias == -1) ? "BEARISH" : "NONE";
   color biaClr = (bias == 1) ? clrLimeGreen : (bias == -1) ? clrCrimson : clrGray;
   int trend = SMC_GetCurrentTrend();
   string trendStr = (trend == 1) ? "BULLISH" : (trend == -1) ? "BEARISH" : "NONE";
   color trendClr = (trend == 1) ? clrLimeGreen : (trend == -1) ? clrCrimson : clrGray;

   int sigDir = SMC_GetSignalDirection();
   string sigStr = (sigDir == 0) ? "---" :
                   StrategyName(SMC_GetSignalStrategy()) + " " +
                   (sigDir == 1 ? "BUY" : "SELL") + " [" +
                   DoubleToString(SMC_GetSignalConfidence(), 2) + "]";
   color sigClr = (sigDir == 1) ? clrLimeGreen : (sigDir == -1) ? clrCrimson : clrGray;

   int x = 10, y = 20, lineH = 16;
   int row = 0;

   // Background
   PanelRect("PANEL_BG", x - 5, y - 5, 220, lineH * 10 + 10, C'20,20,30');

   // Title
   PanelLabel("PANEL_T", x, y + lineH * row++, "SMC ENGINE v1.0", clrWhite, 9, true);
   PanelLabel("PANEL_S", x, y + lineH * row++, _Symbol + " " + StringSubstr(EnumToString((ENUM_TIMEFRAMES)Period()), 7), clrSilver, 8, false);

   // Separator
   row++;

   // Data rows
   PanelLabel("PANEL_TR", x, y + lineH * row, "Trend:", clrSilver, 8, false);
   PanelLabel("PANEL_TV", x + 100, y + lineH * row++, trendStr, trendClr, 8, true);

   PanelLabel("PANEL_SR", x, y + lineH * row, "Signal:", clrSilver, 8, false);
   PanelLabel("PANEL_SV", x + 100, y + lineH * row++, sigStr, sigClr, 8, false);

   PanelLabel("PANEL_OB", x, y + lineH * row, "OBs:", clrSilver, 8, false);
   PanelLabel("PANEL_OV", x + 100, y + lineH * row++,
              IntegerToString(SMC_GetActiveOBCount(0)) +
              " (B:" + IntegerToString(SMC_GetActiveOBCount(1)) +
              " S:" + IntegerToString(SMC_GetActiveOBCount(-1)) + ")",
              clrWhite, 8, false);

   PanelLabel("PANEL_FV", x, y + lineH * row, "FVGs:", clrSilver, 8, false);
   PanelLabel("PANEL_FN", x + 100, y + lineH * row++, IntegerToString(SMC_GetActiveFVGCount(0)), clrWhite, 8, false);

   PanelLabel("PANEL_LQ", x, y + lineH * row, "Liq Pools:", clrSilver, 8, false);
   PanelLabel("PANEL_LN", x + 100, y + lineH * row++, IntegerToString(SMC_GetActiveLiqPoolCount(0)), clrWhite, 8, false);

   PanelLabel("PANEL_TD", x, y + lineH * row, "Trades:", clrSilver, 8, false);
   PanelLabel("PANEL_TN", x + 100, y + lineH * row++,
              IntegerToString(g_tradesToday) + "/" + IntegerToString(InpMaxTradesDay),
              clrWhite, 8, false);

   PanelLabel("PANEL_DD", x, y + lineH * row, "Max DD:", clrSilver, 8, false);
   color ddClr = (g_maxDrawdown < 5) ? clrLimeGreen : (g_maxDrawdown < 15) ? clrYellow : clrRed;
   PanelLabel("PANEL_DN", x + 100, y + lineH * row++, DoubleToString(g_maxDrawdown, 1) + "%", ddClr, 8, false);
}

//+------------------------------------------------------------------+
//| Panel helper: create OBJ_LABEL                                    |
//+------------------------------------------------------------------+
void PanelLabel(string name, int x, int y, string text, color clr, int fontSize, bool bold)
{
   ObjectCreate(0, name, OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_LEFT_UPPER);
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetString(0, name, OBJPROP_TEXT, text);
   ObjectSetInteger(0, name, OBJPROP_COLOR, clr);
   ObjectSetInteger(0, name, OBJPROP_FONTSIZE, fontSize);
   ObjectSetString(0, name, OBJPROP_FONT, bold ? "Arial Bold" : "Arial");
   ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false);
   ObjectSetInteger(0, name, OBJPROP_HIDDEN, true);
}

//+------------------------------------------------------------------+
//| Panel helper: create background rectangle                         |
//+------------------------------------------------------------------+
void PanelRect(string name, int x, int y, int width, int height, color clr)
{
   ObjectCreate(0, name, OBJ_RECTANGLE_LABEL, 0, 0, 0);
   ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_LEFT_UPPER);
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, name, OBJPROP_XSIZE, width);
   ObjectSetInteger(0, name, OBJPROP_YSIZE, height);
   ObjectSetInteger(0, name, OBJPROP_BGCOLOR, clr);
   ObjectSetInteger(0, name, OBJPROP_BORDER_TYPE, BORDER_FLAT);
   ObjectSetInteger(0, name, OBJPROP_BORDER_COLOR, C'50,50,70');
   ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false);
   ObjectSetInteger(0, name, OBJPROP_HIDDEN, true);
}
//+------------------------------------------------------------------+
