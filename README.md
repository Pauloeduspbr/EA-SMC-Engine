# EA SMC Engine - Smart Money Concepts Expert Advisor

**Automated trading system for MetaTrader 5** based on ICT/Smart Money Concepts (SMC), implemented as a high-performance C++ DLL with an MQL5 bridge.

All strategy detection and signal generation runs in native C++ for maximum speed. The MQL5 EA acts as a thin bridge: feeds price data, executes trades, and draws visual objects on the chart.

---

## Architecture

```
MQL5 EA (Bridge)                    C++ DLL (Brain)
+------------------------+         +--------------------------------+
| OnInit()          -----|-------> | SMC_Init()                     |
| OnTick()               |         |                                |
|  FeedBar(OHLCV)   -----|-------> | SMC_FeedBar()                  |
|  Process()         -----|-------> | SMC_Process()                  |
|                         |         |  SwingDetector                 |
|                         |         |  StructureAnalyzer (BOS/CHoCH) |
|                         |         |  OrderBlockTracker             |
|                         |         |  FVGDetector                   |
|                         |         |  LiquidityMapper               |
|                         |         |  SignalGenerator (5 strategies)|
|  GetSignal()       <----|-------- | SMC_GetSignal*()               |
|  ExecuteTrade()         |         |                                |
|  ManagePositions()      |         |                                |
|  DrawChart()            |         |                                |
+------------------------+         +--------------------------------+
```

## Features

### SMC Detection Engine (C++ DLL)
- **Swing Highs/Lows** - Alternating H/L detection with deduplication
- **Break of Structure (BOS)** - Trend continuation confirmation
- **Change of Character (CHoCH)** - Trend reversal detection
- **Order Blocks** - Demand/supply zones with mitigation tracking, visit counting, and high/low probability classification
- **Fair Value Gaps (FVG)** - Imbalance detection with consequent encroachment (midpoint) and visit tracking
- **Liquidity Pools** - Equal highs/lows clustering with sweep detection
- **Premium/Discount Zones** - 50% midpoint of swing range
- **OTE Zone** - ICT Optimal Trade Entry (Fibonacci 0.62-0.79), configurable per asset

### 5 Trading Strategies (independently configurable)

| Strategy | Description | Key Filters |
|----------|-------------|-------------|
| **BOS Continuation** | Pullback to OB/FVG after Break of Structure | Lookback, min score |
| **OB Bounce** | Price reacts at fresh Order Block in trend direction | Zone freshness, high-prob OB, max SL |
| **CHoCH Reversal** | Pullback to OB/FVG after Change of Character | OTE zone requirement, tight lookback |
| **FVG Fill** | Price fills Fair Value Gap aligned with trend | Zone freshness, min score |
| **Liquidity Sweep** | Sweep + structural confirmation + OB/FVG entry | Sweep lookback, structure confirmation |

Each strategy has its own:
- Enable/disable toggle
- Lookback period (bars)
- Minimum confluence score threshold
- Maximum zone visits (freshness filter)
- OTE zone requirement
- High-probability OB requirement
- Kill zone requirement
- Per-strategy max SL distance

### Confluence Scoring (11 factors)
| Factor | Weight | Description |
|--------|--------|-------------|
| Order Block | 0.15 | Price inside active OB |
| Fair Value Gap | 0.15 | Price inside active FVG |
| OTE Zone | 0.12 | Price in Fibonacci 0.62-0.79 |
| BOS Confirmed | 0.10 | Trend confirmed by BOS |
| CHoCH Present | 0.10 | Reversal confirmed |
| Liquidity Sweep | 0.10 | Recent sweep detected |
| Premium/Discount | 0.08 | Correct zone for direction |
| Kill Zone | 0.08 | Active session time window |
| OB+FVG Overlap | 0.05 | Maximum confluence |
| Zone Fresh | 0.05 | First visit to zone |
| OB High Prob | 0.05 | Large-body OB classification |

### MQL5 EA Features
- **Universal lot sizing** - Works on any market (Forex, B3, Indices)
- **Break Even** - Configurable trigger/step with universal conversion
- **Trailing Stop** - Continuous with 75% directional throttling
- **Kill Zones** - Asian, London, New York, London Close (configurable hours)
- **Time/Day filters** - Configurable trading hours and days
- **OnTester()** - Custom optimization metric: `PF * sqrt(trades) * (1 - DD/100)`
- **Visual Panel** - Dashboard with trend, signal, active zones, drawdown
- **Chart Objects** - OB zones, FVG zones, swing arrows, BOS/CHoCH lines, liquidity levels, OTE zone

### SL/TP - 100% Structure-Based
- **SL** based on swing lows/highs and OB boundaries (NOT ATR, NOT fixed pips)
- **TP** targets nearest structural swing/liquidity beyond minimum distance
- Per-strategy and global min/max SL distance
- Minimum TP distance to skip micro-swings

---

## Project Structure

```
EA_SMC/
├── dll/
│   ├── include/
│   │   ├── types.h              # Core structs
│   │   ├── swing_detector.h     # Swing highs/lows
│   │   ├── structure_analyzer.h # BOS/CHoCH
│   │   ├── ob_tracker.h         # Order Blocks
│   │   ├── fvg_detector.h       # Fair Value Gaps
│   │   ├── liquidity_mapper.h   # Liquidity pools
│   │   ├── signal_generator.h   # 5 strategies + scoring
│   │   └── smc_engine.h         # DLL API (60+ exports)
│   ├── src/                     # C++ implementations
│   └── CMakeLists.txt
├── mql5/
│   └── EA_SMC.mq5              # MQL5 Expert Advisor
├── smartmoneyconcepts/
│   └── smc.py                  # Python reference implementation
├── build.sh                    # Cross-compile + deploy
└── README.md
```

---

## Build & Install

### Requirements
- **Linux**: MinGW cross-compiler, CMake 3.10+
- **MetaTrader 5**: Any broker

```bash
sudo apt install g++-mingw-w64-x86-64 cmake
bash build.sh
```

Then compile `EA_SMC.mq5` in MetaEditor (F7).

---

## Configuration

50+ configurable inputs in 14 groups. Each strategy is independently tunable.

### Optimization

Use MT5 **Genetic Optimization** with custom criterion. Key parameters:
- `BOS_Lookback`, `BOS_MinScore`
- `OB_MinScore`, `OB_MaxVisits`, `OB_ReqHP`
- `MinSLPoints`, `MinTPPoints`, `MaxSLPoints`

---

## Performance Evaluation

**Never use only R:R or Win Rate.** Required metrics:

| Metric | Minimum |
|--------|---------|
| Profit Factor | > 1.3 |
| Expectancy | > 0.5 pip/trade |
| Max Drawdown | < 25% |
| Recovery Factor | > 2.0 |

---

## Credits

- Original SMC Python library by [lbnlsj](https://github.com/lbnlsj/smart-money-concepts)
- ICT Trading Concepts
- C++ DLL architecture by NexusTecnologies

## License

MIT License
