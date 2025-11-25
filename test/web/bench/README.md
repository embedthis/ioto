# Web Server Benchmark Suite

Performance benchmark suite for the embedded web server, measuring throughput, latency, and identifying regressions across releases.

## Quick Start

```bash
# From the module root
make test

# Or directly from test/bench directory
cd test/bench
tm bench.tst.c

# Quick validation (1 second total)
tm --duration 1 bench

# Standard benchmarks (30 seconds)
tm --duration 30 bench

# Release benchmarks (5 minutes)
tm --duration 300 bench
```

## What Gets Measured

The benchmark suite measures seven key performance areas:

### 1. Static File Serving
- **1KB, 10KB, 100KB, 1MB files** across different cache states
- **URL Library tests**: Standard HTTP client (includes client overhead)
- **Raw HTTP/HTTPS tests**: Direct socket I/O (shows true server performance)
- **Metrics**: Requests/sec, latency (avg, p95, p99, min, max), throughput (MB/s)

### 2. File Uploads
- **1KB, 10KB, 100KB uploads** via PUT requests
- **Warm vs cold connection** comparison
- **Metrics**: Uploads/sec, latency, throughput

### 3. Action Routes
- **Simple actions**: Minimal processing overhead
- **JSON responses**: Action handler with JSON output
- **Metrics**: Actions/sec, latency

### 4. Authentication
- **Digest authentication** (SHA-256) with session reuse
- **Cold authentication**: No session caching
- **Metrics**: Auth requests/sec, overhead vs non-auth

### 5. HTTPS Performance
- **TLS handshakes**: Full connection establishment
- **Session reuse**: Cached TLS sessions
- **Metrics**: Handshakes/sec, overhead vs HTTP

### 6. Raw Protocol Performance
- **Direct socket I/O**: Bypasses URL library
- **HTTP and HTTPS**: Both warm and cold connection states
- **Metrics**: Maximum server throughput without client overhead

## Understanding the Results

### Result Files

Results are automatically saved to:
- **doc/benchmarks/latest.json5** - Machine-readable JSON5 format
- **doc/benchmarks/latest.md** - Human-readable markdown table

### Key Metrics

| Metric | Description | Interpretation |
|--------|-------------|----------------|
| **Req/Sec** | Requests per second | Higher is better; main throughput metric |
| **Avg Latency** | Average response time (ms) | Lower is better; typical user experience |
| **P95 Latency** | 95th percentile (ms) | Lower is better; most users see this or better |
| **P99 Latency** | 99th percentile (ms) | Lower is better; worst case for most requests |
| **Min/Max** | Fastest/slowest request (ms) | Shows range; max may include outliers |
| **Bytes** | Total data transferred | Validates test execution |
| **Errors** | Failed requests | Should be 0 or very low (<0.1%) |
| **Iterations** | Completed requests | More iterations = better statistics |

### Timing Resolution

- **Precision**: Millisecond (using `rTicks()`)
- **Fast requests**: May show 0ms (below timing resolution)
- **Statistical smoothing**: Multiple iterations provide meaningful aggregates
- **Relative comparison**: Valid for regression detection even with sub-ms requests

### Warm vs Cold Tests

- **Warm**: Reuses connections/sockets (simulates persistent connections)
- **Cold**: New connection for each request (simulates new clients)
- **Raw tests**: Direct socket I/O, shows maximum server capability
- **URL Library tests**: Includes client overhead, closer to real-world usage

## Duration Configuration

The benchmark suite uses **time-based execution** controlled by the `--duration` flag (which sets the `TESTME_DURATION` environment variable).

### Default Behavior

```bash
tm bench  # 60 seconds total (6s soak + 54s benchmark)
```

### Custom Durations

```bash
# Quick smoke test (5 seconds)
tm --duration 5 bench

# Standard benchmarks (30 seconds)
tm --duration 30 bench

# Comprehensive (5 minutes)
tm --duration 300 bench
```

### Duration Allocation

- **10% for soak phase**: Warm up all code paths
- **90% for benchmarks**: Full measurement with statistics
- **Per-group allocation**: Benchmark time divided across test groups
- **File size multipliers**: Large files (100KB, 1MB) use 25% of base duration

### Benefits of Time-Based Testing

- **Predictable execution time**: Suite completes in known time
- **Adaptive iterations**: Faster servers run more iterations
- **Fair comparisons**: All configurations get same time budget
- **Flexible tuning**: Quick validation (1s) to comprehensive (5m)

## Test Execution Flow

### Phase 1: Preparation (prep.sh)
- Creates `site/` directory with test files
- Generates files: 1KB, 10KB, 100KB, 1MB
- Creates authentication test files

### Phase 2: Setup (setup.sh)
- Starts web server on ports 4260 (HTTP), 4261 (HTTPS)
- Waits for health check to pass
- Saves server PID for monitoring

### Phase 3: Soak Phase (C code)
- **One complete sweep** of all benchmark groups
- Warms up all server code paths
- Stabilizes memory allocations
- Primes caches (file cache, digest cache, TLS sessions)
- Duration: 10% of specified duration (minimum 100ms)

### Phase 4: Benchmark Phase (C code)
- Runs each test group sequentially
- Records detailed timing statistics
- Duration: 90% of specified duration
- Each group gets allocated portion of total time

### Phase 5: Analysis & Save
- Calculates statistics (min/max/avg/p95/p99)
- Computes requests per second
- Generates JSON and markdown results
- Saves to `doc/benchmarks/`

### Phase 6: Cleanup (cleanup.sh)
- Stops web server gracefully
- Removes temporary files
- Preserves results on failure for debugging

## Interpreting Results

### Expected Performance Ranges

Based on typical embedded system performance:

| Test | Expected Req/Sec | Notes |
|------|------------------|-------|
| 1KB static | 4,000 - 15,000 | Depends on cache state |
| 10KB static | 3,000 - 12,000 | Network/disk I/O impact |
| 100KB static | 2,000 - 8,000 | Larger transfer overhead |
| 1MB static | 500 - 1,500 | Disk I/O dominant |
| Uploads | 1,500 - 4,000 | Write overhead |
| Actions | 4,000 - 6,000 | CPU bound |
| Auth | 2,000 - 4,000 | Crypto overhead |
| HTTPS handshake | 400 - 600 | TLS negotiation cost |
| HTTPS reuse | 800 - 1,200 | Cached session benefit |

### Performance Factors

- **Platform**: macOS, Linux, Windows performance varies
- **Build type**: Release builds ~2-3x faster than debug
- **TLS library**: OpenSSL vs MbedTLS performance differences
- **Hardware**: CPU speed, memory, disk I/O
- **System load**: Background processes affect results

### What's Normal

- **Sub-millisecond latencies**: Fast requests may show 0ms
- **P99 higher than P95**: Tail latencies from outliers
- **Occasional errors in raw tests**: ~0.1% acceptable for connection reuse edge cases
- **Warm vs cold differences**: 2-4x throughput difference is normal

### What's Concerning

- **High error rates**: >1% indicates problems
- **Increasing latencies**: P95/P99 growing over time
- **Throughput regressions**: >10% decrease vs baseline
- **Memory growth**: Check with leak test suite

## Regression Detection

### Baseline Management

#### Creating a Baseline

After confirming good results, save as version baseline:

```bash
# Run comprehensive benchmarks
cd test/bench
tm --duration 300000 bench

# Review results
cat ../../doc/benchmarks/latest.md

# Save as version baseline
cp ../../doc/benchmarks/latest.json5 ../../doc/benchmarks/v1.0.0.json5
cp ../../doc/benchmarks/latest.md ../../doc/benchmarks/v1.0.0.md
```

#### Comparing Against Baseline

Manual comparison workflow:

```bash
# Run current benchmarks
tm --duration 30000 bench

# Compare key metrics
diff ../../doc/benchmarks/v1.0.0.md ../../doc/benchmarks/latest.md
```

#### Comparison Criteria

- **Regression**: >10% slower (req/sec) or higher latency
- **Improvement**: >10% faster or lower latency
- **Stable**: Within ±10% range
- **Acceptable variance**: ±5% due to system noise

### Release Workflow

```bash
# 1. Run release benchmarks (5 minutes)
cd test/bench
tm --duration 300 bench

# 2. Review results
cat ../../doc/benchmarks/latest.md

# 3. Compare against previous release
diff ../../doc/benchmarks/v1.0.0.md ../../doc/benchmarks/latest.md

# 4. If acceptable, save as new baseline
cp ../../doc/benchmarks/latest.json5 ../../doc/benchmarks/v1.1.0.json5
cp ../../doc/benchmarks/latest.md ../../doc/benchmarks/v1.1.0.md

# 5. Update release notes with performance changes
# Document any significant improvements or regressions
```

## Configuration Files

### testme.json5

Located at `test/bench/testme.json5`:

```json5
{
    enable: 'manual',  // Prevent accidental runs during normal testing
    execution: {
        timeout: 1800,  // 30 minutes max
    },
    services: {
        prep: './prep.sh',      // Create test files
        setup: './setup.sh',    // Start server
        cleanup: './cleanup.sh', // Stop server
        healthcheck: { url: 'http://localhost:4260/index.html' },
    },
}
```

### web.json5

Web server configuration at `test/bench/web.json5`:

**Key settings for benchmarking:**
- Minimal logging (`types: 'error'`)
- Dedicated ports (4260/4261)
- Authentication configured for digest tests
- WebSocket support enabled
- Upload directory configured

## Troubleshooting

### Server Won't Start

```bash
# Check if port is already in use
lsof -i :4260

# Kill any existing web server
pkill -f "web.*4260"

# Try manual startup
cd test/bench
./setup.sh
```

### Results Missing

Check these locations:
- `doc/benchmarks/latest.json5` - Machine-readable
- `doc/benchmarks/latest.md` - Human-readable
- `test/bench/web.log` - Server logs (errors only)

### High Error Rates

Possible causes:
- Server not fully started (increase setup delay)
- Timeout too short (increase `URL_TIMEOUT_MS`)
- System overloaded (reduce duration or close other apps)
- Server crash (check `web.log`)

### Inconsistent Results

Tips for reproducibility:
- Close other applications
- Run multiple times and average
- Use longer duration (300s for release)
- Test on same hardware/platform
- Use same build profile (release vs debug)
- Disable CPU frequency scaling

### Benchmark Crashes

Debug steps:
```bash
# Check server log
cat web.log

# Run under debugger (macOS)
lldb -- ../../build/macosx-dev/bin/tm bench

# Check for memory issues
valgrind tm bench
```

## Understanding Test Groups

### Static Files (URL Library)
- Uses `url` HTTP client library
- Tests 1KB, 10KB, 100KB, 1MB files
- Warm (connection reuse) and cold (new connections)
- Represents typical HTTP client behavior

### Static Files (Raw Protocol)
- Direct socket I/O bypassing URL library
- Shows maximum server capability
- HTTP and HTTPS variants
- Useful for identifying client vs server bottlenecks

### Uploads
- Tests PUT requests with file upload
- Various sizes: 1KB, 10KB, 100KB
- Warm vs cold connection comparison
- Tests upload directory handling

### Actions
- Invokes C action handlers
- Simple action (minimal overhead)
- JSON action (structured response)
- CPU-bound performance

### Authentication
- Digest authentication (SHA-256)
- Session reuse (cached credentials)
- Cold auth (full challenge-response)
- Measures crypto overhead

### HTTPS
- TLS handshake performance (cold)
- Session reuse (cached TLS sessions)
- Shows TLS overhead vs HTTP
- OpenSSL or MbedTLS performance

## Advanced Usage

### Environment Variables

```bash
# Set custom build path
export PATH="../../build/custom/bin:$PATH"

# Enable verbose logging (edit web.json5: types: 'info,trace')
```

**Note**: Use the `--duration` flag (value in seconds) instead of setting `TESTME_DURATION` directly:
```bash
tm --duration 30 bench  # Preferred (30 seconds)
```

### Running Specific Tests

The benchmark suite runs all tests in sequence. To run subsets:

```bash
# Edit bench.tst.c and comment out test functions in fiberMain()
# Then rebuild and run
```

### Custom File Sizes

Edit `bench.tst.c` and modify `fileClasses` array:

```c
static FileClass fileClasses[] = {
    { "500B",  "static/500B.txt",  512,     1.0 },
    { "5KB",   "static/5K.txt",    5120,    1.0 },
    // ...
};
```

Then update `prep.sh` to generate the corresponding files.

## Performance Optimization Tips

### For Faster Benchmarks

1. **Use release builds**: 2-3x faster than debug
   ```bash
   make PROFILE=release
   ```

2. **Reduce duration**: For quick validation
   ```bash
   tm --duration 5 bench
   ```

3. **Run on idle system**: Close other applications

### For More Accurate Results

1. **Increase duration**: More iterations = better statistics
   ```bash
   tm --duration 300 bench
   ```

2. **Multiple runs**: Average results across 3-5 runs

3. **Consistent environment**:
   - Same hardware
   - Same OS state (reboot if needed)
   - Same background load

## CI/CD Integration

### Automated Performance Testing

Example GitHub Actions workflow:

```yaml
name: Performance Benchmarks
on: [push, pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: make PROFILE=release
      - name: Run Benchmarks
        run: |
          cd test/bench
          tm --duration 30 bench
      - name: Upload Results
        uses: actions/upload-artifact@v2
        with:
          name: benchmark-results
          path: doc/benchmarks/latest.*
```

### Performance Gates

Fail builds on regressions:

```bash
# Compare against baseline and check for >10% degradation
# (requires custom comparison script)
./scripts/compare-benchmarks.sh v1.0.0 latest --threshold 10
```

## Files and Directories

```
test/bench/
├── README.md              # This file
├── testme.json5           # TestMe configuration
├── web.json5              # Web server configuration
├── prep.sh                # Prepare test environment
├── setup.sh               # Start web server
├── cleanup.sh             # Stop server, cleanup
├── bench.tst.c            # Main benchmark implementation
├── bench-utils.h          # Utility function headers
├── bench-utils.c          # Utility function implementation
├── bench-test.h           # Test framework integration
└── site/                  # Test files (generated by prep.sh)
    ├── index.html
    ├── static/
    │   ├── 1K.txt
    │   ├── 10K.txt
    │   ├── 100KB.txt
    │   └── 1M.txt
    ├── auth/
    │   └── secret.html
    └── test.json

doc/benchmarks/
├── latest.json5           # Latest results (machine-readable)
├── latest.md              # Latest results (human-readable)
├── v1.0.0.json5          # Baseline for version 1.0.0
└── v1.0.0.md             # Baseline markdown
```

## Additional Resources

- **Implementation Plan**: `AI/plans/benchmark-suite.md`
- **Web Module Design**: `AI/designs/DESIGN.md`
- **Project Roadmap**: `AI/plans/ROADMAP.md`
- **TestMe Documentation**: `../README-TESTME.md`

## Notes

- **Manual execution**: Tests marked `enable: 'manual'` to prevent accidental runs
- **Platform support**: macOS, Linux, Windows (WSL or native)
- **TLS stacks**: Works with OpenSSL or MbedTLS
- **Reproducibility**: Use consistent `--duration` value across runs
- **CI-friendly**: Quick validation in 1-5 seconds
- **Release-ready**: Comprehensive in 5 minutes

## Contributing

When adding new benchmarks:

1. Add test function following existing pattern
2. Update `fiberMain()` to call new test
3. Add results to JSON output structure
4. Update this README with new test description
5. Run comprehensive benchmarks to establish baseline
6. Update `AI/plans/benchmark-suite.md` status

---

**Last Updated**: 2025-11-12
**Status**: Production-ready, actively maintained
