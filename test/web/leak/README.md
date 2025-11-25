# Web Server Memory Leak Test

This directory contains a comprehensive memory leak detection test for the web server.

## Overview

The leak test monitors the web server's memory usage over time while running various request patterns to detect memory leaks. It samples memory at regular intervals and checks for continuous growth beyond acceptable thresholds.

## Test Configuration

- **Soak time**: 30 seconds (initial stabilization period)
- **Test duration**: 180 seconds (3 minutes of load testing)
- **Sample interval**: 5 seconds
- **Max growth threshold**: 10%
- **Platforms**: macOS and Linux only (not Windows)

## Test Pattern

The test exercises the following request classes in rotation:

1. **Static file requests** - Various file sizes (1K, 10K, 100K)
2. **HTTPS requests** - TLS encrypted traffic
3. **Authenticated requests** - Digest authentication
4. **Upload requests** - File uploads and deletions
5. **Mixed requests** - Combination of HTTP/HTTPS/auth
6. **Large file requests** - 1MB files
7. **Concurrent requests** - Parallel request handling
8. **Error requests** - 404s and auth failures

## Running the Test

Since this test is configured with `enable: 'manual'`, it must be explicitly invoked:

```bash
cd test/leak
tm leak.tst.sh
```

Or from the web module root:

```bash
cd test/leak && tm leak.tst.sh
```

### Testing Specific Request Classes

To test a specific request class only (useful for isolating leaks):

```bash
# Test error requests only
TESTME_CLASS=error tm --duration 60 leak.tst.sh

# Test concurrent requests only
TESTME_CLASS=concurrent tm --duration 60 leak.tst.sh
```

Valid `TESTME_CLASS` values: `static`, `https`, `auth`, `upload`, `mixed`, `large`, `concurrent`, `error`, `range`

### Customizing Test Duration

Use `tm --duration N` to set test duration in seconds (does not include 30s soak-in):

```bash
# Run 60 second test (90s total with soak-in)
tm --duration 60 leak.tst.sh

# Run 300 second test (330s total with soak-in)
tm --duration 300 leak.tst.sh
```

## Test Phases

### Phase 1: Soak-in (30 seconds)
The server is allowed to run without load to stabilize memory allocation. This accounts for initial memory allocation patterns and JIT optimizations.

### Phase 2: Load Testing (180 seconds)
Various request patterns are executed while memory is sampled every 5 seconds. Each test cycle exercises different request classes.

### Phase 3: Analysis
Memory samples are analyzed to calculate:
- Baseline memory (after soak-in)
- Minimum/maximum/average memory
- Final memory
- Growth percentage
- Growth trend (linear regression)

## Pass/Fail Criteria

The test **FAILS** if:
- Memory growth exceeds 10% from baseline
- This indicates a potential memory leak

The test **PASSES** if:
- Memory growth is within 10% threshold
- A warning is shown if continuous growth is detected (slope > 100 KB/sample)

## Output Files

- `leak-results.txt` - Test results and statistics
- `.leak-memory-samples.txt` - Raw memory samples (timestamp, KB)
- `web.log` - Web server logs (preserved on failure)
- `web.pid` - Server process ID

## Notes

- The test creates unique temporary files using `$$` to allow parallel test runs
- Memory usage can be unstable initially - hence the soak-in period
- The test is designed to detect significant leaks, not minor fluctuations
- On macOS, memory is measured using RSS (Resident Set Size)
- On Linux, memory is measured using RSS from `/proc`

## Customization

### Environment Variables

Control test behavior without editing the script:

- `TESTME_CLASS` - Test only a specific request class (static, https, auth, upload, mixed, large, concurrent, error, range)
- `TESTME_DURATION` - Override test duration in seconds (does not include soak-in time)

Example:
```bash
TESTME_CLASS=error tm --duration 60 leak.tst.sh
```

### Script Parameters

To adjust test parameters permanently, edit the variables at the top of [leak.tst.sh](leak.tst.sh):

```bash
SOAK_TIME=30             # Soak-in period (seconds)
MAX_GROWTH_PERCENT=10    # Maximum allowed growth (%)
ITERATIONS_PER_CLASS=120 # Iterations per request class
CURL_TIMEOUT=10          # Timeout for curl requests (seconds)
CURL_TIMEOUT_LARGE=30    # Timeout for large file requests (seconds)
```

## Troubleshooting

If the test fails:

1. Check `leak-results.txt` for detailed statistics
2. Review `web.log` for server errors
3. Examine `.leak-memory-samples.txt` for memory growth pattern
4. Consider increasing test duration for more samples
5. Check if a specific request class triggers growth

## Example Output

```
===========================================
Web Server Memory Leak Test
===========================================
Server PID: 12345
Soak time: 30s
Test duration: 180s
Sample interval: 5s
Max growth: 10%
===========================================

Phase 1: Soak-in period (30s)
Waiting for memory to stabilize...
  Memory: 8420 KB
  Memory: 8436 KB
  ...

Baseline memory after soak-in: 8520 KB

Phase 2: Load testing (180s)

Test cycle 1 (elapsed: 0s / 180s)
  Memory: 8520 KB
Running static file requests...
  Memory: 8556 KB
...

===========================================
Phase 3: Memory Analysis
===========================================
Samples collected: 42
Baseline memory:   8520 KB
Minimum memory:    8420 KB
Maximum memory:    8720 KB
Average memory:    8612 KB
Final memory:      8680 KB
Memory growth:     160 KB (1.88%)

PASS: Memory usage is stable
Growth: 1.88% (within 10% threshold)
```
