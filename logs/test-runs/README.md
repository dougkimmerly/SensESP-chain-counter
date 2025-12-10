# Test Run Orchestration System

This directory contains automated test runs for the SensESP Chain Counter. The orchestration system automatically captures serial logs, analyzes test results, and generates comprehensive summaries.

## Directory Structure

```
test-runs/
├── .gitignore                              # Git configuration (logs ignored)
├── README.md                              # This file
├── latest -> 2024-12-06_15-30-45/        # Symlink to most recent run
└── 2024-12-06_15-30-45/                  # Test run directory (timestamp)
    ├── test-001_autoDrop_1kn_3m.log      # Individual test log
    ├── test-001_autoDrop_1kn_3m.log.analysis.txt
    ├── test-002_autoRetrieve_1kn_3m.log
    ├── test-002_autoRetrieve_1kn_3m.log.analysis.txt
    ├── ...                                # 56 test files total
    └── summary.txt                        # Overall summary
```

## Test Suite Overview

**56 Total Tests** organized as:
- **4 depths**: 3m (rode 15m), 5m (rode 25m), 8m (rode 40m), 12m (rode 60m)
- **7 wind speeds per depth**: 1, 4, 8, 12, 18, 20, 25 knots
- **Pattern**: autoDrop → autoRetrieve pairs at each wind speed

### Test Sequence

| Tests  | Depth | Target Rode | Wind Speeds (kn)      |
|--------|-------|-------------|------------------------|
| 1-14   | 3m    | 15m         | 1, 4, 8, 12, 18, 20, 25 |
| 15-28  | 5m    | 25m         | 1, 4, 8, 12, 18, 20, 25 |
| 29-42  | 8m    | 40m         | 1, 4, 8, 12, 18, 20, 25 |
| 43-56  | 12m   | 60m         | 1, 4, 8, 12, 18, 20, 25 |

Each depth/wind combination runs:
1. **autoDrop** - Deploy to target rode
2. **autoRetrieve** - Retrieve to 2m

## Usage

### Starting a Test Run

```bash
# Start the orchestrator FIRST (in one terminal)
cd /Users/doug/Programming/dkSRC/SenseESP/SensESP-chain-counter
./scripts/orchestrate-tests.sh

# The orchestrator will:
# - Auto-detect serial port
# - Create timestamped run directory
# - Wait for test notifications
# - Capture all 56 tests automatically
# - Run analysis after each test
# - Generate running summary

# Then start your test simulator (in another terminal/process)
# The orchestrator will automatically detect and log all tests
```

### Command Line Options

```bash
./scripts/orchestrate-tests.sh [options]

Options:
  --port <device>    Serial port (default: auto-detect)
  --baud <rate>      Baud rate (default: 115200)
  --no-analysis      Skip automatic analysis (capture logs only)
  --help             Show help message
```

### Monitoring Progress

```bash
# View real-time summary
tail -f logs/test-runs/latest/summary.txt

# View latest test log
tail -f logs/test-runs/latest/test-*.log | tail -1

# Check progress
ls -lh logs/test-runs/latest/
```

### After Test Completion

```bash
# View final summary
cat logs/test-runs/latest/summary.txt

# Review specific test
cat logs/test-runs/latest/test-001_autoDrop_1kn_3m.log
cat logs/test-runs/latest/test-001_autoDrop_1kn_3m.log.analysis.txt

# List all failed tests
grep "^STATUS: FAIL" logs/test-runs/latest/*.analysis.txt
```

## Scripts

### 1. orchestrate-tests.sh (Main Script)

**Purpose**: Monitor serial output, detect test boundaries, create per-test logs

**Features**:
- Auto-detect serial port
- Real-time log capture
- Automatic test boundary detection
- Per-test log file creation
- Background analysis execution
- Progress display
- Graceful shutdown (Ctrl+C)

### 2. parse-test-number.sh

**Purpose**: Convert test number (1-56) to test parameters

**Usage**:
```bash
./scripts/parse-test-number.sh 23
# Output:
# testNumber=23
# testType=autoDrop
# depth=8
# windSpeed=8
# targetRode=40.0
```

### 3. analyze-test-result.sh

**Purpose**: Analyze single test log and determine PASS/FAIL

**Usage**:
```bash
./scripts/analyze-test-result.sh logs/test-runs/latest/test-001_autoDrop_1kn_3m.log
```

**Pass Criteria**:

**autoDrop tests:**
- Reached target rode ±1.0m
- No ERROR messages
- Completed successfully
- Negative slack occurrences < 10
- No stuck conditions (same value >20 times)

**autoRetrieve tests:**
- Reached 2.0m ±0.5m
- No free fall direction changes
- Rode decreased monotonically
- No ERROR messages
- Completed successfully

### 4. generate-test-summary.sh

**Purpose**: Create overall summary from all test analyses

**Usage**:
```bash
./scripts/generate-test-summary.sh logs/test-runs/latest/
```

**Output includes**:
- Total/Pass/Fail counts
- Success rate by test type
- Success rate by depth
- Success rate by wind speed
- List of failed tests with reasons
- Timing statistics

## Test Detection Logic

The orchestrator monitors for these patterns in serial output:

### Test Start
```
I (xxxxx) src/main.cpp: Command received is testNotification
```

### Test Type Detection
```
I (xxxxx) src/main.cpp: Command received is autoDrop
I (xxxxx) src/main.cpp: Command received is autoRetrieve
```

### Test Completion
```
I (xxxxx) ChainController: autoDrop completed
I (xxxxx) RetrievalManager: Auto-retrieve complete, stopping
I (xxxxx) src/main.cpp: Command received is testNotification  # Next test
```

### Timeout
- Test running > 10 minutes triggers automatic completion

## File Naming Convention

```
test-NNN_<type>_<wind>kn_<depth>m.log
```

Examples:
- `test-001_autoDrop_1kn_3m.log` - Test 1: Drop at 1kn, 3m depth
- `test-002_autoRetrieve_1kn_3m.log` - Test 2: Retrieve at 1kn, 3m depth
- `test-014_autoRetrieve_25kn_3m.log` - Test 14: Retrieve at 25kn, 3m depth
- `test-015_autoDrop_1kn_5m.log` - Test 15: Drop at 1kn, 5m depth (new depth)

## Analysis File Format

Each test generates a `.analysis.txt` file:

```
==============================================
Test Analysis Report
==============================================
Test Number: 1
Test Type: autoDrop
Depth: 3m
Wind Speed: 1kn
Target Rode: 15.0m
----------------------------------------------
STATUS: PASS
Final Rode: 15.2m
Duration: 2m 34s
Completed: true
----------------------------------------------
Errors: 0
Warnings: 2
Negative Slack Events: 3
----------------------------------------------
Issues: None
==============================================
```

## Summary File Format

The `summary.txt` file includes:

```
============================================================
SensESP Chain Counter - Test Run Summary
============================================================
Generated: 2024-12-06 15:30:45
Test Directory: /path/to/run

OVERALL RESULTS
------------------------------------------------------------
Total Tests: 56 / 56
Passed: 52
Failed: 4
Incomplete: 0
Success Rate: 92.9%

============================================================
SUCCESS RATE BY TEST TYPE
------------------------------------------------------------
autoDrop       : 26/28 (92.9%)
autoRetrieve   : 26/28 (92.9%)

============================================================
SUCCESS RATE BY DEPTH
------------------------------------------------------------
3m             : 13/14 (92.9%)
5m             : 13/14 (92.9%)
8m             : 13/14 (92.9%)
12m            : 13/14 (92.9%)

============================================================
SUCCESS RATE BY WIND SPEED
------------------------------------------------------------
1kn            :  7/8  (87.5%)
4kn            :  8/8  (100.0%)
8kn            :  8/8  (100.0%)
12kn           :  8/8  (100.0%)
18kn           :  7/8  (87.5%)
20kn           :  7/8  (87.5%)
25kn           :  7/8  (87.5%)

============================================================
TIMING STATISTICS
------------------------------------------------------------
Average Duration: 2m 45s
Minimum Duration: 1m 12s
Maximum Duration: 5m 32s
Total Test Time: 2h 34m

============================================================
FAILED TESTS
------------------------------------------------------------
- Test 3 (autoDrop @ 1kn, 3m): Target not reached: 13.4m vs 15.0m
- Test 17 (autoDrop @ 1kn, 5m): Excessive negative slack events: 15
- Test 42 (autoRetrieve @ 25kn, 8m): Free fall detected: 3 occurrences
- Test 55 (autoDrop @ 20kn, 12m): Test did not complete

============================================================
```

## Error Recovery

### Serial Disconnect
- The orchestrator will exit gracefully
- All completed tests are saved
- Summary is generated for partial results
- Symlink points to incomplete run

### Analysis Failures
- Individual test analysis failures don't stop orchestration
- Analysis errors are logged but monitoring continues
- Summary generation uses available analysis files

### Graceful Shutdown
- Press Ctrl+C to stop
- Current test log is saved
- All completed analyses finish
- Final summary is generated
- Shows where to resume

## Resuming Interrupted Runs

If a test run is interrupted:

1. **Identify last completed test**:
   ```bash
   ls -lt logs/test-runs/latest/*.log | head -1
   ```

2. **Determine next test number**:
   - Check the filename of the last log
   - Next test = last test number + 1

3. **Manual continuation**:
   - The test simulator will send all 56 test notifications
   - The orchestrator captures them sequentially
   - Missing tests will have gaps in the sequence

4. **Analysis of partial run**:
   ```bash
   ./scripts/generate-test-summary.sh logs/test-runs/latest/
   ```

## Tips and Best Practices

1. **Start orchestrator before tests**: Always start the orchestrator first, then trigger the test sequence.

2. **Monitor in real-time**: Use `tail -f` on summary.txt to watch progress.

3. **Don't interrupt**: Let the full 56-test sequence complete if possible.

4. **Review failures immediately**: Check `.analysis.txt` files for failed tests.

5. **Archive successful runs**: Move or copy successful run directories for comparison.

6. **Check disk space**: 56 tests can generate significant log data (estimate ~100MB).

## Troubleshooting

### "No serial port found"
- Specify port manually: `./orchestrate-tests.sh --port /dev/cu.usbserial-XXX`
- Check device is connected: `ls /dev/cu.usbserial-*`

### "Permission denied" on serial port
- Add user to dialout group (Linux)
- On macOS, ports should be accessible by default

### Tests not detected
- Verify test notifications appear in serial output
- Check baud rate matches device: `--baud 115200`

### Analysis not running
- Verify analyze-test-result.sh is executable: `chmod +x scripts/*.sh`
- Check bc is installed: `which bc`

### Summary shows 0 tests
- No analysis files generated yet (tests still running)
- Wait for at least one test to complete

## Performance Considerations

- **Memory**: Logs are streamed to disk, minimal RAM usage
- **CPU**: Analysis runs in background, minimal impact on monitoring
- **Disk I/O**: Each test generates ~1-5MB of logs
- **Total time**: Expect 2-4 hours for full 56-test sequence (depending on test durations)

## Version History

- **v1.0** (2024-12-06): Initial orchestration system
  - Automatic test detection
  - Per-test log capture
  - Real-time analysis
  - Running summary generation
  - Graceful shutdown support

---

For questions or issues, refer to the main project README or CLAUDE.md.
