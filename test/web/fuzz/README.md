# Web Server Fuzz Testing

Comprehensive automated fuzz testing for the web server module, designed to discover security vulnerabilities, protocol violations, and edge case handling issues.

## Quick Start

```bash
# Run all fuzz tests (requires web server running)
cd test/fuzz
tm

# Run specific fuzzer
tm http-proto
tm tls-proto
tm url-path

# Run with verbose output
tm -v

# Run with custom iterations
tm -i 50000

# Run with a duration
tm --duration 300 http-proto
```

## Overview

The fuzzing framework currently provides:

- **Protocol Fuzzing** - HTTP request line parsing and validation (implemented: `http.tst.c`)
- **Input Validation** - URL path, query parameter, and fragment validation (implemented: `url.tst.c`)
- **TLS/HTTPS Fuzzing** - HTTPS requests and TLS-specific edge cases (implemented: `tls.tst.c`)

Additional fuzzing categories are planned for future implementation (see [AI/plans/fuzz-testing.md](../../AI/plans/fuzz-testing.md) for the complete roadmap).

## Architecture

```
Fuzz Tests (*.c)
    ↓
Fuzz Library (fuzz.h/c)
    ↓
TestMe Orchestration (testme.json5)
    ↓
Web Server (dedicated fuzz ports)
```

### Build Artifacts

All intermediate objects and executables are placed under the `.testme` directory:

```
.testme/
├── http              # HTTP protocol fuzzer executable
├── url               # URL Path validation fuzzer executable
└── ...               # Other fuzzer artifacts
```

This keeps the source directory clean and follows the standard TestMe convention. The fuzz.c library is included by the tests via a #include "fuzz.c" directive.

### Source Files

```
fuzz.c, fuzz.h              # Fuzzing library (core framework)
http.tst.c                  # HTTP protocol fuzzer (implemented)
url.tst.c                   # URL path validation fuzzer (implemented)
tls.tst.c                   # TLS/HTTPS protocol fuzzer (implemented)
setup.sh, cleanup.sh        # Environment management scripts
web.json5                   # Web server configuration for fuzzing
testme.json5                # TestMe fuzzing configuration
```

## Implemented Fuzzers

### Protocol Fuzzing

**HTTP Request Fuzzer** (`http.tst.c`):
- Tests HTTP request line parsing (method, URI, version)
- Mutation-based fuzzing with 1,000 iterations (default)
- Uses seed corpus from `corpus/http-requests.txt`

**Example**:
```bash
tm http
```

### Input Validation Fuzzing

**URL Path Fuzzer** (`url.tst.c`):
- Tests URL path validation, query parameters, and fragments
- Path traversal attacks, encoding edge cases
- Mutation-based fuzzing with 20,000 iterations (default)
- Uses seed corpus from `corpus/url-paths.txt`

**Example**:
```bash
tm url
```

### TLS/HTTPS Fuzzing

**TLS Protocol Fuzzer** (`tls.tst.c`):
- Tests HTTPS requests with TLS-specific edge cases
- SNI hostname validation and certificate handling
- Host header mutations for SNI mismatch testing
- TLS-specific security header handling
- Mutation-based fuzzing with 1,000 iterations (default)
- Uses seed corpus from `corpus/tls-requests.txt`

**Example**:
```bash
tm tls
```

## Planned Fuzzers

See [AI/plans/fuzz-testing.md](../../AI/plans/fuzz-testing.md) for the complete implementation roadmap including:

- **Additional Protocol Fuzzers**: Headers, chunked encoding, methods
- **Extended Input Validation**: Query strings, form data, JSON, cookies, encoding
- **Resource Exhaustion**: Connection flooding, slowloris, memory exhaustion
- **Security Testing**: Authentication, sessions, XSRF, role-based access
- **Specialized Fuzzing**: TLS/SSL, WebSocket, file uploads, API signatures

## Configuration

Fuzzing is configured via `testme.json5`:

```json5
{
    compiler: {
        c: {
            compiler: 'gcc',
            flags: [
                '-fsanitize=address,undefined',  // Enable sanitizers
                '-fno-omit-frame-pointer',
                '-g', '-O1',
                // ... other flags
            ],
        },
    },

    execution: {
        timeout: 300,        // 5 min per fuzzer
        parallel: true,
        worker: 1,
    },

    environment: {
        ASAN_OPTIONS: 'detect_leaks=1:abort_on_error=1',
        UBSAN_OPTIONS: 'print_stacktrace=1:halt_on_error=1',
    },
}
```

## Environment Variables

- `TESTME_ITERATIONS` - Number of iterations per fuzzer (default: 1000 for http, 20000 for url)
- `TESTME_VERBOSE` - Enable verbose output (set by TestMe via `tm -v`)
- `TESTME_STOP` - Stop on first error (1) or continue (0, default)
- `FUZZ_REPLAY` - Path to crash file for replay mode (skips corpus, runs once)
- `ASAN_OPTIONS` - Address Sanitizer options
- `UBSAN_OPTIONS` - Undefined Behavior Sanitizer options

**Examples:**
```bash
# Stop on first crash for debugging
tm --stop http

# Run 100k iterations and continue through all errors
tm --iterations 100000 --stop 0 url

# Verbose mode with stop on first error
tm -v --stop http

# Replay a crash file
FUZZ_REPLAY=crashes/http/crash-DEADBEEF.txt tm http
```

Server endpoints are configured in `web.json5`:
- HTTP: http://localhost:4200
- HTTPS: https://localhost:4243

## Corpus Management

### Seed Corpus

Initial test inputs are stored in `corpus/`:

```
corpus/
├── http-requests.txt      # HTTP protocol requests and edge cases
└── url-paths.txt          # URL paths, query parameters, and fragments
```

See [AI/plans/fuzz-testing.md](../../AI/plans/fuzz-testing.md) for planned corpus files for future fuzzers.

### Crash Corpus

Crash-inducing inputs are saved to `crashes/`:

```
crashes/
├── http/                  # Client-side crashes in HTTP fuzzer
│   ├── crash-DEADBEEF.txt
│   └── crash-CAFEBABE.txt
├── url/                   # Client-side crashes in URL fuzzer
│   └── crash-12345678.txt
├── server/                # Server-side crashes (web server)
│   ├── crash-abc123.txt   # Crash-inducing input
│   ├── crash-def456.log   # Web server log at crash
│   ├── asan.12345         # ASAN reports
│   ├── ubsan.12345        # UBSAN reports
│   └── core.12345         # Core dumps
└── crashes-archive/
    └── crashes-20250104-143022.tar.gz
```

**Client Crash Files** (crashes/http/, crashes/url/):
Each crash file includes:
- Input that caused the crash
- Signal/error information
- Stack trace (if available)
- Timestamp

**Server Crash Files** (crashes/server/):
Server crashes are detected during fuzzing and saved with:
- Crash-inducing input that killed the server
- Web server log at time of crash
- ASAN/UBSAN sanitizer reports
- Core dumps (if enabled)

## Writing New Fuzzers

### 1. Create Fuzzer File

```c
// newtest.tst.c
#include "../test.h"
#include "fuzz.h"
#include "fuzz.c"

#define CORPUS_FILE "corpus/newtest.txt"
#define CRASH_DIR   "crashes/newtest"

static cchar      *TARGET_URL;
static FuzzRunner *runner;
static int        fuzzResult = 0;
static bool       serverCrashed = 0;
static cchar      *currentFuzzInput = NULL;
static size_t     currentFuzzLen = 0;

static void fuzzFiber(void *arg);
static bool testOracle(cchar *input, size_t len);
static char *mutateInput(cchar *input, size_t *len);
static bool shouldStopFuzzing(void);

int main(int argc, char **argv)
{
    int iterations = getenv("TESTME_ITERATIONS") ? atoi(getenv("TESTME_ITERATIONS")) : 10000;

    FuzzConfig config = {
        .iterations = iterations,
        .timeout = 5000,
        .crashDir = CRASH_DIR,
        .verbose = getenv("TESTME_VERBOSE") != NULL,
        .stop = getenv("TESTME_STOP") && atoi(getenv("TESTME_STOP")) == 1,
    };

    // Setup test environment (starts web server)
    if (!setup((char**) &TARGET_URL, NULL)) {
        return 1;
    }

    // Run fuzzing in fiber
    rStartFiber(fuzzFiber, &config);
    rServiceEvents(0, -1);

    return fuzzResult > 0 ? 1 : 0;
}

static void fuzzFiber(void *arg)
{
    FuzzConfig *config = (FuzzConfig*) arg;
    cchar *replayFile = getenv("FUZZ_REPLAY");

    // Check for replay mode
    if (replayFile) {
        rPrintf("Replaying crash file: %s\n", replayFile);
        runner = fuzzInit(config);
        fuzzSetOracle(runner, testOracle);

        int loaded = fuzzLoadCorpus(runner, replayFile);
        if (loaded == 0) {
            rPrintf("✗ Failed to load crash file: %s\n", replayFile);
            fuzzResult = -1;
            rStop();
            return;
        }

        config->iterations = 1;  // Run once, no mutations
    } else {
        // Normal fuzzing mode
        rPrintf("Starting newtest fuzzer\n");
        rPrintf("Target: %s\n", TARGET_URL);
        rPrintf("Iterations: %d\n", config->iterations);

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testOracle);
        fuzzSetMutator(runner, mutateInput);
        fuzzSetShouldStopCallback(shouldStopFuzzing);

        // Load corpus
        fuzzLoadCorpus(runner, CORPUS_FILE);
    }

    int crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);
    fuzzResult = crashes;
    rStop();
}

static bool testOracle(cchar *input, size_t len)
{
    currentFuzzInput = input;
    currentFuzzLen = len;

    // Test the input against the web server
    // Return true if passed, false if crash/vulnerability found

    // Check if server is still alive
    if (!fuzzIsServerAlive(fuzzGetServerPid())) {
        tinfo("Server crashed during fuzzing");
        fuzzReportServerCrash(input, len);
        serverCrashed = 1;
        return false;
    }

    return true;
}

static char *mutateInput(cchar *input, size_t *len)
{
    // Apply mutations to input
    int strategy = rRandom() % 10;

    switch (strategy) {
        case 0: return fuzzBitFlip(input, len);
        case 1: return fuzzByteFlip(input, len);
        case 2: return fuzzInsertRandom(input, len);
        case 3: return fuzzDeleteRandom(input, len);
        case 4: return fuzzInsertSpecial(input, len);
        case 5: return fuzzTruncate(input, len);
        case 6: return fuzzDuplicate(input, len);
        default: return fuzzBitFlip(input, len);
    }
}

static bool shouldStopFuzzing(void)
{
    return serverCrashed;
}
```

### 2. Add to TestMe

The fuzzer will be automatically discovered if it matches the naming pattern in testme.json5.

### 3. Create Corpus File

Create `corpus/newtest.txt` with seed inputs:
```
valid input 1
valid input 2
edge case input
```

### 4. Run Fuzzer

```bash
cd test/fuzz
tm newtest
```

## Mutation Strategies

The fuzz library provides mutation functions:

```c
// Bit-level mutations
char *fuzzBitFlip(cchar *input, ssize *len);
char *fuzzByteFlip(cchar *input, ssize *len);

// Structural mutations
char *fuzzInsertRandom(cchar *input, ssize *len);
char *fuzzDeleteRandom(cchar *input, ssize *len);
char *fuzzTruncate(cchar *input, ssize *len);
char *fuzzDuplicate(cchar *input, ssize *len);

// Content mutations
char *fuzzInsertSpecial(cchar *input, ssize *len);
char *fuzzReplace(cchar *input, ssize *len, cchar *pattern, cchar *replacement);
char *fuzzOverwriteRandom(cchar *input, ssize *len);

// Generation
char *fuzzRandomString(ssize len);
char *fuzzRandomData(ssize len);
```

## Sanitizers

Sanitizers are configured in `testme.json5`. To temporarily override:

```bash
# Address Sanitizer - detects memory corruption, buffer overflows, use-after-free
CFLAGS="-fsanitize=address" tm

# Undefined Behavior Sanitizer - detects undefined behavior, integer overflow
CFLAGS="-fsanitize=undefined" tm

# Combined (recommended)
CFLAGS="-fsanitize=address,undefined" tm
```

## CI/CD Integration

### GitHub Actions

```yaml
name: Fuzz Testing
on:
  schedule:
    - cron: '0 2 * * *'  # Nightly

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Run fuzz tests
        run: cd test/fuzz && tm -i 50000
      - name: Upload crashes
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: fuzz-crashes
          path: test/fuzz/crashes/
```

## Analyzing Crashes

### 1. Reproduce Crash

```bash
# Find crash files
ls crashes/http/

# Replay crash with FUZZ_REPLAY
FUZZ_REPLAY=crashes/http/crash-DEADBEEF.txt tm http

# Or manually:
cd test/fuzz
FUZZ_REPLAY=crashes/http/crash-DEADBEEF.txt .testme/http
```

### 2. Debug with GDB

```bash
# Set FUZZ_REPLAY and run with debugger
FUZZ_REPLAY=crashes/http/crash-DEADBEEF.txt gdb .testme/http
(gdb) run
(gdb) bt
(gdb) info locals
```

### 3. Minimize Input

Use delta debugging to find minimal reproducer:

```bash
# Manual minimization - edit crash file and replay
vi crashes/http/crash-DEADBEEF.txt
FUZZ_REPLAY=crashes/http/crash-DEADBEEF.txt tm http
```

### 4. Fix and Verify

After fixing the issue:

```bash
# Re-run fuzzer with crash input
FUZZ_REPLAY=crashes/http/crash-DEADBEEF.txt tm http

# Should not crash anymore
```

## Performance Tuning

### Parallel Execution

Run multiple fuzzers concurrently:

```json5
{
    execution: {
        parallel: true,
        worker: 8,  // Run 8 fuzzers in parallel
    }
}
```

### Fuzzing Iterations

Adjust based on CI/CD budget:

```bash
# Quick smoke test (CI on every commit)
tm -i 1000

# Normal testing (CI nightly)
tm -i 50000

# Intensive testing (manual/weekly)
tm -i 10000000
```

## Best Practices

1. **Start with Corpus** - Always seed with valid inputs
2. **Enable Sanitizers** - Use ASan/UBSan to catch issues
3. **Set Timeouts** - Prevent infinite loops
4. **Deduplicate Crashes** - Use hashing to find unique issues
5. **Minimize Reproducers** - Reduce crash inputs to minimal size
6. **Continuous Fuzzing** - Run in CI/CD nightly
7. **Triage Quickly** - Fix crashes as they're found
8. **Update Corpus** - Add new test cases from production

## Troubleshooting

### Fuzzer Won't Connect

```bash
# Check if web server is running
curl http://localhost:4200/

# Check fuzz ports
netstat -an | grep 4200

# Restart setup
cd test/fuzz
../setup.sh
```

### No Crashes Found

Good! But verify:

```bash
# Try more iterations
tm -i 100000 http

# Try different mutation strategies
```

### Too Many Crashes

```bash
# Deduplicate
cd crashes/http
for f in crash-*; do
    md5sum "$f"
done | sort | uniq -d -w 32

# Analyze most common
ls -lS crashes/http/ | head -10
```

## References

- [../../AI/plans/fuzz-testing.md](../../AI/plans/fuzz-testing.md) - Complete fuzzing plan, implementation guide, and task tracking
- [../../AI/archive/plans/server-crash-detection.md](../../AI/archive/plans/server-crash-detection.md) - Server crash detection implementation (completed)
- [../../AI/logs/CHANGELOG.md](../../AI/logs/CHANGELOG.md) - Change history
- [fuzz.h](fuzz.h) - Fuzzing API documentation
- [OWASP Fuzzing Guide](https://owasp.org/www-community/Fuzzing)
- [libFuzzer Tutorial](https://llvm.org/docs/LibFuzzer.html)
- [AFL++ Documentation](https://aflplus.plus/)

## Support

For issues or questions:
1. Check existing crashes in `crashes/`
2. Review fuzzer logs in `web.log`
3. Enable verbose mode: `tm -v`
4. Consult [../../AI/plans/fuzz-testing.md](../../AI/plans/fuzz-testing.md) for architecture and implementation status
