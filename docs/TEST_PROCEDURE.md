# SensESP Chain Counter - Test Procedure

This document describes how to run the comprehensive 56-test suite and analyze results.

## Overview

The test suite validates anchor deployment and retrieval across multiple conditions:
- **7 wind speeds**: 1, 4, 8, 12, 18, 20, 25 knots
- **4 depths**: 3, 5, 8, 12 meters
- **2 operations**: autoDrop (deployment), autoRetrieve (retrieval)
- **Total**: 56 tests (7 × 4 × 2)

## Prerequisites

### 1. Hardware Setup
- ESP32 connected via USB serial
- Simulation environment running (provides depth, distance, wind data)
- Serial port available (e.g., `/dev/cu.usbserial-0001`)

### 2. Software Requirements
- PlatformIO CLI installed
- Bash shell (macOS/Linux)
- `bc` calculator (for progress calculations)
- Serial port not locked by other processes

### 3. Test Firmware
The firmware must have the `testNotification` command handler (added in commit `d8f401c`):
- Receives test boundary notifications from simulation
- Triggers test execution based on test number
- Reports completion status

## Running the Test Suite

### Quick Start

```bash
# 1. Build and upload firmware
pio run -t upload

# 2. Start test orchestration
./scripts/orchestrate-tests.sh
```

The script will:
- Auto-detect serial port
- Wait for test notifications from simulation
- Capture each test to separate log file
- Run analysis after each test
- Generate running summary
- Save all results to timestamped directory

### Manual Options

```bash
# Specify serial port
./scripts/orchestrate-tests.sh --port /dev/cu.usbserial-0001

# Custom baud rate
./scripts/orchestrate-tests.sh --baud 115200

# Skip analysis (capture only)
./scripts/orchestrate-tests.sh --no-analysis

# Show help
./scripts/orchestrate-tests.sh --help
```

### Test Sequence

The simulation runs tests in this order:

| Test | Type | Wind | Depth | Target Rode |
|------|------|------|-------|-------------|
| 1 | autoDrop | 1kn | 3m | 15m |
| 2 | autoRetrieve | 1kn | 3m | 2m |
| 3 | autoDrop | 1kn | 5m | 25m |
| 4 | autoRetrieve | 1kn | 5m | 2m |
| ... | ... | ... | ... | ... |
| 55 | autoDrop | 25kn | 12m | 60m |
| 56 | autoRetrieve | 25kn | 12m | 2m |

**Test Naming Convention:**
```
test-NNN_TYPE_WINDkn_DEPTHm.log
```

Examples:
- `test-001_autoDrop_1kn_3m.log`
- `test-002_autoRetrieve_1kn_3m.log`
- `test-056_autoRetrieve_25kn_12m.log`

## During Test Execution

### Progress Display

The orchestrator shows real-time progress:

```
[Test 15/56] autoRetrieve @ 8kn, 5m → 2m ... RUNNING
Progress: 14/56 (25.0%) | Passed: 12 | Failed: 2 | Success: 85.7%
```

### Stopping Tests

Press `Ctrl+C` to stop gracefully:
- Completes current test
- Finishes pending analyses
- Generates final summary
- Saves all captured data

### Common Issues

**Serial port locked:**
```
Error: Serial port is in use by another process
```
Solution: Close VS Code serial monitor or run:
```bash
pkill -f 'platformio device monitor'
```

**No serial port found:**
```
Error: No serial port found
```
Solution: Specify manually:
```bash
./scripts/orchestrate-tests.sh --port /dev/cu.usbserial-0001
```

**Test timeout (>10 minutes):**
- Test marked as failed
- Next test continues automatically
- Check log file for stuck state

## Test Output

### Directory Structure

```
logs/test-runs/YYYY-MM-DD_HH-MM-SS/
├── test-001_autoDrop_1kn_3m.log          # Individual test logs
├── test-002_autoRetrieve_1kn_3m.log
├── ...
├── test-056_autoRetrieve_25kn_12m.log
├── test-001_autoDrop_1kn_3m.result       # Analysis results
├── test-002_autoRetrieve_1kn_3m.result
├── ...
├── summary.txt                            # Final summary report
├── .pass_count                            # Internal tracking
└── .fail_count                            # Internal tracking

logs/test-runs/latest -> YYYY-MM-DD_HH-MM-SS  # Symlink to latest run
```

### Individual Test Logs

Each test log contains:
- Test start notification
- Command execution (autoDrop or autoRetrieve)
- State transitions (for autoDrop)
- Pause/resume cycles (for autoRetrieve)
- Completion status
- Error messages (if any)

### Analysis Results

Each `.result` file contains:
- Test outcome (PASS/FAIL/TIMEOUT)
- Duration
- Key metrics
- Error summary

### Summary Report

The `summary.txt` contains:
- Overall statistics (pass rate, total duration)
- Per-test results table
- Failure analysis
- Recommendations

## Analyzing Results

### Comprehensive Analysis

After tests complete, run full analysis:

```bash
# Analyze entire test run
./scripts/extract-tests-from-log.sh logs/device-monitor-YYMMDD-HHMMSS.log

# Or use log-analysis skill
claude skill:log-analysis --log logs/device-monitor-YYMMDD-HHMMSS.log
```

This generates a comprehensive report with:
- Executive summary (pass rate, major findings)
- Comparison with previous runs
- Timeout analysis
- Success analysis by depth and wind
- Detailed timing metrics:
  - Command-to-movement latency
  - WAIT_TIGHT stage durations
  - Pause/resume cycle counts and durations
  - Inter-test delays
- Bug reports
- Recommendations

### Key Metrics to Track

**1. Command-to-Movement Latency**
- Time from command received to relay activation
- autoDrop: ~55ms (includes DeploymentManager init)
- autoRetrieve: ~10ms (direct ChainController)

**2. WAIT_TIGHT Stage Duration**
- Time waiting for boat to drift after initial drop
- Varies by wind speed and depth
- Target: <10 seconds

**3. Pause/Resume Cycles (autoRetrieve)**
- Count: Number of slack-based pauses
- Duration: Average pause time
- Should increase with wind speed
- More chain = more cycles

**4. Inter-Test Delays**
- autoDrop → autoRetrieve: 0-1 second
- autoRetrieve → autoDrop: 10-11 seconds (simulation reset)
- After timeout: may be longer (manual intervention)

**5. Pass Rates**
- Overall: Target >95%
- autoDrop: Should be 100% (deployment is simpler)
- autoRetrieve: May have failures at shallow depths

**6. SPIFFS Write Count**
- Number of NVS writes to persist chain position
- Written every 2m of movement AND 5 seconds elapsed
- Force-saved on stop/timeout
- Critical for flash wear analysis
- Baseline: ~23 writes per test average (1268 writes / 56 tests)
- autoDrop: Higher writes (more chain movement)
- autoRetrieve: Lower writes (less total movement)

## Comparing Test Runs

### Baseline (Dec 7, 2025)

**Commit:** `8dd2f96` (post-timeout-removal)
**Log:** `device-monitor-251207-054815.log`
**Results:**
- Total: 56 tests (28 autoDrop + 28 autoRetrieve)
- Passed: 48 (85.7%)
- Failed: 8 (all autoRetrieve at 3m/5m depths)
- autoDrop: 100% success
- autoRetrieve: 71% success (20/28)

**Performance Metrics:**
- SPIFFS writes: 1268 total (~23 per test average)
- Command latency: autoDrop 55ms, autoRetrieve 10ms
- WAIT_TIGHT: 0-6 seconds average
- Pause/resume: 3-11 cycles per retrieval

**Major Findings:**
- ✅ External timeout removal validated (0 false timeouts)
- ✅ Slack-based pause/resume working correctly
- ⚠️ Shallow-depth edge case (fixed in simulation)

### Running Comparison

```bash
# 1. Run new test suite
./scripts/orchestrate-tests.sh

# 2. Compare with baseline
# Look at summary.txt in both runs
diff logs/test-runs/2025-12-07_05-48-15/summary.txt \
     logs/test-runs/latest/summary.txt

# 3. Compare specific metrics
# Extract metrics from both comprehensive analyses
grep "Pass Rate" logs/test-analysis-*.md
grep "Command-to-Movement" logs/test-analysis-*.md
grep "autoRetrieve Success" logs/test-analysis-*.md
```

### What to Look For

**Improvements:**
- Higher overall pass rate
- Fewer timeouts
- Faster command latency
- Shorter WAIT_TIGHT durations

**Regressions:**
- Lower pass rate
- New failure patterns
- Increased latency
- More pause cycles at same conditions

**Consistency:**
- Similar timing across same conditions
- Predictable pause/resume behavior
- Stable error rates

## Test Coverage

### Current Coverage

**Operations:**
- ✅ autoDrop (deployment with staged pauses)
- ✅ autoRetrieve (retrieval with slack monitoring)
- ❌ Manual up/down (not automated)
- ❌ Emergency stop (not automated)

**Environmental Conditions:**
- ✅ Wind: 1-25 knots (7 levels)
- ✅ Depth: 3-12 meters (4 levels)
- ❌ Tide variation (tested via fixed offsets)
- ❌ Current (not modeled)

**Edge Cases:**
- ✅ Shallow depths (3m/5m)
- ✅ High wind (20-25kn)
- ✅ Slack-based pause/resume
- ✅ Movement timeout safety
- ❌ Anchor drag detection
- ❌ Counter overflow
- ❌ Signal K connection loss

### Extending Test Coverage

To add new test scenarios:

1. **Update simulation** to generate new test cases
2. **Update TOTAL_TESTS** in `orchestrate-tests.sh`
3. **Update parse-test-number.sh** with new parameter mappings
4. **Run test suite** and verify new scenarios

## Troubleshooting

### Tests Not Starting

**Symptom:** Orchestrator waiting, no tests run
**Causes:**
- Simulation not sending testNotification
- Wrong serial port
- Firmware doesn't have testNotification handler

**Solution:**
```bash
# Verify firmware has testNotification
grep -r "testNotification" src/main.cpp

# Monitor serial manually
pio device monitor

# Check for test notifications in output
```

### Tests Failing Unexpectedly

**Symptom:** High failure rate, different from baseline
**Causes:**
- Code regression
- Simulation parameters changed
- Hardware issue (serial buffer, timing)

**Solution:**
1. Check git diff for recent changes
2. Verify simulation parameters match baseline
3. Re-run single test manually to isolate
4. Check individual test log for error details

### Analysis Not Running

**Symptom:** No .result files, no summary
**Causes:**
- Analysis scripts not executable
- Missing dependencies (bc, grep, etc.)
- Disk full

**Solution:**
```bash
# Make scripts executable
chmod +x scripts/*.sh

# Run analysis manually
./scripts/analyze-test-result.sh logs/test-runs/latest/test-001_autoDrop_1kn_3m.log

# Generate summary manually
./scripts/generate-test-summary.sh logs/test-runs/latest
```

## Best Practices

1. **Run tests with clean firmware build**
   ```bash
   pio run -t clean
   pio run -t upload
   ```

2. **Verify simulation is ready** before starting orchestrator

3. **Don't interfere during test run** (no manual commands)

4. **Save baseline runs** for comparison
   ```bash
   cp -r logs/test-runs/latest logs/test-runs/baseline-vX.Y.Z
   ```

5. **Document significant changes** in test results
   - What changed in code
   - Expected impact
   - Actual results

6. **Run cleanup after major test sessions**
   ```bash
   ./scripts/cleanup-logs.sh
   ```

## Files Reference

**Test Scripts:**
- `scripts/orchestrate-tests.sh` - Main orchestrator
- `scripts/parse-test-number.sh` - Test parameter lookup
- `scripts/analyze-test-result.sh` - Per-test analysis
- `scripts/generate-test-summary.sh` - Summary generation
- `scripts/extract-tests-from-log.sh` - Extract from monolithic log

**Analysis:**
- `.claude/skills/log-analysis/` - Claude skill for comprehensive analysis
- `logs/test-analysis-*.md` - Generated analysis reports

**Documentation:**
- `docs/TEST_PROCEDURE.md` - This file
- `scripts/README.md` - Script documentation
- `logs/README.md` - Log directory documentation

## See Also

- [CODEBASE_OVERVIEW.md](CODEBASE_OVERVIEW.md) - Architecture overview
- [ARCHITECTURE_MAP.md](ARCHITECTURE_MAP.md) - System diagrams
- [mcp_chaincontroller.md](mcp_chaincontroller.md) - ChainController API
- [logs/test-analysis-251207-comprehensive.md](../logs/test-analysis-251207-comprehensive.md) - Baseline analysis
