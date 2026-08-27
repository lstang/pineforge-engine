# Repository Guidelines

PineForge Engine is a high-performance, bit-exact deterministic PineScript v6 backtesting and execution runtime written in C++17. It is validated trade-for-trade against TradingView's execution engine across a comprehensive test corpus, providing microsecond-level backtesting speed and a stable C ABI.

---

## 1. Project Overview

- **Purpose**: Execute compiled PineScript v6 strategies natively in C++ with exact TradingView trade parity (entry/exit times, fill prices, slippage, commission, pyramiding, OCA, margin calls, and bar magnifier).
- **Core Strategy**: A transpiler (`pineforge-codegen`) translates PineScript into C++ source (`generated.cpp`). The compiled strategy shared library (`strategy.so`/`.dll`) statically links `libpineforge` (`libpineforge.a`/`pineforge.lib`) and exposes a fixed, versioned 28-symbol C ABI declared in `<pineforge/pineforge.h>`.
- **ABI Stability**: Monotonic layout versioning (`PF_ABI_VERSION = 2`), append-only structs, hidden internal symbols, caller-allocated `pf_report_t` with runtime heap-allocated inner arrays freed via `report_free()`.

---

## 2. Architecture & Data Flow

```
[OHLCV CSV / Ticks Tape]
          │
          ▼
┌──────────────────────────────────────────────────────────────┐
│ TimeframeAggregator (PASSTHROUGH / RATIO / CALENDAR D, W, M) │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│ BacktestEngine::run() / BacktestEngine::dispatch_bar()       │
│                                                              │
│  1. Intrabar Sub-Bar / Path Generation                       │
│     ├── Standard: 4-waypoint OHLC path (O -> H -> L -> C)    │
│     └── Bar Magnifier: 6 sampling distributions or VWAP path │
│                                                              │
│  2. Order Processing & Fill Simulation (engine_fills.cpp)    │
│     ├── PendingOrder Queue evaluation (Stop / Limit / Trail) │
│     ├── Directional mintick snapping & slippage models       │
│     ├── OCA (One-Cancels-All) cancel / reduce mechanics      │
│     ├── Pyramiding caps & FIFO position execution            │
│     └── Margin Call Liquidation check & auto-trim            │
│                                                              │
│  3. Script Execution (on_bar / on_order_fill)                │
│     ├── Series<T> & DynamicRingBuffer history indexing       │
│     ├── 66 TA State Classes (RMA, RSI, SMA, EMA, MACD, etc.) │
│     ├── PineMatrix & PineMap state containers                │
│     └── Strategy Commands (entry, order, exit, close, cancel)│
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│ Metrics & Report Generation (engine_report.cpp / metrics.cpp)│
│  └── pf_report_t (trades, metrics, equity curve, trace logs) │
└──────────────────────────────────────────────────────────────┘
```

### Key Subsystems

1. **BacktestEngine (`include/pineforge/engine.hpp`, `src/engine_*.cpp`)**:
   - Manages positions (`PositionSide::FLAT/LONG/SHORT`, `signed_position_size_`), pending orders (`PendingOrder`), cash/equity accounting, risk filters, and report generation.
   - Supports both one-shot batch backtests (`run()`, `run_backtest_full()`) and realtime continuous streams (`stream_begin()`, `stream_push_tick()`, `stream_advance_time()`, `stream_end()`).
2. **Technical Analysis Library (`include/pineforge/ta.hpp`, `src/ta_*.cpp`)**:
   - 66 stateful TA classes with `compute()` and `recompute()` methods supporting bar updates and `calc_on_order_fills` rollbacks.
   - RMA, EMA, SMA, WMA, HMA, VWMA, RSI, MACD, Bollinger Bands, Keltner Channels, DMI/ADX, Supertrend, SAR, Stochastics, Pivot Points, etc.
3. **Timeframe & Session Engine (`include/pineforge/timeframe.hpp`, `include/pineforge/session_time.hpp`, `src/timeframe.cpp`, `src/session_time.cpp`, `src/timezone.cpp`)**:
   - TradingView calendar periods (`D`, `W`, `M`) and intraday bar aggregations.
   - `ScopedTimezone` RAII mutex-locked timezone environment manager.
   - Session window parsing (`"0930-1600:23456"`), `session.ismarket`, and `time_tradingday`.
4. **Data Structures (`include/pineforge/series.hpp`, `matrix.hpp`, `generic_matrix.hpp`, `map.hpp`, `na.hpp`)**:
   - `DynamicRingBuffer<T>` & `Series<T>`: Single-subtract circular buffer for Pine history lookbacks (`Series[0]` current, `Series[1]` previous, `na<T>()` on out-of-bounds).
   - `PineMatrix`: Eigen-backed matrix with copy-on-write aliasing and NaN handling.
   - `PineMap<K,V>`: Canonical NaN-hashing map preserving insertion order.

---

## 3. Key Directories

| Directory | Purpose |
|---|---|
| `include/pineforge/` | Public C ABI (`pineforge.h`), engine core (`engine.hpp`), TA classes (`ta.hpp`), data structures (`series.hpp`, `matrix.hpp`, `map.hpp`, `na.hpp`), and portability shims (`portability.hpp`). |
| `src/` | Engine implementation partitions: `engine_run.cpp`, `engine_fills.cpp`, `engine_orders.cpp`, `engine_risk.cpp`, `engine_security.cpp`, `engine_stream.cpp`, `c_abi.cpp`, `timeframe.cpp`, `session_time.cpp`, `timezone.cpp`, `magnifier.cpp`, `ta_*.cpp`. |
| `tests/` | 138+ standalone C++ unit/integration test binaries and CTest configuration. |
| `tutorial/` | Tutorial strategies (MACD, Multi-Timeframe HTF/LTF) and Python ctypes runner scripts (`run.py`, `run_stream.py`, `run_mtf.py`, `run_advanced.py`). |
| `scripts/` | Tooling & verification scripts (`run_strategy.py`, `verify_corpus.py`, `derive_corpus_feeds.py`, `run_corpus.sh`, `run_stream_corpus.py`, `check_c_abi_runtime.py`, `coverage.sh`). |
| `benchmarks/` | Google Benchmark runner (`benchmarks/speed/`) and 3-way comparison harness vs PyneCore and PineTS. |
| `docker/` | Docker container harness (`run_json.py`, `entrypoint.sh`, `Dockerfile`) for headless MCP/JSON evaluation. |
| `docs/` | Doxygen documentation and markdown specification pages. |
| `cmake/` | CMake version resolver, package config templates, and smoke consumer tests. |

---

## 4. Development Commands

### CMake Configuration & Build

```bash
# Configure standard Debug build with tests and tutorial
cmake -B build -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPINEFORGE_BUILD_TESTS=ON \
  -DPINEFORGE_BUILD_TUTORIAL=ON

# Build all targets (runtime library, tutorial strategies, test binaries)
cmake --build build -j 8

# Build with ASan + UBSan
cmake -B build-asan -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPINEFORGE_ENABLE_SANITIZERS=ON
cmake --build build-asan -j 8

# Build with Source Coverage (Clang / GCC)
cmake -B build-cov -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPINEFORGE_ENABLE_COVERAGE=ON
cmake --build build-cov -j 8
bash scripts/coverage.sh
```

### Running Tests

```bash
# Run entire test suite (all C++ binaries + Python self-tests)
ctest --test-dir build --output-on-failure -j 8

# Run specific test by regex with verbose output
ctest --test-dir build -R test_ta -V
ctest --test-dir build -R test_calendar_wm_open_utc_fastpath -V
ctest --test-dir build -R test_derive_corpus_feeds -V

# Run a compiled C++ test binary directly
./build/bin/test_ta
./build/bin/test_matrix
./build/bin/test_margin_call

# Run Python regression self-tests directly
python scripts/derive_corpus_feeds_self_test.py
python scripts/fingerprint_self_test.py
python scripts/run_corpus_self_test.py
python scripts/test_verify_corpus_metrics.py
python scripts/test_run_strategy_identity.py
```

### Running Tutorial & Verification Harnesses

```bash
# Run MACD tutorial strategy
python tutorial/run.py

# Run Realtime Streaming tutorial
python tutorial/run_stream.py

# Run Multi-Timeframe (HTF/LTF) tutorial
python tutorial/run_mtf.py

# Run parameter sweep demo
python tutorial/run_advanced.py

# Execute single strategy against custom OHLCV CSV
python scripts/run_strategy.py tutorial/macd --ohlcv tutorial/data/btcusdt_15m_7d.csv
```

---

## 5. Code Conventions & Common Patterns

### Floating-Point Determinism & FMA Suppression
- **Mandatory `-ffp-contract=off`**: TradingView executes in JavaScript (IEEE-754 double without FMA). All C++ compilation units MUST compile with `-ffp-contract=off` to round `*` and `+` independently. FMA introduces 1–2 ULP discrepancies that compound across RMA/EMA chains and flip threshold decisions.
- **Float Comparison**: In technical tests, use `near(a, b, 1e-9)` or `std::fabs(a - b) < 1e-9`. In engine determinism/reproducibility tests, enforce exact bit-level equality (`==`).

### Public C ABI vs Internal C++ Contract
- `<pineforge/pineforge.h>` is the **only** public contract header.
- Symbols exported in `libpineforge` must be marked with `PF_API`.
- When including `engine.hpp` in translation units defining strategy exports (`generated.cpp`), `engine.hpp` automatically defines `PINEFORGE_NO_STRATEGY_DECLS` to prevent type collisions between `pf_report_t` C PODs and internal C++ types (`ReportC`, `Bar`).
- Public ABI layouts are compile-time verified using `static_assert` in `src/c_abi.cpp`.

### Error Handling & Boundary Safety
- **No C++ Exception Leaks Across ABI**: `BacktestEngine::run()` and C ABI methods wrap execution in `try / catch (const std::exception&)` blocks.
- **Error Propagation**: Errors are stored in `last_error_` and inspected via `strategy_get_last_error(s)`.
- **Pre-check & Fail-Closed**: Timeframe validations, negative tick counts, and illegal parameter states fail early and populate `last_error_`.

### Memory Management & Resource Lifetime
- Strategy handles (`pf_strategy_t`) are created with `strategy_create()` and destroyed with `strategy_free()`.
- Reports (`pf_report_t`) are allocated by the caller on the stack/heap, while embedded arrays (`trades`, `security_diag`, `trace`, `equity_curve`) are allocated by the engine and **must** be released via `report_free(report)`.

### Timezone & Thread Safety
- Mutex-guarded RAII scope `pine_tz::ScopedTimezone` locks a process-wide mutex and sets `TZ` lazily to avoid redundant `tzset()` syscall storms.
- `portability.hpp` provides cross-platform Howard Hinnant date conversion algorithms (`portable_timegm`, `portable_gmtime_r`, `portable_localtime_r`, `portable_mktime`), ensuring consistent behavior across Linux, macOS, and Windows for pre-1970 and modern timestamps.

### Test Authoring Pattern
C++ test files in `tests/` are standalone executables with `int main()` and custom `CHECK` macros (no heavy testing framework):

```cpp
#include <pineforge/engine.hpp>
#include <pineforge/bar.hpp>
#include <cstdio>
#include <vector>

using namespace pineforge;

static int passed = 0;
static int failed = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failed; \
    } else { \
        ++passed; \
    } \
} while (0)

class TestProbe : public BacktestEngine {
public:
    void on_bar(const Bar& bar) override {
        strategy_entry("Long", true);
    }
};

static void test_example() {
    std::printf("test_example\n");
    TestProbe probe;
    std::vector<Bar> feed = { /* ... */ };
    probe.run(feed.data(), static_cast<int>(feed.size()));
    CHECK(probe.trade_count() > 0);
}

int main() {
    test_example();
    std::printf("test_example: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
```

---

## 6. Important Files

- `CMakeLists.txt`: Root build definition, compiler flags, option toggles, dependency discovery, and package export.
- `include/pineforge/pineforge.h`: Canonical public C ABI declarations and POD data structures.
- `include/pineforge/engine.hpp`: `BacktestEngine` class definition, position/order structs, internal helpers.
- `include/pineforge/ta.hpp`: 66 official PineScript technical analysis stateful indicator classes.
- `include/pineforge/portability.hpp`: Cross-platform date, time, and timezone shims.
- `src/c_abi.cpp`: ABI implementation, `static_assert` struct layout validation, and C runtime exports.
- `src/engine_run.cpp` & `src/engine_fills.cpp`: Bar dispatch lifecycle, intrabar paths, and pending order execution.
- `scripts/run_strategy.py`: Standard Python ctypes harness for loading and running strategy `.so` files.
- `scripts/verify_corpus.py`: Parity evaluation rubric and trade comparison engine against TradingView outputs.
- `scripts/derive_corpus_feeds.py`: Deterministic 1m -> 15m OHLCV resampler with atomic file writes.

---

## 7. Runtime & Tooling Preferences

- **Language Standard**: C++17 (`set(CMAKE_CXX_STANDARD 17)` required, no compiler extensions).
- **C Standard**: C11 for public header verification.
- **Supported Compilers**: Clang (14+), GCC (9+), MSVC / Clang-CL (VS 2022 / VS 2026).
- **Build System**: CMake >= 3.16 + Ninja (recommended for fast parallel builds).
- **External Dependencies**:
  - **Eigen3 (3.3+)**: Required for `matrix.*` operations (system install or automated `FetchContent`).
  - **Threads**: For concurrent timezone validation.
  - **Python 3.9+**: For ctypes harnesses, validation scripts, and CTest integration.
- **Linking Semantics**: Strategy shared libraries use whole-archive linking (`-Wl,--whole-archive` on Linux, `-Wl,-force_load` on macOS, `-Wl,/WHOLEARCHIVE` on Windows) to retain all C ABI exports from `libpineforge`.

---

## 8. Testing & QA

- **CTest Integration**: Every C++ test binary and Python self-test is registered in CTest.
- **Coverage**: Source-level coverage supported via `scripts/coverage.sh` (Clang `llvm-cov` or GCC `gcovr`).
- **Sanitizers**: Zero-leak and undefined-behavior compliance verified under `PINEFORGE_ENABLE_SANITIZERS=ON` (ASan + UBSan).
- **Regression Gates**:
  - `check_c_abi_runtime.py` ensures no undocumented symbols leak into the public ABI.
  - `fingerprint_self_test.py` validates canonical RFC-8785 JSON hashing and float tokenization.
  - `verify_corpus.py` verifies strict trade-by-trade parity against TradingView reference trade lists.
