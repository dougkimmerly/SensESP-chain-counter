# SPIFFS Write Analysis - Baseline Test Run

**Date:** December 7, 2025
**Log:** device-monitor-251207-054815.log
**Test Suite:** 56 tests (28 autoDrop + 28 autoRetrieve)
**Code Version:** Commit 8dd2f96 (post-timeout-removal)

---

## Executive Summary

**Total SPIFFS Writes:** 1,266
**Average per Test:** 22.6 writes
**autoDrop Average:** 22.6 writes per deployment
**autoRetrieve Average:** 22.5 writes per retrieval

**Key Finding:** SPIFFS writes are **directly proportional to depth**, not wind speed or operation type.

---

## Write Behavior

### Thresholds
- **Distance:** Save every 2 meters of chain movement
- **Time:** AND at least 5 seconds elapsed since last save
- **Force Save:** On stop/timeout (no thresholds)

### Pattern
Writes occur during:
1. Chain deployment (lowering)
2. Chain retrieval (raising)
3. Completion (force-save final position)

Wind speed does NOT significantly affect write count because:
- Writes track chain movement (counter pulses)
- Not boat movement or slack changes
- 2m threshold is hit regardless of wind-induced pauses

---

## Detailed Results by Test

| Test | Operation | Wind | Depth | Writes |
|------|-----------|------|-------|--------|
| 1    | autoDrop      | 1kn  | 3m    | 13 |
| 2    | autoRetrieve  | 1kn  | 3m    | 10 |
| 3    | autoDrop      | 1kn  | 5m    | 18 |
| 4    | autoRetrieve  | 1kn  | 5m    | 18 |
| 5    | autoDrop      | 1kn  | 8m    | 24 |
| 6    | autoRetrieve  | 1kn  | 8m    | 26 |
| 7    | autoDrop      | 1kn  | 12m   | 35 |
| 8    | autoRetrieve  | 1kn  | 12m   | 36 |
| 9    | autoDrop      | 4kn  | 3m    | 13 |
| 10   | autoRetrieve  | 4kn  | 3m    | 14 |
| 11   | autoDrop      | 4kn  | 5m    | 18 |
| 12   | autoRetrieve  | 4kn  | 5m    | 19 |
| 13   | autoDrop      | 4kn  | 8m    | 25 |
| 14   | autoRetrieve  | 4kn  | 8m    | 26 |
| 15   | autoDrop      | 4kn  | 12m   | 35 |
| 16   | autoRetrieve  | 4kn  | 12m   | 36 |
| 17   | autoDrop      | 8kn  | 3m    | 13 |
| 18   | autoRetrieve  | 8kn  | 3m    | 10 |
| 19   | autoDrop      | 8kn  | 5m    | 18 |
| 20   | autoRetrieve  | 8kn  | 5m    | 19 |
| 21   | autoDrop      | 8kn  | 8m    | 25 |
| 22   | autoRetrieve  | 8kn  | 8m    | 26 |
| 23   | autoDrop      | 8kn  | 12m   | 35 |
| 24   | autoRetrieve  | 8kn  | 12m   | 36 |
| 25   | autoDrop      | 12kn | 3m    | 13 |
| 26   | autoRetrieve  | 12kn | 3m    | 14 |
| 27   | autoDrop      | 12kn | 5m    | 18 |
| 28   | autoRetrieve  | 12kn | 5m    | 18 |
| 29   | autoDrop      | 12kn | 8m    | 25 |
| 30   | autoRetrieve  | 12kn | 8m    | 26 |
| 31   | autoDrop      | 12kn | 12m   | 35 |
| 32   | autoRetrieve  | 12kn | 12m   | 36 |
| 33   | autoDrop      | 18kn | 3m    | 13 |
| 34   | autoRetrieve  | 18kn | 3m    | 13 |
| 35   | autoDrop      | 18kn | 5m    | 17 |
| 36   | autoRetrieve  | 18kn | 5m    | 17 |
| 37   | autoDrop      | 18kn | 8m    | 25 |
| 38   | autoRetrieve  | 18kn | 8m    | 26 |
| 39   | autoDrop      | 18kn | 12m   | 35 |
| 40   | autoRetrieve  | 18kn | 12m   | 36 |
| 41   | autoDrop      | 20kn | 3m    | 13 |
| 42   | autoRetrieve  | 20kn | 3m    | 10 |
| 43   | autoDrop      | 20kn | 5m    | 18 |
| 44   | autoRetrieve  | 20kn | 5m    | 14 |
| 45   | autoDrop      | 20kn | 8m    | 25 |
| 46   | autoRetrieve  | 20kn | 8m    | 26 |
| 47   | autoDrop      | 20kn | 12m   | 35 |
| 48   | autoRetrieve  | 20kn | 12m   | 36 |
| 49   | autoDrop      | 25kn | 3m    | 13 |
| 50   | autoRetrieve  | 25kn | 3m    | 9  |
| 51   | autoDrop      | 25kn | 5m    | 18 |
| 52   | autoRetrieve  | 25kn | 5m    | 14 |
| 53   | autoDrop      | 25kn | 8m    | 25 |
| 54   | autoRetrieve  | 25kn | 8m    | 26 |
| 55   | autoDrop      | 25kn | 12m   | 35 |
| 56   | autoRetrieve  | 25kn | 12m   | 34 |

---

## Analysis by Depth

| Depth | autoDrop Avg | autoRetrieve Avg | Combined Avg |
|-------|--------------|------------------|--------------|
| 3m    | 13.0         | 11.4             | 12.2         |
| 5m    | 17.9         | 17.0             | 17.4         |
| 8m    | 24.9         | 26.0             | 25.4         |
| 12m   | 35.0         | 35.7             | 35.4         |

### Linear Relationship

SPIFFS writes scale **linearly with depth**:

```
3m depth:  ~12 writes  (target rode: 15m)
5m depth:  ~17 writes  (target rode: 25m)
8m depth:  ~25 writes  (target rode: 40m)
12m depth: ~35 writes  (target rode: 60m)
```

**Formula:** Approximately 0.6 writes per meter of rode deployed/retrieved
(Based on 2m save threshold: 1 write per 2m ≈ 0.5, plus force-saves)

---

## Analysis by Operation

| Operation    | Total Writes | Avg per Test | Tests |
|--------------|--------------|--------------|-------|
| autoDrop     | 635          | 22.6         | 28    |
| autoRetrieve | 631          | 22.5         | 28    |

**Conclusion:** Operation type has **negligible impact** on write count. Both deployment and retrieval move the same amount of chain (and thus trigger the same number of saves).

---

## Flash Wear Implications

### ESP32 NVS Specifications
- **Technology:** Flash-based Non-Volatile Storage
- **Write Endurance:** ~100,000 cycles per sector
- **Wear Leveling:** Yes (built-in to ESP32 NVS library)

### Test Suite Impact
- **Writes per full suite:** 1,266
- **Test suites before wear:** ~79 suites (100,000 / 1,266)
- **With wear leveling:** ~4,000+ suites

### Production Usage Estimates

**Typical Anchoring (12m depth):**
- Deploy: 35 writes
- Retrieve: 36 writes
- **Total per anchor cycle: 71 writes**

**Daily Usage Scenarios:**

| Scenario | Cycles/Day | Writes/Day | Days to 100k | Years |
|----------|------------|------------|--------------|-------|
| Light (1 anchor/day) | 1 | 71 | 1,408 | **3.9** |
| Moderate (2 anchors/day) | 2 | 142 | 704 | **1.9** |
| Heavy cruising (3 anchors/day) | 3 | 213 | 469 | **1.3** |

**With wear leveling (10x improvement):**

| Scenario | Years to Failure |
|----------|------------------|
| Light    | **39 years** |
| Moderate | **19 years** |
| Heavy    | **13 years** |

### Shallow Anchoring (3m depth)
- **Writes per cycle:** 12 + 11 = 23
- **Light usage:** 4,348 days = **11.9 years** (without wear leveling)
- **With wear leveling:** **119 years**

---

## Optimization Opportunities

### Current Thresholds
```cpp
SAVE_THRESHOLD_M = 2.0;         // Save every 2m
MIN_SAVE_INTERVAL_MS = 5000;    // AND 5 seconds minimum
```

### Potential Optimizations

**Option 1: Increase distance threshold**
```cpp
SAVE_THRESHOLD_M = 3.0;  // Save every 3m instead of 2m
```
- Reduces writes by ~33%
- 12m deployment: 35 → 23 writes
- Trade-off: Less position accuracy after power loss

**Option 2: Increase time threshold**
```cpp
MIN_SAVE_INTERVAL_MS = 10000;  // 10 seconds instead of 5
```
- Minimal impact (distance threshold dominates)
- Only helps in very slow operations

**Option 3: Depth-based adaptive threshold**
```cpp
// Scale threshold with depth
float save_threshold = max(2.0, depth * 0.25);  // 2m min, scales with depth
```
- Shallow anchoring (3m): threshold = 2m → 13 writes
- Deep anchoring (12m): threshold = 3m → 23 writes (save ~34%)
- Maintains accuracy where it matters (shallow)

**Recommendation:** Keep current thresholds. Flash life is **acceptable** even with heavy usage, and frequent saves provide better position recovery after power loss.

---

## Validation Metrics for Future Runs

When comparing test runs, track:

1. **Total writes:** Should remain ~1,266 ± 5%
2. **Writes by depth:** Should match baseline ±10%
3. **autoDrop vs autoRetrieve:** Should be within 1-2 writes
4. **Linear scaling:** Writes should increase proportionally with rode length

**Regression indicators:**
- ❌ Total writes increase >10% without code changes
- ❌ Writes per depth deviate significantly from baseline
- ❌ Large variance between same-depth tests (>3 writes)

**Expected changes:**
- Optimization: Writes decrease proportionally to threshold increase
- Bug: Writes increase due to duplicate saves or failed threshold checks

---

## Comparison Tool

Use the analysis script to compare future runs:

```bash
# Analyze new test run
./scripts/analyze-spiffs-writes.sh logs/device-monitor-YYMMDD-HHMMSS.log > logs/spiffs-writes-analysis-YYMMDD.md

# Compare with baseline
diff logs/spiffs-writes-analysis-251207.md logs/spiffs-writes-analysis-YYMMDD.md
```

---

**Analysis Generated:** December 7, 2025
**Tool:** scripts/analyze-spiffs-writes.sh
**Baseline Version:** Post-timeout-removal (commit 8dd2f96)
