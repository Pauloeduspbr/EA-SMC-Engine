# EA-SMC-Engine — Smart Money Concepts Expert Advisor

## Arquitetura
- **DLL (C++):** dll/include/*.h + dll/src/*.cpp — toda logica de deteccao SMC
- **MQL5:** mql5/EA_SMC.mq5 — wrapper fino, trading, risk, visuais
- **Build:** CMakeLists.txt (MinGW x64), deploy para Terminal + Tester agents

## Estrategias (5)
1. OB Bounce — entrada em Order Block fresco com trend
2. BOS Continuation — continuacao apos Break of Structure
3. CHoCH Reversal — reversao apos Change of Character
4. FVG Fill — entrada em Fair Value Gap com trend
5. Liquidity Sweep — entrada apos sweep de liquidez

## Scoring de Confluencia (11 fatores, pesos fixos na DLL)
OB(0.15) + FVG(0.15) + OTE(0.12) + BOS(0.10) + CHoCH(0.10) + LiqSweep(0.10) + Premium/Discount(0.08) + KillZone(0.08) + OB+FVG overlap(0.05) + Fresh(0.05) + HighProb(0.05) = max 1.0

## Tools MCP Disponiveis

### EA Specialist (usar para auditoria e analise)
- `validate_dll_contract` — validar contrato DLL<->MQL5
- `analyze_struct_alignment` — verificar alinhamento de structs
- `diagnose_pipeline` — diagnosticar bottlenecks de sinais
- `analyze_filter_efficiency` — custo vs beneficio de filtros
- `evaluate_metrics` — avaliar metricas contra thresholds do CLAUDE.md pai
- `search_by_strategy` — buscar livros por estrategia (ict, fibonacci, etc)
- `read_chapter` — ler capitulo de livro da base de conhecimento

### Trading Knowledge (referencia de estrategias)
- `search_trading_knowledge` — buscar na base de 17 livros
- `get_mql5_reference` — referencia MQL5
- `get_chapter` — ler capitulo especifico

## Slash Commands Customizados
- `/audit-ea` — auditoria completa antes de backtest
- `/analyze-backtest` — analise de resultado com 5 camadas
- `/optimize-params` — preparar/analisar otimizacao de parametros
- `/diagnose-pipeline` — diagnosticar 0 trades ou poucos trades

## Deploy Paths
- DLL build: dll/build/smc_engine.dll
- Terminal: C:\Users\santospaulo\AppData\Roaming\MetaQuotes\Terminal\FEC98F1D078C037902D797DB372EA18E\MQL5\Libraries\
- Tester: ...Tester\...\Agent-127.0.0.1-300X\MQL5\Libraries\ (17 agentes)
- EA source: deve ser copiado para Terminal\MQL5\Experts\ e compilado

## Regras
- Seguir TODAS as regras do CLAUDE.md pai em EA_Projetos/
- NUNCA criar scripts .py para analise — usar inline ou tools MCP
- OnTester custom metric: PF * sqrt(trades) * (1 - DD/100)
- Parametros de scoring sao hardcoded na DLL — recompilacao necessaria para alterar pesos
