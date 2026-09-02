# Test Architecture

The Ioto Device Agent has a comprehensive test suite covering all modules: Safe Runtime (R), JSON, Database, MQTT, URL (HTTP client), and the Web server. Tests are organized by module under the `test/` directory. The web server tests include a leak detection suite and fuzz testing framework imported from the upstream `../web` repository.

## Prerequisites

Install the following before running tests:

- **Bun** - JavaScript runtime ([bun.sh](https://bun.sh))
- **TestMe** - Test runner: `bun install @embedthis/testme -g`
- **GCC or Clang** - C compiler
- **OpenSSL** - Development files (`libssl-dev` on Linux, `brew install openssl` on macOS)
- **Make** - GNU Make
- **Bash** - Shell for service scripts

The project must be built first (`make`) to produce `libioto.a` and the `unit` app configuration.

## Quick Start

```bash
make                     # Build the project first
make test                # Run full test suite
cd test && tm            # Alternative: run via TestMe directly
cd test && tm web        # Run one module's tests
cd test && tm web/http   # Run a single test
```

## Directory Structure

```
test/
    testme.json5             # Global TestMe configuration
    prep.sh                  # Global prep: verify unit app, copy certs
    cleanup.sh               # Global cleanup

    r/                       # Safe Runtime tests (13 tests)
        testme.json5
        buf.tst.c            # Buffer operations
        event.tst.c          # Event loop
        file.tst.c           # File I/O
        hash.tst.c           # Hash tables
        list.tst.c           # Linked lists
        mem.tst.c            # Memory allocation
        printf.tst.c         # Formatted output
        run.tst.c            # Runtime event loop
        signal.tst.c         # Signal handling
        socket.tst.c         # Socket operations
        string.tst.c         # String utilities
        thread.tst.c         # Threading
        time.tst.c           # Time functions

    json/                    # JSON library tests (28 tests)
        testme.json5
        test.h               # JSON test helpers
        array.tst.c          # Array operations
        blend.tst.c          # Object blending
        construct.tst.c      # Construction
        conversion.tst.c     # Type conversion
        edge.tst.c           # Edge cases
        format.tst.c         # Output formatting
        fuzz.tst.c           # JSON fuzzing
        getters.tst.c        # Property access
        iterate.tst.c        # Iteration
        set.tst.c            # Property mutation
        template.tst.c       # Templates
        ...                  # Additional tests
        cmd/                 # JSON CLI tool tests (.tst.sh)
        data/                # Test JSON files

    db/                      # Database tests (16 tests)
        testme.json5
        callbacks.tst.c      # Event callbacks
        create-items.tst.c   # Record creation
        data-types.tst.c     # Type handling
        enumeration.tst.c    # Query enumeration
        expire.tst.c         # Record expiration
        fields.tst.c         # Field operations
        find.tst.c           # Record lookup
        open.tst.c           # Database open/close
        pagination.tst.c     # Result pagination
        persist.tst.c        # Persistence
        schema-validation.tst.c  # Schema enforcement
        ...

    mqtt/                    # MQTT protocol tests (8 tests, manual)
        testme.json5         # enable: 'manual' (requires broker)
        alloc.tst.c          # Allocation
        connect.tst.c        # Connection handling
        publish.tst.c        # Publishing
        subscribe.tst.c      # Subscriptions
        topic.tst.c          # Topic handling
        ...

    url/                     # HTTP client tests (22 tests)
        testme.json5
        test.h               # URL test helpers
        setup.sh             # Start test web server
        web.json5             # Server config for URL tests
        auth.tst.c           # Authentication
        chunked.tst.c        # Chunked transfer
        fetch.tst.c          # URL fetching
        methods.tst.c        # HTTP methods
        post.tst.c           # POST requests
        streaming.tst.c      # Streaming
        upload.tst.c         # Multipart upload
        ...
        site/                # Test web content
        leak/                # URL client leak tests (manual)

    web/                     # Web server tests (56+ tests)
        testme.json5
        test.h               # Web test helpers
        setup.sh             # Start web server
        cleanup.sh           # Stop server, show logs on failure
        prep.sh              # Create test files (1K-10M)
        web.json5            # Server configuration
        signatures.json5     # Test signatures

        # Unit test files (.tst.c)
        http.tst.c           # HTTP protocol
        methods.tst.c        # HTTP methods
        auth-basic.tst.c     # Basic authentication
        auth-digest.tst.c    # Digest authentication
        post.tst.c           # POST requests
        upload.tst.c         # File upload
        tls.tst.c            # HTTPS/TLS
        websocket-*.tst.c    # WebSocket tests
        session.tst.c        # Session management
        security-*.tst.c     # Security tests
        ...

        site/                # Static test website
        leak/                # Memory leak detection (manual)
        fuzz/                # Fuzz testing framework (manual)
        bench/               # Performance benchmarks (manual)
        manual/              # Manual-only tests

    command/                 # CLI tests
    link/                    # Linking/embedding tests
    certs/                   # Test SSL certificates (from ../certs)
```

## TestMe Framework

TestMe is the test runner. It compiles each `.tst.c` file into a standalone executable, links it against `libioto.a`, and runs it. Shell tests (`.tst.sh`) are executed directly.

### Configuration Hierarchy

Configuration flows from the root `testme.json5` to subdirectories:

**Root config** (`test/testme.json5`):
```json5
{
    compiler: {
        c: {
            gcc: {
                flags: [
                    '-Wformat', '-Wformat-security',
                    '-I../include', '-L../build/bin',
                    '-Wl,-rpath,${CONFIGDIR}/../build/bin',
                ],
                libraries: ['ioto', 'm', 'crypto', 'ssl'],
            },
        },
    },
    environment: {
        default: { PATH: '../build/bin:../bin:${PATH}' },
    },
    execution: {
        workers: 1,
        parallel: false,
        timeout: 180,
    },
    services: {
        globalCleanup: './cleanup.sh',
        globalPrep: './prep.sh',
    },
}
```

**Module config** (e.g., `test/web/testme.json5`):
```json5
{
    inherit: ['compiler', 'environment'],
    execution: { workers: 1, parallel: false, timeout: 180 },
    services: {
        prep: './prep.sh',
        setup: './setup.sh',
        cleanup: './cleanup.sh',
        healthcheck: { url: 'http://localhost:4240/index.html' },
    },
}
```

Subdirectories inherit compiler and environment settings from the root. Each directory can define its own service lifecycle scripts and execution parameters.

### Assertion API

Tests use the TestMe assertion macros defined in `testme.h`:

| Function | Purpose |
|----------|---------|
| `ttrue(cond)` | Assert condition is true |
| `tfalse(cond)` | Assert condition is false |
| `teqi(actual, expected)` | Assert integers are equal |
| `tmatch(actual, expected)` | Assert strings match |
| `tcontains(str, substr)` | Assert string contains substring |
| `tnotnull(ptr)` | Assert pointer is not null |
| `tnull(ptr)` | Assert pointer is null |
| `tfail(fmt, ...)` | Force test failure with message |
| `tskip(reason)` | Skip test with reason |
| `tinfo(fmt, ...)` | Trace info (use instead of `printf`) |
| `tdepth()` | Get test depth level |

### Test File Structure

Each C test file is a standalone program:

```c
/*
    example.tst.c - Unit tests for feature
 */
#include "testme.h"
#include "r.h"

static void testFeature()
{
    char *buf;

    buf = rAlloc(256);
    tnotnull(buf);
    scopy(buf, 256, "hello");
    tmatch(buf, "hello");
    rFree(buf);
}

int main(void)
{
    rInit(0, 0);
    testFeature();
    rTerm();
    return 0;
}
```

Key conventions:
- Include `testme.h` for assertions
- Initialize the Safe Runtime with `rInit()` at the start
- Terminate with `rTerm()` before returning
- Use `tinfo()` for trace output, never `printf`
- Tests that need fiber coroutines use `rInit(fiberMain, 0)`

### Test Compilation

TestMe compiles each `.tst.c` file with flags from `testme.json5`:
1. Include path: `-I../include`
2. Library path: `-L../build/bin`
3. Linked against: `libioto.a`, OpenSSL (`crypto`, `ssl`), math (`m`)
4. The resulting binary is executed and assertions are captured

## Service Lifecycle

Tests that need running services (web server, etc.) use lifecycle scripts managed by TestMe:

```
prep.sh    →  Create test files, directories, certificates
setup.sh   →  Start services (web server) in background
             ← healthcheck polls endpoint until ready
             ... tests run ...
cleanup.sh →  Stop services, show logs on failure, remove temp files
```

### Global Prep (`test/prep.sh`)

Verifies the project is configured for the `unit` app and copies test certificates:

```bash
# Must be configured for unit testing
app=$(json app apps/unit/state/config/ioto.json5)
if [ "$app" != "unit" ]; then
    echo "Need unit app" && exit 1
fi
# Copy test certificates
cp ../certs/*.crt certs/ && cp ../certs/*.key certs/
```

### Web Server Setup (`test/web/setup.sh`)

Starts the web server from `web.json5` configuration:

```bash
ENDPOINT=$(json 'web.listen[0]' web.json5)
web --config web.json5 --trace web.log:all:all &
# TestMe healthcheck polls: http://localhost:4240/index.html
```

### Web Cleanup (`test/web/cleanup.sh`)

On failure, dumps the server log for diagnosis:

```bash
if [ "${TESTME_SUCCESS}" = "1" ]; then
    rm -f web.log
else
    cat web.log      # Show logs for failed tests
fi
```

## Unit Test App

The `apps/unit/` application provides server-side infrastructure for integration tests. It links against `libioto.a` and exposes test action endpoints that unit tests invoke via HTTP. Configuration resides in `apps/unit/state/config/` with `ioto.json5`, `web.json5`, `schema.json5`, and `db.json5`.

## Module Test Details

### Safe Runtime (`test/r/`)

13 tests covering the foundational R library: memory allocation, string operations, buffers, hash tables, linked lists, file I/O, sockets, event loop, signals, threading, and time functions.

### JSON (`test/json/`)

28 tests covering JSON5 parsing, object construction, array operations, property access and mutation, iteration, type conversion, templates, file I/O, formatting, and edge cases. Includes a JSON fuzzer (`fuzz.tst.c`). The `cmd/` subdirectory has shell script tests for the JSON CLI tool.

### Database (`test/db/`)

16 tests covering CRUD operations, schema validation, field operations, record expiration, pagination, callbacks, persistence, and data types. Tests use their own `db/` directory for test database files.

### MQTT (`test/mqtt/`)

8 tests covering connection, publish, subscribe, topic handling, protocol packets, and edge cases. Marked `enable: 'manual'` because they require an external MQTT broker.

### URL HTTP Client (`test/url/`)

22 tests for the HTTP client library: fetching, HTTP methods, authentication (basic and digest), chunked transfer, cookies, headers, streaming, multipart upload, and error handling. Starts its own web server via `setup.sh` for testing. Has its own `leak/` subdirectory for client-side memory leak testing.

### Web Server (`test/web/`)

56+ unit tests organized by feature:

| Category | Tests |
|----------|-------|
| HTTP Protocol | `http`, `http10`, `methods`, `methods-extended`, `headers`, `status-codes` |
| Authentication | `auth-basic`, `auth-digest`, `auth-config`, `auth-roles-array` |
| Content | `file`, `dir`, `file-serving`, `content-type`, `path` |
| Requests | `post`, `put-large`, `query`, `form`, `upload`, `upload-multipart` |
| Responses | `redirects`, `redirect`, `range`, `compressed`, `chunked` |
| WebSockets | `websocket-messaging`, `websocket-upgrade-extended` |
| SSE/Streaming | `sse`, `sse-enhanced`, `stream` |
| Sessions | `session`, `session-security` |
| Security | `tls`, `security-dos`, `security-headers`, `security-path-traversal`, `security-uri-validation`, `xsrf` |
| Caching | `cache-control`, `conditional` |
| Limits | `limits`, `validate` |
| Server | `init`, `host`, `keep-alive`, `multi-instance`, `sockets` |
| Fiber/I/O | `fiber-blocks`, `io`, `buffer`, `error-handling` |

The web test `prep.sh` creates test files of various sizes (1K, 10K, 25K, 100K, 500K, 1M, 10M) and range-test files in the `site/` directory.

## Leak Testing

Memory leak tests are located at `test/web/leak/` and `test/url/leak/`. These are imported from the upstream `../web` repository and run manually.

### Configuration

```json5
{
    enable: 'manual',
    execution: { timeout: 3600 },          // 1 hour max
    tests: {
        'leak.tst.sh': {
            platforms: ['macosx', 'linux'],  // RSS sampling not available elsewhere
        },
    },
}
```

### How It Works

The leak test (`leak.tst.sh`) monitors the web server's RSS (Resident Set Size) over time while exercising all request paths:

**Phase 1 - Soak-in**: Runs request patterns for a warm-up period (default 60s, max 300s) to allow one-time initialization allocations to settle. Memory growth during soak-in is expected and tracked separately.

**Phase 2 - Load testing**: Runs the same request patterns for the test duration (default 300s) while sampling memory at regular intervals. Records memory at 20% checkpoints for trend analysis.

**Phase 3 - Analysis**: Computes statistics and determines pass/fail. Calculates linear regression slope to detect slow steady growth patterns.

### Request Classes

The leak test exercises nine distinct request classes in rotation:

| Class | Description |
|-------|-------------|
| `static` | Static file serving (index.html, 1K, 10K, 100K) |
| `https` | HTTPS/TLS requests |
| `auth` | Digest authentication |
| `upload` | PUT file upload and DELETE |
| `mixed` | Combined HTTP, HTTPS, and auth requests |
| `large` | Large file requests (1MB) |
| `concurrent` | Parallel requests (10 concurrent) |
| `error` | 404 error responses |
| `range` | Byte range requests (single, suffix, multipart) |

### Pass/Fail Criteria

- **Maximum growth threshold**: 10% over baseline (configurable via `MAX_GROWTH_PERCENT`)
- **Continuous growth detection**: Linear regression slope > 100 KB/sample triggers a warning
- **Growth pattern analysis**: Reports whether growth is early (initialization), late (possible leak), or continuous (investigate)

### Running Leak Tests

```bash
cd test/web/leak
tm leak                                      # Full test (300s)
tm --duration 60 leak                        # Quick test (60s)
TESTME_CLASS=static tm leak                  # Test single class
TESTME_CLASS=https tm --duration 120 leak    # HTTPS-only, 2 minutes
```

Valid `TESTME_CLASS` values: `static`, `https`, `auth`, `upload`, `mixed`, `large`, `concurrent`, `error`, `range`.

### Output

- Console: Memory statistics, growth by class, pass/fail verdict
- `leak-results.txt`: Full results with memory samples
- `.leak-memory-samples.txt`: Raw timestamp/KB data for graphing

### Memory Sampling

The `get-memory.sh` script reads RSS via `ps -o rss=` on macOS and Linux. Memory values are in KB.

## Fuzz Testing

Fuzz tests are located at `test/web/fuzz/`. These are imported from the upstream `../web` repository and run manually.

### Architecture

The fuzz framework consists of a reusable library (`fuzz.h`/`fuzz.c`) and protocol-specific test files:

```
fuzz/
    fuzz.h               # Fuzzing library API
    fuzz.c               # Fuzzing library implementation
    http-proto.tst.c     # HTTP protocol fuzzer
    tls-proto.tst.c      # TLS/HTTPS protocol fuzzer
    url-path.tst.c       # URL path validation fuzzer
    corpus/              # Seed inputs
        http-requests.txt    # HTTP protocol examples
        tls-requests.txt     # HTTPS/TLS examples
        url-paths.txt        # URL path patterns
    crashes/             # Crash-inducing inputs (deduplicated)
        http/            # HTTP fuzzer crashes
        url/             # URL fuzzer crashes
        server/          # Server-side crashes with logs
```

### Fuzzing Library

The `FuzzRunner` orchestrates fuzzing campaigns:

```c
typedef struct FuzzRunner {
    FuzzConfig config;       // Duration, iterations, timeout, seed
    FuzzStats stats;         // Crashes, errors, hangs, coverage
    FuzzOracle oracle;       // Test function: returns true if passed
    FuzzMutator mutator;     // Mutation strategy callback
    RList *corpus;           // Seed corpus entries
    RHash *crashes;          // Crash deduplication via hash
} FuzzRunner;
```

Each fuzzer provides:
- An **oracle function** that sends a fuzzed input to the server and returns true/false
- A **mutator function** that applies protocol-aware mutations to corpus entries

### Mutation Strategies

The library provides 10 built-in mutation strategies:

| Strategy | Description |
|----------|-------------|
| `FUZZ_BIT_FLIP` | Flip random bits |
| `FUZZ_BYTE_FLIP` | Flip random bytes |
| `FUZZ_INSERT_RANDOM` | Insert random data at random positions |
| `FUZZ_DELETE_RANDOM` | Delete random bytes |
| `FUZZ_OVERWRITE_RANDOM` | Overwrite with random data |
| `FUZZ_INSERT_SPECIAL` | Insert special characters (null, CRLF, etc.) |
| `FUZZ_REPLACE_PATTERN` | Replace known patterns |
| `FUZZ_SPLICE` | Splice two inputs together |
| `FUZZ_DUPLICATE` | Duplicate data blocks |
| `FUZZ_TRUNCATE` | Truncate at random point |

Special characters used: `\x00\r\n\t "'<>&;|\`$(){}[]\/%`

Common attack patterns tested: path traversal, SQL injection, XSS, null bytes, JNDI injection, HTTP response splitting, URL encoding attacks.

### Implemented Fuzzers

**http-proto.tst.c** - Fuzzes HTTP request parsing: methods, URIs, version strings, and header formatting. Uses `corpus/http-requests.txt` as seed inputs.

**tls-proto.tst.c** - Fuzzes TLS/HTTPS connections including SNI validation. Uses `corpus/tls-requests.txt` as seeds.

**url-path.tst.c** - Fuzzes URL path validation and traversal attack detection. Uses `corpus/url-paths.txt` as seeds.

### Crash Detection

- **Client-side**: Signal handlers catch SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS
- **Server-side**: Monitors server PID liveness; reports crashes with the input that caused them
- **Deduplication**: Crashes are hashed to avoid duplicate reports
- **Crash files**: Saved with the triggering input and metadata for replay

### Configuration

```json5
{
    enable: 'manual',
    execution: { workers: 1, timeout: 36000 },  // 10 hours max
    services: {
        healthCheck: {
            url: 'http://localhost:4200',
            timeout: 120,       // 2 min for sanitizer builds
        },
    },
}
```

Optional sanitizer integration (ASAN/UBSAN) is configured via environment variables in `testme.json5`.

### Running Fuzz Tests

```bash
cd test/web/fuzz
tm http                                      # HTTP fuzzer (60s default)
tm --duration 300 tls                        # TLS fuzzer, 5 minutes
tm --duration 600 url                        # URL fuzzer, 10 minutes
FUZZ_REPLAY=crashes/http/crash-abc.txt tm http  # Replay a crash
TESTME_VERBOSE=1 tm http                     # Verbose output
TESTME_STOP=1 tm http                        # Stop on first crash
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `TESTME_DURATION` | Test duration in seconds |
| `TESTME_VERBOSE` | Enable verbose output |
| `TESTME_STOP` | Stop on first crash |
| `FUZZ_REPLAY` | Path to crash file to replay |
| `FUZZ_MUTATE` | Enable/disable mutation (0/1) |
| `FUZZ_RANDOMIZE` | Randomize corpus order (0/1) |

## Benchmark Testing

Performance benchmarks are at `test/web/bench/`. Marked `enable: 'manual'` with a 30-minute timeout.

```bash
cd test/web/bench
tm --duration 5 bench       # Quick smoke test
tm --duration 60 bench      # Standard benchmarks
tm --duration 300 bench     # Extended benchmarks
```

Measures static file serving (various sizes), file uploads, action routes, digest authentication, HTTPS/TLS performance, and raw protocol throughput. Results are written to `doc/benchmarks/`.

## Upstream Web Tests

The web server leak and fuzz tests in `test/web/leak/` and `test/web/fuzz/` are imported from the upstream `../web` repository. The upstream web module maintains the authoritative versions of these tests. When updating:

1. In the upstream web repo: make changes and run `make cache`
2. In the agent repo: `pak sync` to import updates
3. Rebuild with `make`

## Running Tests

### Full Suite
```bash
make test                          # All automated tests
```

### By Module
```bash
cd test && tm r                    # Safe Runtime
cd test && tm json                 # JSON
cd test && tm db                   # Database
cd test && tm url                  # HTTP client
cd test && tm web                  # Web server
```

### Individual Tests
```bash
cd test && tm r/mem                # Single test by name
cd test && tm web/auth-basic       # Single web test
cd test && tm "web/security-*"     # Pattern matching
```

### Manual Tests
```bash
cd test/web/leak && tm leak        # Memory leak detection
cd test/web/fuzz && tm http        # HTTP fuzzing
cd test/web/bench && tm bench      # Benchmarks
cd test/mqtt && tm                 # MQTT (requires broker)
```

### Duration Control
```bash
tm --duration 60 leak              # 60-second leak test
tm --duration 300 bench            # 5-minute benchmarks
```

## Ports

Each test subsystem uses dedicated ports to avoid conflicts:

| Test Suite | HTTP Port | HTTPS Port |
|------------|-----------|------------|
| Web unit tests | 4240 | 4241 |
| URL tests | (from web.json5) | (from web.json5) |
| Leak tests | 4250 | 4251 |
| Fuzz tests | 4200 | - |
| Benchmarks | 4260 | (from web.json5) |

## Test Execution Model

- **Sequential**: All tests run with `workers: 1` and `parallel: false`
- **Single-threaded**: Tests use the R fiber coroutine model, not OS threads
- **Isolated**: Each `.tst.c` compiles to a separate binary
- **Deterministic**: Sequential execution prevents race conditions
- **Self-contained**: Each test initializes and tears down its own state
